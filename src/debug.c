/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXVI-MMXXIV John Boehr & contributors
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

static inline void run_common_asserts(struct vyrtue_context *ctx)
{
    if (NULL == vyrtue_context_scope_stack_top_ht(ctx)) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: debug failed to fetch stack ht");
    }

    if (NULL == vyrtue_context_scope_stack_top_ast(ctx)) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: debug failed to fetch scope stack ast");
    }
}

static zend_ast *vyrtue_debug_sample_replacement_enter(zend_ast *ast, struct vyrtue_context *ctx)
{
    fprintf(stderr, "entering sample function\n");

    run_common_asserts(ctx);

    return zend_ast_create_zval_from_long(12345);
}

static zend_ast *vyrtue_debug_sample_replacement_leave(zend_ast *ast, struct vyrtue_context *ctx)
{
    fprintf(stderr, "leaving sample function\n");

    run_common_asserts(ctx);

    return NULL;
}

static zend_ast *vyrtue_debug_sample_function_enter(zend_ast *ast, struct vyrtue_context *ctx)
{
    fprintf(stderr, "entering sample function\n");

    run_common_asserts(ctx);

    return NULL;
}

static zend_ast *vyrtue_debug_sample_function_leave(zend_ast *ast, struct vyrtue_context *ctx)
{
    fprintf(stderr, "leaving sample function\n");

    run_common_asserts(ctx);

    return NULL;
}

static zend_ast *vyrtue_debug_sample_attribute_enter(zend_ast *ast, struct vyrtue_context *ctx)
{
    fprintf(stderr, "entering sample attribute\n");

    run_common_asserts(ctx);

    return NULL;
}

static zend_ast *vyrtue_debug_sample_attribute_leave(zend_ast *ast, struct vyrtue_context *ctx)
{
    fprintf(stderr, "leaving sample attribute\n");

    run_common_asserts(ctx);

    return NULL;
}

VYRTUE_LOCAL PHP_MINIT_FUNCTION(vyrtue_debug)
{
    zend_string *tmp;

    tmp = zend_string_init_interned(ZEND_STRL("VyrtueExt\\Debug\\sample_replacement_function"), 1);
    vyrtue_register_function_visitor("vyrtue internal debug", tmp, vyrtue_debug_sample_replacement_enter, vyrtue_debug_sample_replacement_leave);
    zend_string_release(tmp);

    tmp = zend_string_init_interned(ZEND_STRL("VyrtueExt\\Debug\\sample_function"), 1);
    vyrtue_register_function_visitor("vyrtue internal debug", tmp, vyrtue_debug_sample_function_enter, vyrtue_debug_sample_function_leave);
    zend_string_release(tmp);

    tmp = zend_string_init_interned(ZEND_STRL("VyrtueExt\\Debug\\SampleAttribute"), 1);
    vyrtue_register_attribute_visitor("vyrtue internal debug", tmp, vyrtue_debug_sample_attribute_enter, vyrtue_debug_sample_attribute_leave);
    zend_string_release(tmp);

    return SUCCESS;
}
