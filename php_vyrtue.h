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

ZEND_BEGIN_MODULE_GLOBALS(vyrtue)
    HashTable kind_hooks;
    HashTable function_hooks;
ZEND_END_MODULE_GLOBALS(vyrtue)

struct vyrtue_preprocess_context;
typedef zend_ast *(*vyrtue_ast_callback)(zend_ast *ast, struct vyrtue_preprocess_context *ctx);

VYRTUE_PUBLIC void vyrtue_register_kind_visitor(enum _zend_ast_kind kind, vyrtue_ast_callback enter, vyrtue_ast_callback leave);

VYRTUE_PUBLIC void vyrtue_register_function_visitor(zend_string *function_name, vyrtue_ast_callback enter, vyrtue_ast_callback leave);

ZEND_EXTERN_MODULE_GLOBALS(vyrtue);

#endif /* PHP_VYRTUE_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: fdm=marker
 * vim: et sw=4 ts=4
 */
