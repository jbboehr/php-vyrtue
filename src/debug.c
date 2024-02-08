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
#include "main/php.h"

#include "php_vyrtue.h"

static zend_ast *vyrtue_debug_sample_enter(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    return zend_ast_create_zval_from_long(12345);
}

static zend_ast *vyrtue_debug_sample_leave(zend_ast *ast, struct vyrtue_preprocess_context *ctx)
{
    return NULL;
}

VYRTUE_LOCAL PHP_MINIT_FUNCTION(vyrtue_debug)
{
    zend_string *function_name = zend_string_init_interned(ZEND_STRL("VyrtueExt\\Debug\\sample_replacement_function"), 1);
    vyrtue_register_function_visitor(function_name, vyrtue_debug_sample_enter, vyrtue_debug_sample_leave);
    zend_string_release(function_name);

    return SUCCESS;
}
