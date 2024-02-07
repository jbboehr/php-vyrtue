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
#include "php_vyrtue.h"
#include "compile.h"

struct vyrtue_preprocess_context
{
    bool in_namespace;
    zend_string *current_namespace;
    HashTable *imports;
    HashTable *imports_function;
    HashTable *imports_const;
    HashTable *seen_symbols;
};

static zend_ast *vyrtue_ast_walk(zend_ast *ast, struct vyrtue_preprocess_context *ctx);

/**
 * @see zend_get_import_ht
 * @license Zend-2.0
 */
static HashTable *vyrtue_get_import_ht(uint32_t type, struct vyrtue_preprocess_context *ctx)
{
    switch (type) {
        case ZEND_SYMBOL_CLASS:
            if (!ctx->imports) {
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

static void vyrtue_end_namespace(struct vyrtue_preprocess_context *ctx)
{
    ctx->in_namespace = false;
    vyrtue_reset_import_tables(ctx);
    if (ctx->current_namespace) {
#ifdef VYRTUE_DEBUG
        if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_DUMP_NAMESPACE"))) {
            fprintf(
                stderr, "VYRTUE_NAMESPACE: LEFT: %.*s\n", (int) ctx->current_namespace->len, ctx->current_namespace->val
            );
        }
#endif
        ctx->current_namespace = NULL;
    }
}

static void vyrtue_register_seen_symbol(zend_string *name, uint32_t kind, struct vyrtue_preprocess_context *ctx)
{
    zval *zv = zend_hash_find(ctx->seen_symbols, name);
    if (zv) {
        Z_LVAL_P(zv) |= kind;
    } else {
        zval tmp;
        ZVAL_LONG(&tmp, kind);
        zend_hash_add_new(ctx->seen_symbols, name, &tmp);
    }
}

static bool vyrtue_have_seen_symbol(zend_string *name, uint32_t kind, struct vyrtue_preprocess_context *ctx)
{
    zval *zv = zend_hash_find(ctx->seen_symbols, name);
    return zv && (Z_LVAL_P(zv) & kind) != 0;
}

/**
 * @see zend_compile_namespace
 */
static void vyrtue_ast_process_namespace(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *name_ast = ast->child[0];
    zend_ast *stmt_ast = ast->child[1];
    bool with_bracket = stmt_ast != NULL;

    // there is a bunch of validation here in zend_compile_namespace that we *might* not need

    if (name_ast) {
        ctx->current_namespace = zend_ast_get_str(name_ast);
    } else {
        ctx->current_namespace = NULL;
    }

    vyrtue_reset_import_tables(ctx);

    ctx->in_namespace = true;

    if (with_bracket) {
        // FC(has_bracketed_namespaces) = 1;
    }

#ifdef VYRTUE_DEBUG
    if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_DUMP_NAMESPACE"))) {
        fprintf(
            stderr, "VYRTUE_NAMESPACE: ENTER: %.*s\n", (int) ctx->current_namespace->len, ctx->current_namespace->val
        );
    }
#endif

    if (stmt_ast) {
        zend_ast *replace_child = vyrtue_ast_walk(stmt_ast, ctx);
        ZEND_ASSERT(replace_child == NULL);
        vyrtue_end_namespace(ctx);
    }
}

/**
 * @see zend_compile_use
 */
static void vyrtue_ast_process_use(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
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
            zend_string_efree(ns_name);
        }

        zend_string_addref(old_name);
        old_name = zend_new_interned_string(old_name);

        if (!zend_hash_add_ptr(current_import, lookup_name, old_name)) {
            // symbol was imported twice - let zend_compile handle the error
        }

#ifdef VYRTUE_DEBUG
        if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_DUMP_USE"))) {
            fprintf(
                stderr,
                "VYRTUE_USE: %.*s => %.*s\n",
                (int) old_name->len,
                old_name->val,
                (int) new_name->len,
                new_name->val
            );
        }
#endif

        zend_string_release_ex(lookup_name, 0);
        zend_string_release_ex(new_name, 0);
    }
}

/**
 * @see zend_compile_group_use
 */
static void vyrtue_ast_process_group_use(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    uint32_t i;
    zend_string *ns = zend_ast_get_str(ast->child[0]);
    zend_ast_list *list = zend_ast_get_list(ast->child[1]);

    for (i = 0; i < list->children; i++) {
        zend_ast *inline_use, *use = list->child[i];
        zval *name_zval = zend_ast_get_zval(use->child[0]);
        zend_string *name = Z_STR_P(name_zval);
        zend_string *compound_ns = zend_concat_names(ZSTR_VAL(ns), ZSTR_LEN(ns), ZSTR_VAL(name), ZSTR_LEN(name));
        zend_string_release_ex(name, 0);
        ZVAL_STR(name_zval, compound_ns);
        inline_use = zend_ast_create_list(1, ZEND_AST_USE, use);
        inline_use->attr = ast->attr ? ast->attr : use->attr;
        vyrtue_ast_process_use(inline_use, ctx);
    }
}

static zend_ast *vyrtue_ast_enter_node(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    return NULL;
}

static zend_ast *vyrtue_ast_leave_node(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    return NULL;
}

static void vyrtue_ast_walk_list(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast_list *list = zend_ast_get_list(ast);

    for (int i = 0; i < list->children; i++) {
        zend_ast *child = list->child[i];
        if (UNEXPECTED(NULL == child)) {
            continue;
        }

        zend_ast *replace_child = vyrtue_ast_walk(child, ctx);

        if (UNEXPECTED(replace_child != NULL)) {
            zend_ast_destroy(list->child[i]);
            list->child[i] = replace_child;
        }
    }
}

static void vyrtue_ast_walk_children(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    uint32_t i, children = zend_ast_get_num_children(ast);
    for (i = 0; i < children; i++) {
        if (UNEXPECTED(ast->child[i] == NULL)) {
            continue;
        }

        zend_ast *replace_child = vyrtue_ast_walk(ast->child[i], ctx);

        if (UNEXPECTED(replace_child != NULL)) {
            zend_ast_destroy(ast->child[i]);
            ast->child[i] = replace_child;
        }
    }
}

static zend_ast *vyrtue_ast_walk(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *replace_child;

    if (NULL == ast) {
        return NULL;
    }

    // these have special handling
    switch (ast->kind) {
        case ZEND_AST_NAMESPACE:
            vyrtue_ast_process_namespace(ast, ctx);
            return NULL;

        case ZEND_AST_USE:
            vyrtue_ast_process_use(ast, ctx);
            return NULL;

        case ZEND_AST_GROUP_USE:
            vyrtue_ast_process_group_use(ast, ctx);
            return NULL;
    }

    replace_child = vyrtue_ast_enter_node(ast, ctx);
    if (replace_child) {
        // @TODO should we recurse on the replacement (also, below for leave node)?
        return replace_child;
    }

    if (zend_ast_is_list(ast)) {
        vyrtue_ast_walk_list(ast, ctx);
    } else {
        vyrtue_ast_walk_children(ast, ctx);
    }

    return vyrtue_ast_leave_node(ast, ctx);
}

VYRTUE_PUBLIC void vyrtue_ast_process_file(zend_ast *ast)
{
    struct vyrtue_preprocess_context ctx = {0};

    ctx.seen_symbols = emalloc(sizeof(HashTable));
    zend_hash_init(ctx.seen_symbols, 8, NULL, NULL, 0);

    zend_ast *replace_child = vyrtue_ast_walk(ast, &ctx);
    ZEND_ASSERT(replace_child == NULL);

#ifdef VYRTUE_DEBUG
    if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_DUMP_AST"))) {
        zend_string *str = zend_ast_export("<?php\n", ast, "");
        fprintf(stderr, "%.*s", (int) str->len, str->val);
        zend_string_release(str);
    }
#endif

    vyrtue_end_namespace(&ctx);
    zend_hash_destroy(ctx.seen_symbols);
}
