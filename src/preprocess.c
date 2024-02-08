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
#include "ext/standard/php_var.h"
#include "php_vyrtue.h"
#include "compile.h"
#include "private.h"

struct vyrtue_preprocess_context
{
    bool in_namespace;
    bool in_group_use;
    zend_string *current_namespace;
    HashTable *imports;
    HashTable *imports_function;
    HashTable *imports_const;
    HashTable *seen_symbols;
};

static zend_ast *vyrtue_ast_walk(zend_ast *ast, struct vyrtue_preprocess_context *ctx);

static void dump_ht(HashTable *ht)
{
    zval tmp = {0};
    ZVAL_ARR(&tmp, ht);
    php_var_dump(&tmp, 1);
}

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
            fprintf(stderr, "VYRTUE_NAMESPACE: LEFT: %.*s\n", (int) ctx->current_namespace->len, ctx->current_namespace->val);
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
 * @see zend_prefix_with_ns
 */
static zend_string *vyrtue_prefix_with_ns(zend_string *name, struct vyrtue_preprocess_context *ctx)
{
    if (ctx->current_namespace) {
        zend_string *ns = ctx->current_namespace;
        return zend_concat_names(ZSTR_VAL(ns), ZSTR_LEN(ns), ZSTR_VAL(name), ZSTR_LEN(name));
    } else {
        return zend_string_copy(name);
    }
}

/**
 * @see zend_resolve_non_class_name
 */
static zend_string *vyrtue_resolve_non_class_name(
    zend_string *name, uint32_t type, bool *is_fully_qualified, bool case_sensitive, HashTable *current_import_sub, struct vyrtue_preprocess_context *ctx
)
{
    char *compound;
    *is_fully_qualified = 0;

    if (ZSTR_VAL(name)[0] == '\\') {
        /* Remove \ prefix (only relevant if this is a string rather than a label) */
        *is_fully_qualified = 1;
        return zend_string_init(ZSTR_VAL(name) + 1, ZSTR_LEN(name) - 1, 0);
    }

    if (type == ZEND_NAME_FQ) {
        *is_fully_qualified = 1;
        return zend_string_copy(name);
    }

    if (type == ZEND_NAME_RELATIVE) {
        *is_fully_qualified = 1;
        return vyrtue_prefix_with_ns(name, ctx);
    }

    if (current_import_sub) {
        /* If an unqualified name is a function/const alias, replace it. */
        zend_string *import_name;
        if (case_sensitive) {
            import_name = zend_hash_find_ptr(current_import_sub, name);
        } else {
            import_name = zend_hash_find_ptr_lc(current_import_sub, name);
        }

        if (import_name) {
            *is_fully_qualified = 1;
            return zend_string_copy(import_name);
        }
    }

    compound = memchr(ZSTR_VAL(name), '\\', ZSTR_LEN(name));
    if (compound) {
        *is_fully_qualified = 1;
    }

    if (compound && ctx->imports) {
        /* If the first part of a qualified name is an alias, substitute it. */
        size_t len = compound - ZSTR_VAL(name);
        zend_string *import_name = zend_hash_str_find_ptr_lc(ctx->imports, ZSTR_VAL(name), len);

        if (import_name) {
            return zend_concat_names(ZSTR_VAL(import_name), ZSTR_LEN(import_name), ZSTR_VAL(name) + len + 1, ZSTR_LEN(name) - len - 1);
        }
    }

    return vyrtue_prefix_with_ns(name, ctx);
}

/**
 * @see zend_resolve_function_name
 */
static zend_string *vyrtue_resolve_function_name(zend_string *name, uint32_t type, bool *is_fully_qualified, struct vyrtue_preprocess_context *ctx)
{
    return vyrtue_resolve_non_class_name(name, type, is_fully_qualified, 0, ctx->imports_function, ctx);
}

/**
 * @see zend_compile_namespace
 */
static zend_ast *vyrtue_ast_process_namespace_enter(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
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
        fprintf(stderr, "VYRTUE_NAMESPACE: ENTER: %.*s\n", (int) ctx->current_namespace->len, ctx->current_namespace->val);
    }
#endif

    return NULL;
}

static zend_ast *vyrtue_ast_process_namespace_leave(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *stmt_ast = ast->child[1];

    if (stmt_ast) {
        vyrtue_end_namespace(ctx);
    }

    return NULL;
}

/**
 * @see zend_compile_use
 */
static zend_ast *vyrtue_ast_process_use_enter(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast_list *list = zend_ast_get_list(ast);
    uint32_t i;
    zend_string *current_ns = ctx->current_namespace;
    uint32_t type = ast->attr;
    HashTable *current_import = vyrtue_get_import_ht(type, ctx);
    bool case_sensitive = type == ZEND_SYMBOL_CONST;

    if (ctx->in_group_use) {
        return NULL;
    }

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
            fprintf(stderr, "VYRTUE_USE: %.*s => %.*s\n", (int) old_name->len, old_name->val, (int) new_name->len, new_name->val);
        }
#endif

        zend_string_release_ex(lookup_name, 0);
        zend_string_release_ex(new_name, 0);
    }

    return NULL;
}

/**
 * @see zend_compile_group_use
 */
static zend_ast *vyrtue_ast_process_group_use_enter(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
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
        vyrtue_ast_process_use_enter(inline_use, ctx);
    }

    ctx->in_group_use = true;

    return NULL;
}

static zend_ast *vyrtue_ast_process_group_use_leave(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    ctx->in_group_use = false;

    return NULL;
}

static zend_ast *vyrtue_ast_enter_node(zend_ast *ast, const struct vyrtue_visitor_array *visitors, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *rv = NULL;

    for (size_t i = 0; i < visitors->length; i++) {
        if (visitors->data[i].enter) {
            rv = visitors->data[i].enter(ast, ctx);
            if (rv && ast != rv) {
                // We can't guarantee the same kind of node will be returned ...
                return rv;
            }
        }
    }

    return NULL;
}

static zend_ast *vyrtue_ast_leave_node(zend_ast *ast, const struct vyrtue_visitor_array *visitors, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *rv = NULL;

    for (size_t i = visitors->length; i-- > 0;) {
        if (visitors->data[i].leave) {
            rv = visitors->data[i].leave(ast, ctx);
            if (rv && ast != rv) {
                // We can't guarantee the same kind of node will be returned ...
                return rv;
            }
        }
    }

    return NULL;
}

static zend_ast *vyrtue_ast_process_call_enter(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *name_ast = ast->child[0];

    // ignore dynamic calls
    if (name_ast->kind != ZEND_AST_ZVAL || Z_TYPE_P(zend_ast_get_zval(name_ast)) != IS_STRING) {
        return NULL;
    }

    bool is_fully_qualified;
    zend_string *name_str = vyrtue_resolve_function_name(Z_STR_P(zend_ast_get_zval(name_ast)), name_ast->attr, &is_fully_qualified, ctx);
    if (!is_fully_qualified) {
        // ignore unqualified function calls
        // php_error_docref(NULL, E_WARNING, "Unqualified function call: %.*s", (int) name_str->len, name_str->val);
        return NULL;
    }

    const struct vyrtue_visitor_array *visitors = vyrtue_get_function_visitors(name_str);

    return vyrtue_ast_enter_node(ast, visitors, ctx);
}

static zend_ast *vyrtue_ast_process_call_leave(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *name_ast = ast->child[0];

    // ignore dynamic calls
    if (name_ast->kind != ZEND_AST_ZVAL || Z_TYPE_P(zend_ast_get_zval(name_ast)) != IS_STRING) {
        return NULL;
    }

    bool is_fully_qualified;
    zend_string *name_str = vyrtue_resolve_function_name(Z_STR_P(zend_ast_get_zval(name_ast)), name_ast->attr, &is_fully_qualified, ctx);
    if (!is_fully_qualified) {
        // ignore unqualified function calls
        // php_error_docref(NULL, E_WARNING, "Unqualified function call: %.*s", (int) name_str->len, name_str->val);
        return NULL;
    }

    const struct vyrtue_visitor_array *visitors = vyrtue_get_function_visitors(name_str);

    return vyrtue_ast_leave_node(ast, visitors, ctx);
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

        if (UNEXPECTED(replace_child != NULL && replace_child != child)) {
            zend_ast_destroy(list->child[i]);
            list->child[i] = replace_child;
        }
    }
}

static void vyrtue_ast_walk_children(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    uint32_t i, children = zend_ast_get_num_children(ast);
    for (i = 0; i < children; i++) {
        zend_ast *child = ast->child[i];
        if (UNEXPECTED(child == NULL)) {
            continue;
        }

        zend_ast *replace_child = vyrtue_ast_walk(child, ctx);

        if (UNEXPECTED(replace_child != NULL && replace_child != child)) {
            zend_ast_destroy(ast->child[i]);
            ast->child[i] = replace_child;
        }
    }
}

static zend_ast *vyrtue_ast_walk(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    zend_ast *replace;
    zend_ast_decl *decl;

    if (NULL == ast) {
        return NULL;
    }

    const struct vyrtue_visitor_array *visitors = vyrtue_get_kind_visitors(ast->kind);

    replace = vyrtue_ast_enter_node(ast, visitors, ctx);
    if (replace) {
        return vyrtue_ast_walk(replace, ctx) ?: replace;
    }

    switch (ast->kind) {
        case ZEND_AST_FUNC_DECL:
        case ZEND_AST_CLOSURE:
        case ZEND_AST_METHOD:
        case ZEND_AST_ARROW_FUNC:
        case ZEND_AST_CLASS: {
            decl = (zend_ast_decl *) ast;
            if (EXPECTED(decl->child[2])) {
                if (EXPECTED(zend_ast_is_list(decl->child[2]))) {
                    vyrtue_ast_walk_list(decl->child[2], ctx);
                } else {
                    vyrtue_ast_walk_children(decl->child[2], ctx);
                }
            }
            break;
        }
    }

    if (zend_ast_is_list(ast)) {
        vyrtue_ast_walk_list(ast, ctx);
    } else {
        vyrtue_ast_walk_children(ast, ctx);
    }

    replace = vyrtue_ast_leave_node(ast, visitors, ctx);
    if (replace) {
        return vyrtue_ast_walk(replace, ctx) ?: replace;
    }

    return NULL;
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
    efree(ctx.seen_symbols);
}

VYRTUE_LOCAL PHP_MINIT_FUNCTION(vyrtue_preprocess)
{
    vyrtue_register_kind_visitor(ZEND_AST_USE, vyrtue_ast_process_use_enter, NULL);
    vyrtue_register_kind_visitor(ZEND_AST_GROUP_USE, vyrtue_ast_process_group_use_enter, vyrtue_ast_process_group_use_leave);
    vyrtue_register_kind_visitor(ZEND_AST_NAMESPACE, vyrtue_ast_process_namespace_enter, vyrtue_ast_process_namespace_leave);
    vyrtue_register_kind_visitor(ZEND_AST_CALL, vyrtue_ast_process_call_enter, vyrtue_ast_process_call_leave);

    return SUCCESS;
}
