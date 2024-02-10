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
#include "context.h"

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
zend_ast *vyrtue_context_node_stack_top(struct vyrtue_context *ctx)
{
    return vyrtue_context_stack_top(&ctx->node_stack);
}

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
zend_ast *vyrtue_context_scope_stack_top(struct vyrtue_context *ctx)
{
    return vyrtue_context_stack_top(&ctx->scope_stack);
}
