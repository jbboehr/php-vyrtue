/*
+----------------------------------------------------------------------+
| Zend Engine                                                          |
+----------------------------------------------------------------------+
| Copyright (c) Zend Technologies Ltd. (http://www.zend.com)           |
+----------------------------------------------------------------------+
| This source file is subject to version 2.00 of the Zend license,     |
| that is bundled with this package in the file LICENSE, and is        |
| available through the world-wide-web at the following url:           |
| http://www.zend.com/license/2_00.txt.                                |
| If you did not receive a copy of the Zend license and are unable to  |
| obtain it through the world-wide-web, please send a note to          |
| license@zend.com so we can mail you a copy immediately.              |
+----------------------------------------------------------------------+
| Authors: Andi Gutmans <andi@php.net>                                 |
|          Zeev Suraski <zeev@php.net>                                 |
|          Nikita Popov <nikic@php.net>                                |
+----------------------------------------------------------------------+
*/

// These are all mostly based on zend_compile functions, so I suppose
// they are probably covered under the zend license

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdbool.h>

#include "Zend/zend_API.h"
#include "Zend/zend_compile.h"
#include "main/php.h"
#include "main/php_streams.h"

#include "php_vyrtue.h"
#include "context.h"
#include "compile.h"

static void str_dtor(zval *zv)
{
    zend_string_release_ex(Z_STR_P(zv), 0);
}

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
HashTable *vyrtue_get_import_ht(uint32_t type, struct vyrtue_context *ctx)
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

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
void vyrtue_reset_import_tables(struct vyrtue_context *ctx)
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

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
void vyrtue_end_namespace(struct vyrtue_context *ctx)
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

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_string *vyrtue_prefix_with_ns(zend_string *name, struct vyrtue_context *ctx)
{
    if (ctx->current_namespace) {
        zend_string *ns = ctx->current_namespace;
        return zend_concat_names(ZSTR_VAL(ns), ZSTR_LEN(ns), ZSTR_VAL(name), ZSTR_LEN(name));
    } else {
        return zend_string_copy(name);
    }
}

VYRTUE_ATTR_NONNULL(1, 3, 6)
VYRTUE_ATTR_WARN_UNUSED_RESULT
static zend_string *vyrtue_resolve_non_class_name(
    zend_string *name, uint32_t type, bool *is_fully_qualified, bool case_sensitive, HashTable *current_import_sub, struct vyrtue_context *ctx
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

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
zend_string *vyrtue_resolve_function_name(zend_string *name, uint32_t type, bool *is_fully_qualified, struct vyrtue_context *ctx)
{
    return vyrtue_resolve_non_class_name(name, type, is_fully_qualified, 0, ctx->imports_function, ctx);
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
static uint32_t vyrtue_get_class_fetch_type(zend_string *name)
{
    if (zend_string_equals_literal_ci(name, "self")) {
        return ZEND_FETCH_CLASS_SELF;
    } else if (zend_string_equals_literal_ci(name, "parent")) {
        return ZEND_FETCH_CLASS_PARENT;
    } else if (zend_string_equals_literal_ci(name, "static")) {
        return ZEND_FETCH_CLASS_STATIC;
    } else {
        return ZEND_FETCH_CLASS_DEFAULT;
    }
}

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL(3)
VYRTUE_ATTR_WARN_UNUSED_RESULT
zend_string *vyrtue_resolve_class_name(zend_string *name, uint32_t type, struct vyrtue_context *ctx)
{
    char *compound;

    if (ZEND_FETCH_CLASS_DEFAULT != vyrtue_get_class_fetch_type(name)) {
        if (type == ZEND_NAME_FQ) {
            // zend_error_noreturn(E_COMPILE_ERROR, "'\\%s' is an invalid class name", ZSTR_VAL(name));
            return NULL;
        }
        if (type == ZEND_NAME_RELATIVE) {
            // zend_error_noreturn(E_COMPILE_ERROR, "'namespace\\%s' is an invalid class name", ZSTR_VAL(name));
            return NULL;
        }
        ZEND_ASSERT(type == ZEND_NAME_NOT_FQ);
        return zend_string_copy(name);
    }

    if (type == ZEND_NAME_RELATIVE) {
        return vyrtue_prefix_with_ns(name, ctx);
    }

    if (type == ZEND_NAME_FQ) {
        if (ZSTR_VAL(name)[0] == '\\') {
            /* Remove \ prefix (only relevant if this is a string rather than a label) */
            name = zend_string_init(ZSTR_VAL(name) + 1, ZSTR_LEN(name) - 1, 0);
            if (ZEND_FETCH_CLASS_DEFAULT != vyrtue_get_class_fetch_type(name)) {
                // zend_error_noreturn(E_COMPILE_ERROR, "'\\%s' is an invalid class name", ZSTR_VAL(name));
                return NULL;
            }
            return name;
        }

        return zend_string_copy(name);
    }

    if (ctx->imports) {
        compound = memchr(ZSTR_VAL(name), '\\', ZSTR_LEN(name));
        if (compound) {
            /* If the first part of a qualified name is an alias, substitute it. */
            size_t len = compound - ZSTR_VAL(name);
            zend_string *import_name = zend_hash_str_find_ptr_lc(ctx->imports, ZSTR_VAL(name), len);

            if (import_name) {
                return zend_concat_names(ZSTR_VAL(import_name), ZSTR_LEN(import_name), ZSTR_VAL(name) + len + 1, ZSTR_LEN(name) - len - 1);
            }
        } else {
            /* If an unqualified name is an alias, replace it. */
            zend_string *import_name = zend_hash_find_ptr_lc(ctx->imports, name);

            if (import_name) {
                return zend_string_copy(import_name);
            }
        }
    }

    /* If not fully qualified and not an alias, prepend the current namespace */
    return vyrtue_prefix_with_ns(name, ctx);
}

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
zend_string *vyrtue_resolve_class_name_ast(zend_ast *ast, struct vyrtue_context *ctx)
{
    zval *class_name = zend_ast_get_zval(ast);
    if (Z_TYPE_P(class_name) != IS_STRING) {
        // zend_error_noreturn(E_COMPILE_ERROR, "Illegal class name");
        return NULL;
    }
    return vyrtue_resolve_class_name(Z_STR_P(class_name), ast->attr, ctx);
}
