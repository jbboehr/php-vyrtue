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

#ifndef PHP_VYRTUE_H
#define PHP_VYRTUE_H

#include <stdbool.h>

#include "main/php.h"

#define PHP_VYRTUE_NAME "vyrtue"
#define PHP_VYRTUE_VERSION "0.1.0"
#define PHP_VYRTUE_RELEASE "2024-01-27"
#define PHP_VYRTUE_AUTHORS "John Boehr <jbboehr@gmail.com> (lead)"

#if (__GNUC__ >= 4) || defined(__clang__) || defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
#define VYRTUE_PUBLIC __attribute__((visibility("default")))
#define VYRTUE_LOCAL __attribute__((visibility("hidden")))
#elif defined(PHP_WIN32) && defined(VYRTUE_EXPORTS)
#define VYRTUE_PUBLIC __declspec(dllexport)
#define VYRTUE_LOCAL
#else
#define VYRTUE_PUBLIC
#define VYRTUE_LOCAL
#endif

#if (__GNUC__ >= 3) || defined(__clang__)
#define VYRTUE_ATTR_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define VYRTUE_ATTR_NONNULL_ALL __attribute__((nonnull))
#define VYRTUE_ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define VYRTUE_ATTR_NONNULL(...)
#define VYRTUE_ATTR_NONNULL_ALL
#define VYRTUE_ATTR_WARN_UNUSED_RESULT
#endif

#if ((__GNUC__ >= 5) || ((__GNUC__ >= 4) && (__GNUC_MINOR__ >= 9))) || defined(__clang__)
#define VYRTUE_ATTR_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define VYRTUE_ATTR_RETURNS_NONNULL
#endif

extern zend_module_entry vyrtue_module_entry;
#define phpext_vyrtue_ptr &vyrtue_module_entry

#if defined(ZTS) && ZTS
#include "TSRM.h"
#endif

#if defined(ZTS) && ZTS
#define VYRTUE_G(v) TSRMG(vyrtue_globals_id, zend_vyrtue_globals *, v)
#else
#define VYRTUE_G(v) (vyrtue_globals.v)
#endif

#if defined(ZTS) && defined(COMPILE_DL_VYRTUE)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

struct vyrtue_context;
typedef zend_ast *(*vyrtue_ast_callback)(zend_ast *ast, struct vyrtue_context *ctx);

ZEND_BEGIN_MODULE_GLOBALS(vyrtue)
    HashTable attribute_visitors;
    HashTable function_visitors;
    HashTable kind_visitors;
ZEND_END_MODULE_GLOBALS(vyrtue)

ZEND_EXTERN_MODULE_GLOBALS(vyrtue);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
zend_ast *vyrtue_context_node_stack_top(struct vyrtue_context *ctx);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
zend_ast *vyrtue_context_scope_stack_top_ast(struct vyrtue_context *ctx);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
HashTable *vyrtue_context_scope_stack_top_ht(struct vyrtue_context *ctx);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_RETURNS_NONNULL
zend_arena *vyrtue_context_get_arena(struct vyrtue_context *ctx);

VYRTUE_PUBLIC
zend_never_inline void vyrtue_ast_process(zend_ast *ast);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
void vyrtue_ast_process_file(zend_ast *ast);

VYRTUE_PUBLIC
void vyrtue_register_attribute_visitor(const char *visitor_name, zend_string *attribute_name, vyrtue_ast_callback enter, vyrtue_ast_callback leave);

VYRTUE_PUBLIC
void vyrtue_register_function_visitor(const char *visitor_name, zend_string *function_name, vyrtue_ast_callback enter, vyrtue_ast_callback leave);

VYRTUE_PUBLIC
void vyrtue_register_kind_visitor(const char *visitor_name, enum _zend_ast_kind kind, vyrtue_ast_callback enter, vyrtue_ast_callback leave);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_RETURNS_NONNULL
VYRTUE_ATTR_WARN_UNUSED_RESULT
const struct vyrtue_visitor_array *vyrtue_get_attribute_visitors(zend_string *attribute_name);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_RETURNS_NONNULL
VYRTUE_ATTR_WARN_UNUSED_RESULT
const struct vyrtue_visitor_array *vyrtue_get_function_visitors(zend_string *function_name);

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_RETURNS_NONNULL
VYRTUE_ATTR_WARN_UNUSED_RESULT
const struct vyrtue_visitor_array *vyrtue_get_kind_visitors(enum _zend_ast_kind kind);

// backwards compatibility
#define vyrtue_preprocess_context vyrtue_context

#endif /* PHP_VYRTUE_H */
