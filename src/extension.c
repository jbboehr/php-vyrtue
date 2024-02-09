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

#include "Zend/zend_API.h"
#include "Zend/zend_constants.h"
#include "Zend/zend_ini.h"
#include "Zend/zend_modules.h"
#include "Zend/zend_operators.h"
#include "main/php.h"
#include "main/php_ini.h"
#include "ext/standard/info.h"

#include "php_vyrtue.h"
#include "private.h"

ZEND_DECLARE_MODULE_GLOBALS(vyrtue);

static void (*original_ast_process)(zend_ast *ast) = NULL;

PHP_INI_BEGIN()
PHP_INI_END()

VYRTUE_PUBLIC
zend_never_inline void vyrtue_ast_process(zend_ast *ast)
{
    if (NULL != original_ast_process) {
        original_ast_process(ast);
    }

    vyrtue_ast_process_file(ast);
}

static PHP_RINIT_FUNCTION(vyrtue)
{
#if defined(COMPILE_DL_VYRTUE) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    return SUCCESS;
}

static PHP_MINIT_FUNCTION(vyrtue)
{
    int flags = CONST_CS | CONST_PERSISTENT;

    REGISTER_INI_ENTRIES();

    // Register constants
    REGISTER_STRING_CONSTANT("VyrtueExt\\VERSION", (char *) PHP_VYRTUE_VERSION, flags);
#ifdef VYRTUE_DEBUG
    REGISTER_BOOL_CONSTANT("VyrtueExt\\DEBUG", true, flags);
#else
    REGISTER_BOOL_CONSTANT("VyrtueExt\\DEBUG", false, flags);
#endif

    if (NULL == original_ast_process) {
        original_ast_process = zend_ast_process;
        zend_ast_process = vyrtue_ast_process;
    }

    PHP_MINIT(vyrtue_preprocess)(INIT_FUNC_ARGS_PASSTHRU);
#ifdef VYRTUE_DEBUG
    PHP_MINIT(vyrtue_debug)(INIT_FUNC_ARGS_PASSTHRU);
#endif

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(vyrtue)
{
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(vyrtue)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_VYRTUE_VERSION);
    php_info_print_table_row(2, "Released", PHP_VYRTUE_RELEASE);
    php_info_print_table_row(2, "Authors", PHP_VYRTUE_AUTHORS);

    DISPLAY_INI_ENTRIES();
}

static PHP_GINIT_FUNCTION(vyrtue)
{
#if defined(COMPILE_DL_VYRTUE) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    memset(vyrtue_globals, 0, sizeof(zend_vyrtue_globals));
    zend_hash_init(&vyrtue_globals->function_hooks, 16, NULL, NULL, 1);
    zend_hash_init(&vyrtue_globals->kind_hooks, 16, NULL, NULL, 1);
}

static PHP_GSHUTDOWN_FUNCTION(vyrtue)
{
    zend_hash_destroy(&vyrtue_globals->function_hooks);
    zend_hash_destroy(&vyrtue_globals->kind_hooks);
}

const zend_function_entry vyrtue_functions[] = {
#ifdef VYRTUE_DEBUG
#endif
    PHP_FE_END,
};

static const zend_module_dep vyrtue_deps[] = {
    {"ast",     NULL, NULL, MODULE_DEP_OPTIONAL},
    {"opcache", NULL, NULL, MODULE_DEP_OPTIONAL},
    ZEND_MOD_END,
};

zend_module_entry vyrtue_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    vyrtue_deps,                /* Deps */
    PHP_VYRTUE_NAME,            /* Name */
    vyrtue_functions,           /* Functions */
    PHP_MINIT(vyrtue),          /* MINIT */
    PHP_MSHUTDOWN(vyrtue),      /* MSHUTDOWN */
    PHP_RINIT(vyrtue),          /* RINIT */
    NULL,                       /* RSHUTDOWN */
    PHP_MINFO(vyrtue),          /* MINFO */
    PHP_VYRTUE_VERSION,         /* Version */
    PHP_MODULE_GLOBALS(vyrtue), /* Globals */
    PHP_GINIT(vyrtue),          /* GINIT */
    PHP_GSHUTDOWN(vyrtue),      /* GSHUTDOWN */
    NULL,
    STANDARD_MODULE_PROPERTIES_EX,
};

#ifdef COMPILE_DL_VYRTUE
#if defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(vyrtue) // Common for all PHP extensions which are build as shared modules
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: fdm=marker
 * vim: et sw=4 ts=4
 */
