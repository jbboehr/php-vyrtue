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
VYRTUE_ATTR_RETURNS_NONNULL
zend_arena *vyrtue_context_get_arena(struct vyrtue_context *ctx)
{
    return ctx->arena;
}

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
zend_ast *vyrtue_context_scope_stack_top_ast(struct vyrtue_context *ctx)
{
    struct vyrtue_context_stack_frame *frame = vyrtue_context_stack_top(&ctx->scope_stack);
    if (UNEXPECTED(NULL == frame)) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: stack underflow");
    }
    ZEND_ASSERT(frame->ast != NULL);
    return frame->ast;
}

VYRTUE_PUBLIC
VYRTUE_ATTR_NONNULL_ALL
HashTable *vyrtue_context_scope_stack_top_ht(struct vyrtue_context *ctx)
{
    struct vyrtue_context_stack_frame *frame = vyrtue_context_stack_top(&ctx->scope_stack);
    if (UNEXPECTED(NULL == frame)) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: stack underflow");
    }
    if (frame->ht == NULL) {
        frame->ht = zend_arena_calloc(&ctx->arena, 1, sizeof(HashTable));
        zend_hash_init(frame->ht, 8, NULL, NULL, 0);
    }
    return frame->ht;
}
