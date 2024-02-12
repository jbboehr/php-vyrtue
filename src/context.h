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

#define VYRTUE_STACK_SIZE 256

struct vyrtue_context_stack
{
    size_t i;
    zend_ast *data[VYRTUE_STACK_SIZE];
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
    stack->data[stack->i] = ast;
    stack->i++;
}

VYRTUE_ATTR_NONNULL_ALL
static inline void vyrtue_context_stack_pop(struct vyrtue_context_stack *stack, zend_ast *ast)
{
    if (UNEXPECTED(stack->i <= 0)) {
        zend_error(E_WARNING, "vyrtue: stack underflow");
        return;
    }

    stack->i--;

    if (UNEXPECTED(ast != stack->data[stack->i])) {
        zend_error(E_WARNING, "vyrtue: stack pop mismatch");
    }

    stack->data[stack->i] = NULL;
}

VYRTUE_ATTR_NONNULL_ALL
static inline size_t vyrtue_context_stack_count(struct vyrtue_context_stack *stack)
{
    return stack->i;
}

VYRTUE_ATTR_NONNULL_ALL
static inline zend_ast *vyrtue_context_stack_top(struct vyrtue_context_stack *stack)
{
    if (UNEXPECTED(stack->i <= 0)) {
        zend_error(E_WARNING, "vyrtue: stack underflow");
        return NULL;
    }

    return stack->data[stack->i - 1];
}

#endif
