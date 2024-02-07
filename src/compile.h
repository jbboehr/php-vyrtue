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

#include <Zend/zend_API.h>
#include <stdbool.h>

static bool zend_get_unqualified_name(const zend_string *name, const char **result, size_t *result_len)
{
    const char *ns_separator = zend_memrchr(ZSTR_VAL(name), '\\', ZSTR_LEN(name));
    if (ns_separator != NULL) {
        *result = ns_separator + 1;
        *result_len = ZSTR_VAL(name) + ZSTR_LEN(name) - *result;
        return 1;
    }

    return 0;
}

static char *zend_get_use_type_str(uint32_t type)
{
    switch (type) {
        case ZEND_SYMBOL_CLASS:
            return "";
        case ZEND_SYMBOL_FUNCTION:
            return " function";
        case ZEND_SYMBOL_CONST:
            return " const";
            EMPTY_SWITCH_DEFAULT_CASE()
    }

    return " unknown";
}

static void str_dtor(zval *zv)
{
    zend_string_release_ex(Z_STR_P(zv), 0);
}

static zend_string *zend_concat_names(char *name1, size_t name1_len, char *name2, size_t name2_len)
{
    return zend_string_concat3(name1, name1_len, "\\", 1, name2, name2_len);
}
