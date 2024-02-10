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

#include <Zend/zend_modules.h>

struct vyrtue_preprocess_context
{
    bool in_namespace;
    bool in_group_use;
    zend_string *current_namespace;
    HashTable *imports;
    HashTable *imports_function;
    HashTable *imports_const;
};

struct vyrtue_visitor
{
    const char *name;
    vyrtue_ast_callback enter;
    vyrtue_ast_callback leave;
};

struct vyrtue_visitor_array
{
    size_t size;
    size_t length;
    struct vyrtue_visitor data[];
};

typedef zend_ast *(*vyrtue_ast_enter_leave_fn)(zend_ast *ast, const struct vyrtue_visitor_array *visitors, struct vyrtue_preprocess_context *ctx);

#ifdef VYRTUE_DEBUG
VYRTUE_LOCAL extern PHP_MINIT_FUNCTION(vyrtue_debug);
#endif
VYRTUE_LOCAL extern PHP_MINIT_FUNCTION(vyrtue_preprocess);
