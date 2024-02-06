
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

ZEND_DECLARE_MODULE_GLOBALS(vyrtue);

static void (*original_ast_process)(zend_ast *ast) = NULL;

void vyrtue_ast_process_file(zend_ast *ast);

PHP_INI_BEGIN()
PHP_INI_END()

VYRTUE_PUBLIC zend_never_inline void vyrtue_ast_process(zend_ast *ast)
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

    if (NULL == original_ast_process) {
        original_ast_process = zend_ast_process;
        zend_ast_process = vyrtue_ast_process;
    }

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
    NULL,
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
