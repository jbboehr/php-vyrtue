/**
 * Copyright (C) 2024 John Boehr & contributors
 *
 * This file is part of php-vyrtue.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdbool.h>

#include "Zend/zend_API.h"
#include "Zend/zend_exceptions.h"
#include "main/php.h"
#include "main/php_streams.h"
#include "ext/standard/php_string.h"
#include "php_vyrtue.h"
#include "compile.h"

struct vyrtue_preprocess_context
{
    bool should_perform_second_pass;
    zend_string *current_namespace;
    HashTable *imports;
    HashTable *imports_function;
    HashTable *imports_const;
};

/**
 * @see zend_get_import_ht
 * @license Zend-2.0
 */
static HashTable *vyrtue_get_import_ht(uint32_t type, struct vyrtue_preprocess_context *ctx) /* {{{ */
{
    switch (type) {
        case ZEND_SYMBOL_CLASS:
            if (ctx->imports) {
                ctx->imports = emalloc(sizeof(HashTable));
                zend_hash_init(ctx->imports, 8, NULL, str_dtor, 0);
            }
            return ctx->imports;
        case ZEND_SYMBOL_FUNCTION:
            if (!ctx->imports_function) {
                ctx->imports_function = emalloc(sizeof(HashTable));
                zend_hash_init(ctx->imports_function, 8, NULL, str_dtor, 0);
            }
            return ctx->imports_function;
        case ZEND_SYMBOL_CONST:
            if (!ctx->imports_const) {
                ctx->imports_const = emalloc(sizeof(HashTable));
                zend_hash_init(ctx->imports_const, 8, NULL, str_dtor, 0);
            }
            return ctx->imports_const;
        default:
            return NULL;
    }
}

/**
 * @see zend_reset_import_tables
 * @license Zend-2.0
 */
static void vyrtue_reset_import_tables(struct vyrtue_preprocess_context *ctx)
{
    if (ctx->imports) {
        zend_hash_destroy(ctx->imports);
        efree(ctx->imports);
        ctx->imports = NULL;
    }

    if (ctx->imports_function) {
        zend_hash_destroy(ctx->imports_function);
        efree(ctx->imports_function);
        ctx->imports_function = NULL;
    }

    if (ctx->imports_const) {
        zend_hash_destroy(ctx->imports_const);
        efree(ctx->imports_const);
        ctx->imports_const = NULL;
    }
}

/**
 * @see zend_compile_namespace
 */
static void vyrtue_ast_first_pass_namespace(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *name_ast = ast->child[0];

    if (name_ast) {
        ctx->current_namespace = zend_ast_get_str(name_ast);
    } else {
        ctx->current_namespace = NULL;
    }
}

/**
 * @see zend_compile_use
 */
static void vyrtue_ast_first_pass_use(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast_list *list = zend_ast_get_list(ast);
    uint32_t i;
    zend_string *current_ns = ctx->current_namespace;
    uint32_t type = ast->attr;
    HashTable *current_import = vyrtue_get_import_ht(type, ctx);
    bool case_sensitive = type == ZEND_SYMBOL_CONST;

    for (i = 0; i < list->children; ++i) {
        zend_ast *use_ast = list->child[i];
        zend_ast *old_name_ast = use_ast->child[0];
        zend_ast *new_name_ast = use_ast->child[1];
        zend_string *old_name = zend_ast_get_str(old_name_ast);
        zend_string *new_name, *lookup_name;

        if (new_name_ast) {
            new_name = zend_string_copy(zend_ast_get_str(new_name_ast));
        } else {
            const char *unqualified_name;
            size_t unqualified_name_len;
            if (zend_get_unqualified_name(old_name, &unqualified_name, &unqualified_name_len)) {
                new_name = zend_string_init(unqualified_name, unqualified_name_len, 0);
            } else {
                new_name = zend_string_copy(old_name);
            }
        }

        if (case_sensitive) {
            lookup_name = zend_string_copy(new_name);
        } else {
            lookup_name = zend_string_tolower(new_name);
        }

        if (current_ns) {
            zend_string *ns_name = zend_string_alloc(ZSTR_LEN(current_ns) + 1 + ZSTR_LEN(new_name), 0);
            zend_str_tolower_copy(ZSTR_VAL(ns_name), ZSTR_VAL(current_ns), ZSTR_LEN(current_ns));
            ZSTR_VAL(ns_name)[ZSTR_LEN(current_ns)] = '\\';
            memcpy(ZSTR_VAL(ns_name) + ZSTR_LEN(current_ns) + 1, ZSTR_VAL(lookup_name), ZSTR_LEN(lookup_name) + 1);

            //            if (zend_have_seen_symbol(ns_name, type)) {
            //                zend_check_already_in_use(type, old_name, new_name, ns_name);
            //            }

            zend_string_efree(ns_name);
        } /* else if (zend_have_seen_symbol(lookup_name, type)) {
            zend_check_already_in_use(type, old_name, new_name, lookup_name);
        } */

        zend_string_addref(old_name);
        old_name = zend_new_interned_string(old_name);
        zend_hash_add_ptr(current_import, lookup_name, old_name);

        // @todo we could potentially check for registered macros here

        zend_string_release_ex(lookup_name, 0);
        zend_string_release_ex(new_name, 0);
    }
}

static void vyrtue_ast_first_pass(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    if (ast->kind != ZEND_AST_STMT_LIST) {
        return;
    }

    zend_ast_list *list = zend_ast_get_list(ast);

    for (int i = 0; i < list->children; i++) {
        zend_ast *child = list->child[i];
        if (UNEXPECTED(NULL == child)) {
            continue;
        }

        //        fprintf(stderr, "|%d %d %d| \n", (int) child->kind, ZEND_AST_NAMESPACE, ZEND_AST_USE);

        switch (child->kind) {
            case ZEND_AST_NAMESPACE:
                vyrtue_ast_first_pass_namespace(child, ctx);
                break;
            case ZEND_AST_USE:
                vyrtue_ast_first_pass_use(child, ctx);
                break;
        }
    }
}

static void vyrtue_ast_second_pass(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    // todo
}

VYRTUE_PUBLIC void vyrtue_ast_process_file(zend_ast *ast)
{
    struct vyrtue_preprocess_context ctx = {
        .should_perform_second_pass = false,
    };

    vyrtue_ast_first_pass(ast, &ctx);

    if (ctx.should_perform_second_pass) {
        vyrtue_ast_second_pass(ast, &ctx);
    }

#ifdef VYRTUE_DEBUG
    if (NULL != getenv("PHP_VYRTUE_DEBUG_DUMP")) {
        zend_string *str = zend_ast_export("<?php\n", ast, "");
        fprintf(stderr, "%.*s", str->len, str->val);
    }
#endif

    vyrtue_reset_import_tables(&ctx);
}
