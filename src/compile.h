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

#include <stdbool.h>
#include <Zend/zend_API.h>

VYRTUE_LOCAL
bool zend_get_unqualified_name(const zend_string *name, const char **result, size_t *result_len);

static zend_string *zend_concat_names(char *name1, size_t name1_len, char *name2, size_t name2_len)
{
    return zend_string_concat3(name1, name1_len, "\\", 1, name2, name2_len);
}

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
HashTable *vyrtue_get_import_ht(uint32_t type, struct vyrtue_context *ctx);

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
void vyrtue_reset_import_tables(struct vyrtue_context *ctx);

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
void vyrtue_end_namespace(struct vyrtue_context *ctx);

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
zend_string *vyrtue_resolve_function_name(zend_string *name, uint32_t type, bool *is_fully_qualified, struct vyrtue_context *ctx);

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL(3)
VYRTUE_ATTR_WARN_UNUSED_RESULT
zend_string *vyrtue_resolve_class_name(zend_string *name, uint32_t type, struct vyrtue_context *ctx);

VYRTUE_LOCAL
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_WARN_UNUSED_RESULT
zend_string *vyrtue_resolve_class_name_ast(zend_ast *ast, struct vyrtue_context *ctx);
