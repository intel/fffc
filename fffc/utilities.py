#! /usr/bin/env python3

# Copyright (C) 2020 Intel Corporation.
# SPDX-License-Identifier: MIT

import base64
import enum

from pycparser import c_ast
from pycparser.c_generator import CGenerator


def expect_string(value):
    try:
        return value.decode()
    except Exception:
        return value


def expect_int(value):
    try:
        return int(value)
    except Exception:
        return value


def expect_attr(die, attr_name, default=None):
    try:
        return die.attributes[attr_name].value
    except Exception:
        return default


def demand_attr(die, attr_name):
    try:
        return die.attributes[attr_name].value
    except Exception as ex:
        raise ex


def expect_string_attr(die, attr_name, default=None):
    try:
        return expect_string(expect_attr(die, attr_name, default))
    except Exception:
        return default


def expect_int_attr(die, attr_name, default=None):
    try:
        return expect_int(expect_attr(die, attr_name, default))
    except Exception:
        return default


def demand_string_attr(die, attr_name):
    return expect_string(demand_attr(die, attr_name))


class TypeStatus(enum.Enum):
    NEW = 1
    DECLARED = 2
    DONE = 3


def build_source(statements):
    s = ""
    generator = CGenerator()
    seen_statements = set()
    for statement in statements:
        current_statement = ""
        if not statement:
            continue
        if isinstance(statement, c_ast.FuncDef):
            current_statement += generator.visit(statement) + "\n"
        elif isinstance(statement, c_ast.Pragma):
            current_statement += generator.visit(statement) + "\n\n"
        else:
            current_statement += generator.visit(statement) + ";\n\n"
        if current_statement not in seen_statements:
            seen_statements.add(current_statement)
            s += current_statement
    return s
