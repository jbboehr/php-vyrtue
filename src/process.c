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
#include "context.h"
#include "visitor.h"

static zend_ast *vyrtue_ast_walk(zend_ast *ast, struct vyrtue_context *ctx);

static void dump_ht(HashTable *ht)
{
    zval tmp = {0};
    ZVAL_ARR(&tmp, ht);
    php_var_dump(&tmp, 1);
}

#ifdef VYRTUE_DEBUG
static inline void vyrtue_ast_process_debug_dump_ast(zend_ast *ast)
{
    zend_string *str = zend_ast_export("", ast, "");
    fprintf(stderr, "%.*s", (int) str->len, str->val);
    fflush(stderr);
    zend_string_release(str);
}

static void vyrtue_ast_process_debug_replacement(zend_ast *before, zend_ast *after)
{
    if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_DUMP_REPLACEMENT"))) {
        fprintf(stderr, "BEFORE: ");
        fflush(stderr);
        vyrtue_ast_process_debug_dump_ast(before);
        fprintf(stderr, "AFTER: ");
        fflush(stderr);
        vyrtue_ast_process_debug_dump_ast(after);
    }
}
#else
#define vyrtue_ast_process_debug_dump_ast(ast)
#define vyrtue_ast_process_debug_replacement(before, after)
#endif

/**
 * @see zend_compile_namespace
 */
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_namespace_enter(zend_ast *ast, struct vyrtue_context *ctx)
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

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_namespace_leave(zend_ast *ast, struct vyrtue_context *ctx)
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
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_use_enter(zend_ast *ast, struct vyrtue_context *ctx)
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
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_group_use_enter(zend_ast *ast, struct vyrtue_context *ctx)
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
        zend_ast *replace = vyrtue_ast_process_use_enter(inline_use, ctx);

        if (UNEXPECTED(replace != NULL)) {
            zend_throw_exception(zend_ce_parse_error, "vyrtue visitor attempted to replace invalid AST node", 0);
            return NULL;
        }
    }

    ctx->in_group_use = true;

    return NULL;
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_group_use_leave(zend_ast *ast, struct vyrtue_context *ctx)
{
    ctx->in_group_use = false;

    return NULL;
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_enter_node(zend_ast *ast, const struct vyrtue_visitor_array *visitors, struct vyrtue_context *ctx)
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

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_leave_node(zend_ast *ast, const struct vyrtue_visitor_array *visitors, struct vyrtue_context *ctx)
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

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_call_enter(zend_ast *ast, struct vyrtue_context *ctx)
{
    zend_ast *name_ast = ast->child[0];

    // ignore dynamic calls
    if (name_ast->kind != ZEND_AST_ZVAL || Z_TYPE_P(zend_ast_get_zval(name_ast)) != IS_STRING) {
#ifdef VYRTUE_DEBUG
        if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_CALL"))) {
            php_error_docref(NULL, E_WARNING, "vyrtue: Dynamic function call");
        }
#endif
        return NULL;
    }

    bool is_fully_qualified;
    zend_string *name_str = vyrtue_resolve_function_name(Z_STR_P(zend_ast_get_zval(name_ast)), name_ast->attr, &is_fully_qualified, ctx);
    if (!is_fully_qualified) {
        // ignore unqualified function calls
#ifdef VYRTUE_DEBUG
        if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_CALL"))) {
            php_error_docref(NULL, E_WARNING, "vyrtue: Unqualified function call: %.*s", (int) name_str->len, name_str->val);
        }
#endif
        return NULL;
    }

    const struct vyrtue_visitor_array *visitors = vyrtue_get_function_visitors(name_str);

    return vyrtue_ast_enter_node(ast, visitors, ctx);
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_call_leave(zend_ast *ast, struct vyrtue_context *ctx)
{
    zend_ast *name_ast = ast->child[0];

    // ignore dynamic calls
    if (name_ast->kind != ZEND_AST_ZVAL || Z_TYPE_P(zend_ast_get_zval(name_ast)) != IS_STRING) {
#ifdef VYRTUE_DEBUG
        if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_CALL"))) {
            php_error_docref(NULL, E_WARNING, "vyrtue: Dynamic function call");
        }
#endif
        return NULL;
    }

    bool is_fully_qualified;
    zend_string *name_str = vyrtue_resolve_function_name(Z_STR_P(zend_ast_get_zval(name_ast)), name_ast->attr, &is_fully_qualified, ctx);
    if (!is_fully_qualified) {
        // ignore unqualified function calls
#ifdef VYRTUE_DEBUG
        if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_CALL"))) {
            php_error_docref(NULL, E_WARNING, "vyrtue: Unqualified function call: %.*s", (int) name_str->len, name_str->val);
        }
#endif
        return NULL;
    }

    const struct vyrtue_visitor_array *visitors = vyrtue_get_function_visitors(name_str);

    return vyrtue_ast_leave_node(ast, visitors, ctx);
}

VYRTUE_ATTR_NONNULL_ALL
static void vyrtue_ast_walk_list(zend_ast *ast, struct vyrtue_context *ctx)
{
    zend_ast_list *list = zend_ast_get_list(ast);

    for (int i = 0; i < list->children; i++) {
        zend_ast *child = list->child[i];
        if (UNEXPECTED(NULL == child)) {
            continue;
        }

        zend_ast *replace_child = vyrtue_ast_walk(child, ctx);

        if (UNEXPECTED(replace_child != NULL && replace_child != child)) {
            vyrtue_ast_process_debug_replacement(list->child[i], replace_child);
            zend_ast_destroy(child);
            list->child[i] = replace_child;
        }
    }
}

VYRTUE_ATTR_NONNULL_ALL
static void vyrtue_ast_walk_children(zend_ast *ast, struct vyrtue_context *ctx)
{
    uint32_t i, children = zend_ast_get_num_children(ast);
    for (i = 0; i < children; i++) {
        zend_ast *child = ast->child[i];
        if (UNEXPECTED(child == NULL)) {
            continue;
        }

        zend_ast *replace_child = vyrtue_ast_walk(child, ctx);

        if (UNEXPECTED(replace_child != NULL && replace_child != child)) {
            vyrtue_ast_process_debug_replacement(child, replace_child);
            zend_ast_destroy(child);
            ast->child[i] = replace_child;
        }
    }
}

VYRTUE_ATTR_NONNULL_ALL
static void vyrtue_ast_walk_recurse(zend_ast *ast, struct vyrtue_context *ctx)
{
    if (EXPECTED(zend_ast_is_list(ast))) {
        vyrtue_ast_walk_list(ast, ctx);
    } else {
        vyrtue_ast_walk_children(ast, ctx);
    }
}

VYRTUE_ATTR_NONNULL_ALL
static zend_ast *vyrtue_ast_process_attributes(zend_ast *ast, zend_ast *parent_ast, vyrtue_ast_enter_leave_fn fn, struct vyrtue_context *ctx)
{
    zend_ast_list *list = zend_ast_get_list(ast);
    uint32_t g, i, j;

    ZEND_ASSERT(ast->kind == ZEND_AST_ATTRIBUTE_LIST);

    for (g = 0; g < list->children; g++) {
        zend_ast_list *group = zend_ast_get_list(list->child[g]);

        ZEND_ASSERT(group->kind == ZEND_AST_ATTRIBUTE_GROUP);

        for (i = 0; i < group->children; i++) {
            ZEND_ASSERT(group->child[i]->kind == ZEND_AST_ATTRIBUTE);

            zend_ast *el = group->child[i];
            zend_string *name = vyrtue_resolve_class_name_ast(el->child[0], ctx);
            if (UNEXPECTED(name == NULL)) {
                continue;
            }

            const struct vyrtue_visitor_array *visitors = vyrtue_get_attribute_visitors(name);
            zend_ast *replace = fn(parent_ast, visitors, ctx);

            if (UNEXPECTED(replace != NULL)) {
                return replace;
            }
        }
    }

    return NULL;
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_class_enter(zend_ast *ast, struct vyrtue_context *ctx)
{
    zend_ast_decl *decl = (zend_ast_decl *) ast;

    if (decl->child[3]) {
        zend_ast *replace = vyrtue_ast_process_attributes(decl->child[3], ast, vyrtue_ast_enter_node, ctx);
        if (UNEXPECTED(replace != NULL)) {
            return replace;
        }
    }

    return NULL;
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_process_class_leave(zend_ast *ast, struct vyrtue_context *ctx)
{
    zend_ast_decl *decl = (zend_ast_decl *) ast;

    if (decl->child[3]) {
        zend_ast *replace = vyrtue_ast_process_attributes(decl->child[3], ast, vyrtue_ast_leave_node, ctx);
        if (UNEXPECTED(replace != NULL)) {
            return replace;
        }
    }

    return NULL;
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_ast *vyrtue_ast_walk(zend_ast *ast, struct vyrtue_context *ctx)
{
    zend_ast *replace;
    zend_ast_decl *decl;

    // this may be removed by compiler due to VYRTUE_ATTR_NONNULL_ALL
    ZEND_ASSERT(ast != NULL);

    const struct vyrtue_visitor_array *visitors = vyrtue_get_kind_visitors(ast->kind);

    replace = vyrtue_ast_enter_node(ast, visitors, ctx);
    if (replace) {
        return vyrtue_ast_walk(replace, ctx) ?: replace;
    }

    vyrtue_context_stack_push(&ctx->node_stack, ast);

    switch (ast->kind) {
        case ZEND_AST_FUNC_DECL:
        case ZEND_AST_CLOSURE:
        case ZEND_AST_METHOD:
        case ZEND_AST_ARROW_FUNC:
        case ZEND_AST_CLASS: {
            vyrtue_context_stack_push(&ctx->scope_stack, ast);

            decl = (zend_ast_decl *) ast;
            if (EXPECTED(decl->child[2])) {
                vyrtue_ast_walk_recurse(decl->child[2], ctx);
            }

            vyrtue_context_stack_pop(&ctx->scope_stack, ast);
            break;
        }
        default: {
            vyrtue_ast_walk_recurse(ast, ctx);
            break;
        }
    }

    vyrtue_context_stack_pop(&ctx->node_stack, ast);

    replace = vyrtue_ast_leave_node(ast, visitors, ctx);
    if (replace) {
        return vyrtue_ast_walk(replace, ctx) ?: replace;
    }

    return NULL;
}

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
void vyrtue_ast_process_file(zend_ast *ast)
{
    struct vyrtue_context ctx = {0};

    zend_ast *replace = vyrtue_ast_walk(ast, &ctx);
    if (replace != NULL) {
        zend_throw_exception(zend_ce_parse_error, "vyrtue visitor attempted to replace the root AST node which is unsupported", 0);
        return;
    }

#ifdef VYRTUE_DEBUG
    if (UNEXPECTED(NULL != getenv("PHP_VYRTUE_DEBUG_DUMP_AST"))) {
        zend_string *str = zend_ast_export("<?php\n", ast, "");
        fprintf(stderr, "%.*s", (int) str->len, str->val);
        zend_string_release(str);
    }
#endif

    vyrtue_end_namespace(&ctx);

    if (UNEXPECTED(vyrtue_context_stack_count(&ctx.scope_stack) > 0)) {
        zend_error(E_WARNING, "vyrtue: ast process ended with %lu items on the scope stack", vyrtue_context_stack_count(&ctx.scope_stack));
    }
    if (UNEXPECTED(vyrtue_context_stack_count(&ctx.node_stack) > 0)) {
        zend_error(E_WARNING, "vyrtue: ast process ended with %lu items on the node stack", vyrtue_context_stack_count(&ctx.node_stack));
    }
}

VYRTUE_LOCAL
PHP_MINIT_FUNCTION(vyrtue_process)
{
    vyrtue_register_kind_visitor("vyrtue internal", ZEND_AST_USE, vyrtue_ast_process_use_enter, NULL);
    vyrtue_register_kind_visitor("vyrtue internal", ZEND_AST_GROUP_USE, vyrtue_ast_process_group_use_enter, vyrtue_ast_process_group_use_leave);
    vyrtue_register_kind_visitor("vyrtue internal", ZEND_AST_NAMESPACE, vyrtue_ast_process_namespace_enter, vyrtue_ast_process_namespace_leave);
    vyrtue_register_kind_visitor("vyrtue internal", ZEND_AST_CALL, vyrtue_ast_process_call_enter, vyrtue_ast_process_call_leave);
    vyrtue_register_kind_visitor("vyrtue internal", ZEND_AST_CLASS, vyrtue_ast_process_class_enter, vyrtue_ast_process_class_leave);

    return SUCCESS;
}
