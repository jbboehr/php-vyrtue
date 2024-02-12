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

#ifndef PHP_VYRTUE_CONTEXT_H
#define PHP_VYRTUE_CONTEXT_H

#include <Zend/zend_API.h>
#include <Zend/zend_hash.h>
#include <Zend/zend_errors.h>
#include "php_vyrtue.h"

#define VYRTUE_STACK_SIZE 128

struct vyrtue_context_stack_frame
{
    zend_ast *ast;
    HashTable *ht;
};

struct vyrtue_context_stack
{
    size_t i;
    struct vyrtue_context_stack_frame data[VYRTUE_STACK_SIZE];
};

struct vyrtue_context
{
    zend_arena *arena;
    bool in_namespace;
    bool in_group_use;
    zend_string *current_namespace;
    HashTable *imports;
    HashTable *imports_function;
    HashTable *imports_const;
    struct vyrtue_context_stack scope_stack;
    struct vyrtue_context_stack node_stack;
};

VYRTUE_ATTR_NONNULL_ALL
static inline void vyrtue_context_stack_push(struct vyrtue_context_stack *stack, zend_ast *ast)
{
    if (UNEXPECTED(stack->i) >= VYRTUE_STACK_SIZE) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: stack overflow");
    }

    stack->data[stack->i].ast = ast;
    stack->i++;
}

VYRTUE_ATTR_NONNULL_ALL
static inline void vyrtue_context_stack_pop(struct vyrtue_context_stack *stack, zend_ast *ast)
{
    if (UNEXPECTED(stack->i <= 0)) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: stack underflow");
    }

    stack->i--;

    if (UNEXPECTED(ast != stack->data[stack->i].ast)) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: stack pop mismatch");
    }

    stack->data[stack->i].ast = NULL;

    if (stack->data[stack->i].ht) {
        zend_hash_destroy(stack->data[stack->i].ht);
    }
}

VYRTUE_ATTR_NONNULL_ALL
static inline size_t vyrtue_context_stack_count(struct vyrtue_context_stack *stack)
{
    return stack->i;
}

VYRTUE_ATTR_NONNULL_ALL
VYRTUE_ATTR_RETURNS_NONNULL
static inline struct vyrtue_context_stack_frame *vyrtue_context_stack_top(struct vyrtue_context_stack *stack)
{
    if (UNEXPECTED(stack->i <= 0)) {
        zend_error_noreturn(E_COMPILE_ERROR, "vyrtue: stack underflow");
    }

    return &stack->data[stack->i - 1];
}

#endif
