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
#include "private.h"

static const struct vyrtue_visitor_array EMPTY_VISITOR_ARRAY = {
    .size = 0,
    .length = 0,
};

VYRTUE_PUBLIC void vyrtue_register_function_visitor(zend_string *function_name, vyrtue_ast_callback enter, vyrtue_ast_callback leave)
{
    struct vyrtue_visitor visitor = {
        .enter = enter,
        .leave = leave,
    };

    struct vyrtue_visitor_array *arr = zend_hash_find_ptr(&VYRTUE_G(function_hooks), function_name);
    if (!arr) {
        size_t size = sizeof(*arr) + sizeof(arr->data[0]);
        arr = pemalloc(size, 1);
        memset(arr, 0, size);
        arr->size = size;
    } else {
        size_t size = sizeof(*arr) + sizeof(arr->data[0]) * arr->length + 1;
        arr = perealloc(arr, size, 1);
        arr->size = size;
    }

    arr->data[arr->length] = visitor;
    arr->length++;

    zend_hash_update_ptr(&VYRTUE_G(function_hooks), function_name, arr);
}

VYRTUE_PUBLIC
void vyrtue_register_kind_visitor(enum _zend_ast_kind kind, vyrtue_ast_callback enter, vyrtue_ast_callback leave)
{
    struct vyrtue_visitor visitor = {
        .enter = enter,
        .leave = leave,
    };

    struct vyrtue_visitor_array *arr = zend_hash_index_find_ptr(&VYRTUE_G(kind_hooks), (zend_ulong) kind);
    if (!arr) {
        size_t size = sizeof(*arr) + sizeof(arr->data[0]);
        arr = pemalloc(size, 1);
        memset(arr, 0, size);
        arr->size = size;
    } else {
        size_t size = sizeof(*arr) + sizeof(arr->data[0]) * arr->length + 1;
        arr = perealloc(arr, size, 1);
        arr->size = size;
    }

    arr->data[arr->length] = visitor;
    arr->length++;

    zend_hash_index_update_ptr(&VYRTUE_G(kind_hooks), (zend_ulong) kind, arr);
}

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_RETURNS_NONNULL
VYRTUE_ATTR_WARN_UNUSED_RESULT
const struct vyrtue_visitor_array *vyrtue_get_function_visitors(zend_string *function_name)
{
    const struct vyrtue_visitor_array *arr = zend_hash_find_ptr(&VYRTUE_G(function_hooks), function_name);
    if (arr) {
        return arr;
    } else {
        return &EMPTY_VISITOR_ARRAY;
    }
}

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_RETURNS_NONNULL
VYRTUE_ATTR_WARN_UNUSED_RESULT
const struct vyrtue_visitor_array *vyrtue_get_kind_visitors(enum _zend_ast_kind kind)
{
    const struct vyrtue_visitor_array *arr = zend_hash_index_find_ptr(&VYRTUE_G(kind_hooks), (zend_ulong) kind);
    if (arr) {
        return arr;
    } else {
        return &EMPTY_VISITOR_ARRAY;
    }
}
