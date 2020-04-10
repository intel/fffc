#! /usr/bin/env python3

# Copyright (C) 2020 Intel Corporation.
# SPDX-License-Identifier: MIT

"""
dwarf_to_c.py

This program takes a path to an ELF file with DWARF debugging information and
'unDWARFs' it, converting it back into a set of C99 header files describing the
types and functions it contains. It also generates the corresponding mutators
for those types.
"""

import glob
from pathlib import Path
import os
import os.path

from elftools.elf.elffile import ELFFile
from elftools.construct.lib.container import ListContainer

from pycparser import c_ast
from pycparser.c_generator import CGenerator

from .utilities import *
from .template import *


class NotCompiledWithASAN(Exception):
    """Raised when an ELF file was not compiled with ASAN"""
    def __init__(self, elf=""):
        if elf:
            elf = str(elf) + " "
        super().__init__("ELF " + elf + "was not compiled with ASAN")


class NotCompiledWithDWARF(Exception):
    """Raised when an ELF file was not compiled with DWARF"""
    def __init__(self, elf=""):
        if elf:
            elf = str(elf) + " "
        super().__init__("ELF " + elf + "was not compiled with DWARF info; add -g.")


class NotWrittenInC(Exception):
    """Raised when an ELF file was not written in C"""
    def __init__(self, elf=""):
        self.elf = elf
        if elf:
            elf = str(elf) + " "
        super().__init__("ELF " + elf + "was not written in C, not fuzzing")


class DwarfType:
    """Represents the C base types-- ints, chars, and so on.

    These have some general properties and some specific ones. The general cases
    are enough that this is the base class, but the specific ones are dangerous
    and should be paid attention to.

    In particular:

    1/ every base type *will* have a name. There are no anonymous
    base types. This is very different from everything else.

    2/ there are no child types of any kind for base types. This is as low as
    it goes.

    3/ base types cannot be defined, but an instance of a base type can be
    declared. So the status field, definition, and declaration bits are
    pretty wimpy.
    """

    die = None
    cu_object = None
    typename = None
    status = None

    name_attribute = "DW_AT_name"
    type_attribute = "DW_AT_type"
    byte_size_attribute = "DW_AT_byte_size"

    mutator = None

    def __init__(self, die, cu):
        self.die = die
        self.cu_object = cu
        self.typename = self._get_type_name()
        self.status = TypeStatus.DONE

    def __repr__(self):
        location = self.cu_object.get_offset(self.die)
        data = " ".join([str(type(self)), hex(id(self)), str(self.typename), location])
        return "<" + data + ">"

    def _get_child_elements_by_tag(self, tag):
        for child in self.die.iter_children():
            if child and child.tag == tag:
                yield child

    def _get_type_name(self):
        try:
            typename = demand_string_attr(self.die, self.name_attribute)
            # this is a horrible hack to help fix clang's broken DWARF output
            if typename == "sizetype":
                typename = "size_t"
            return typename
        except KeyError:
            return None

    def _get_offset_of_subtype(self, attribute, die=None):
        if not die:
            die = self.die
        try:
            # see if we have a type at all
            raw_offset = demand_attr(die, attribute)
            raw_offset += self.cu_object.cu.cu_offset
        except KeyError:
            # this occurs when we have a void subtype
            return None
        return self.cu_object.format_offset(raw_offset)

    def get_status(self):
        return self.status

    def get_typename(self):
        return self.typename

    def get_size(self):
        return expect_int_attr(self.die, self.byte_size_attribute)

    def add_dependency(self, dependency, name=None):
        # we can't add a dependency on nothing
        if not dependency.get_typename():
            return
        if dependency.get_status() != TypeStatus.DONE:
            ast = dependency.define(name)
            self.cu_object.inferred_header.add_type(name, dependency, ast)

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            identifier = c_ast.IdentifierType([self.typename])
            tdecl = c_ast.TypeDecl(name, qualifiers or [], identifier)
            return tdecl

        return ref

    def declare(self, name=None):
        reference = self.get_reference()
        return reference(name)

    def define(self, name=None):
        return self.declare(name)

    def generate_mutator(self):
        return None, None


class DwarfBaseType(DwarfType):

    # XXX fill this in for other common types
    actual_base_types = {
        "_Bool": (2, 1),
        "char": (6, 1),
        "double": (4, 8),
        "__int128": (5, 16),
        "__int128 unsigned": (7, 16),
        "int": (5, 4),
        "long double": (4, 16),
        "long int": (5, 8),
        "long long int": (5, 8),
        "long long unsigned int": (7, 8),
        "long unsigned int": (7, 8),
        "short": (5, 2),
        "short int": (5, 2),
        "short unsigned int": (7, 2),
        "signed char": (6, 1),
        "unsigned char": (8, 1),
        "unsigned int": (7, 4),
        "unsigned short": (7, 2),
        "size_t": (7, 8),
        "float": (4, 4),
    }

    encoding_tag = "DW_AT_encoding"

    def __init__(self, die, cu):
        self.die = die
        self.cu_object = cu
        self.typename = self._get_type_name()
        self.encoding = self._get_encoding()
        self.size = self.get_size()
        if self.typename in self.actual_base_types:
            self.status = TypeStatus.DONE
        else:
            print(
                "Found unknown basetype. This is probably a bug.",
                self.typename,
                self.encoding,
                self.size,
            )
            self.status = TypeStatus.NEW

    def _get_encoding(self):
        return demand_attr(self.die, self.encoding_tag)

    def _get_matching_real_basename(self):
        if self.typename not in self.actual_base_types:
            my_attrs = (self.encoding, self.size)
            for name, attributes in self.actual_base_types.items():
                if attributes == my_attrs:
                    return name

    def build_ast(self, name, qualifiers, funcspec, storage):
        real_basename = self._get_matching_real_basename()
        identifier = c_ast.IdentifierType([real_basename])
        tdecl = c_ast.TypeDecl(self.typename, qualifiers, identifier)
        storage = ["typedef"] + (storage or [])
        return c_ast.Typedef(self.typename, [], storage, tdecl)

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        self.status = TypeStatus.DONE
        return self.build_ast(name, qualifiers, funcspec, storage)


class DwarfVoidType(DwarfType):
    def __init__(self):
        self.typename = "void"
        self.status = TypeStatus.DONE

    def get_reference(self):
        def VOID(name=None, qualifiers=None, funcspec=None, storage=None):
            ident = c_ast.IdentifierType(["void"])
            return c_ast.TypeDecl(name, qualifiers or [], ident)

        return VOID

    def __repr__(self):
        return "<void>"


class DwarfStructType(DwarfType):

    members = None
    member_types = None

    member_tag = "DW_TAG_member"
    bit_size_attribute = "DW_AT_bit_size"
    byte_size_attribute = "DW_AT_byte_size"
    is_declaration_attribute = "DW_AT_declaration"
    location_attribute = "DW_AT_data_member_location"

    is_packed = False

    constructor = c_ast.Struct

    def __init__(self, die, cu):
        self.die = die
        self.cu_object = cu
        self.typename = self._get_type_name()
        self.status = TypeStatus.NEW

    def get_member_name(self, member_die):
        return expect_string_attr(member_die, self.name_attribute)

    def get_member_location(self, member_die):
        return expect_int_attr(member_die, self.location_attribute)

    def get_member_type(self, member_die):
        # if a type is defined, it will have an offset; if it is not, it's a void
        offset = self._get_offset_of_subtype(self.type_attribute, member_die)
        if not offset:
            return DwarfVoidType().get_reference()
        # add the type if it doesn't exist yet
        subtype = self.cu_object.get_or_add_type(offset)
        # if the type is not anonymous, add a dependency on it.
        # if it is anonymous, we'll define it below.
        if subtype.get_typename():
            self.add_dependency(subtype)
        return subtype

    def get_member_bit_size(self, member_die):
        return expect_int_attr(member_die, self.bit_size_attribute)

    def get_member_byte_size(self, member_die):
        return expect_int_attr(member_die, self.byte_size_attribute)

    def get_member_ast(self, mname, mtype, msize):
        if msize:
            msize = c_ast.Constant("int", str(msize))
        if mtype.get_typename():
            ast = mtype.get_reference()(mname)
        else:
            ast = mtype.define(mname)
        return c_ast.Decl(mname, [], [], [], ast, None, msize)

    def is_aligned(self, start, align, end):
        if not all([start, align, end]):
            return True
        if start < end:
            return False
        if (((end / align) + 1) * align) == start:
            return True
        return False

    def get_members(self):
        members = []
        member_types = []
        for memb in self._get_child_elements_by_tag(self.member_tag):
            # it isn't necessarily the case that we'll have all named attributes
            mname = self.get_member_name(memb)
            # it *is* the case that we'll have all typed entries
            mtype = self.get_member_type(memb)
            # ...but they might also be weird sizes or alignments
            mbitsize = self.get_member_bit_size(memb)
            mbytesize = self.get_member_byte_size(memb)
            malignment = mtype.get_size()
            moffset = self.get_member_location(memb)
            size = malignment or mbytesize
            if (moffset and size) and (moffset % size):
                self.is_packed |= True
            # now make the AST
            mast = self.get_member_ast(mname, mtype, mbitsize)
            members.append(mast)
            member_types.append(mtype)
        return members, member_types

    def is_declaration(self):
        try:
            out = demand_attr(self.die, self.is_declaration_attribute)
            return True
        except KeyError:
            return False

    def build_ast(self, name, typename, members, qualifiers):
        struct_ast = self.constructor(typename, members)
        decl = c_ast.TypeDecl(name, qualifiers, struct_ast)
        return decl

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            return self.build_ast(name, self.typename, None, qualifiers or [])

        return ref

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        self.status = TypeStatus.DECLARED
        return self.build_ast(name, self.typename, None, [])

    def define(self, name=None, qualifiers=None, funcspec=None, storage=None):
        if self.is_declaration():
            return self.declare(name, qualifiers, funcspec, storage)
        if self.members is None:
            self.members, self.member_types = self.get_members()
        self.status = TypeStatus.DONE
        qualifiers = qualifiers or []
        # XXX this sucks. Specifically, it doesn't handle alignment at all.
        # XXX And it's a horrible hack.
        if self.is_packed:
            # XXX reintroduce packed types at some point
            # typename = "__attribute__((packed)) " + (self.typename or "")
            typename = self.typename
        else:
            typename = self.typename
        return self.build_ast(name, typename, self.members, qualifiers)

    def generate_mutator(self):
        if self.mutator:
            return self.mutator
        self.mutator = StructureMutatorTemplate().inject(self)
        return self.mutator


class DwarfEnumType(DwarfType):

    members = None
    enumerator_tag = "DW_TAG_enumerator"
    value_attribute = "DW_AT_const_value"
    is_declaration_attribute = "DW_AT_declaration"

    def __init__(self, die, cu):
        self.die = die
        self.cu_object = cu
        self.typename = self._get_type_name()
        self.status = TypeStatus.NEW

    def is_declaration(self):
        try:
            out = demand_attr(self.die, self.is_declaration_attribute)
            return True
        except KeyError:
            return False

    def get_enum_values(self):
        enum_values = []
        for die in self._get_child_elements_by_tag(self.enumerator_tag):
            enum_name = expect_string_attr(die, self.name_attribute)
            enum_value = expect_int_attr(die, self.value_attribute)
            enum_value = c_ast.Constant("int", str(enum_value))
            enum_values.append(c_ast.Enumerator(enum_name, enum_value))
        return c_ast.EnumeratorList(enum_values)

    def build_ast(self, name, typename, members, qualifiers):
        enum_ast = c_ast.Enum(typename, members)
        decl = c_ast.TypeDecl(name, qualifiers, enum_ast)
        return decl

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            return self.build_ast(name, self.typename, None, qualifiers)

        return ref

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        if self.is_declaration():
            self.members = None
            self.status = TypeStatus.DECLARED
            qualifiers = qualifiers or []
            return self.build_ast(name, self.typename, self.members, storage)
        return self.define(name, qualifiers, funcspec, storage)

    def define(self, name=None, qualifiers=None, funcspec=None, storage=None):
        if self.is_declaration():
            return self.declare(name, qualifiers, funcspec, storage)
        if self.members is None:
            self.members = self.get_enum_values()
        self.status = TypeStatus.DONE
        qualifiers = qualifiers or []
        return self.build_ast(name, self.typename, self.members, qualifiers)

    def generate_mutator(self):
        if self.mutator:
            return self.mutator
        self.mutator = EnumMutatorTemplate().inject(self)
        return self.mutator


class DwarfUnionType(DwarfStructType):
    constructor = c_ast.Union

    def generate_mutator(self):
        if self.mutator:
            return self.mutator
        self.mutator = UnionMutatorTemplate().inject(self)
        return self.mutator


class DwarfFunctionType(DwarfType):

    argument_tag = "DW_TAG_formal_parameter"
    varargs_tag = "DW_TAG_unspecified_parameters"
    external_attr = "DW_AT_external"
    low_pc_attr = "DW_AT_low_pc"

    return_type = None
    arguments = None
    variadic = False

    def __init__(self, die, cu):
        self.die = die
        self.cu_object = cu
        self.typename = self._get_type_name()
        self._parse_external()
        self._parse_low_pc()
        self.status = TypeStatus.NEW

    def _parse_external(self):
        try:
            # We require both that the external attr be present, indicating that
            # this function was available to other CUs
            demand_attr(self.die, self.external_attr)
            self.external = True
        except Exception:
            self.external = False

    def _parse_low_pc(self):
        try:
            self.low_pc = demand_attr(self.die, self.low_pc_attr)
        except Exception:
            self.low_pc = None
            self.external = False

    def get_return_type(self):
        offset = self._get_offset_of_subtype(self.type_attribute)
        if not offset:
            return DwarfVoidType()
        subtype = self.cu_object.get_or_add_type(offset)
        self.add_dependency(subtype)
        return subtype

    def has_varargs(self):
        for arg in self._get_child_elements_by_tag(self.varargs_tag):
            return True
        return False

    def get_arguments(self):
        arguments = []
        for arg in self._get_child_elements_by_tag(self.argument_tag):
            arg_offset = self._get_offset_of_subtype(self.type_attribute, arg)
            if not arg_offset:
                arg_type = DwarfVoidType()
            else:
                arg_type = self.cu_object.get_or_add_type(arg_offset)
            self.add_dependency(arg_type)
            arg_ref = arg_type.get_reference()
            arg_name = expect_string_attr(arg, self.name_attribute)
            arg_decl = c_ast.Decl(arg_name, [], [], [], arg_ref(arg_name), None, None)

            arguments.append(arg_decl)
        if arguments and self.has_varargs():
            arguments.append(c_ast.EllipsisParam())
            self.variadic = True
        return arguments

    def rewrite_argument_names(self):
        for argument in self.arguments:
            if argument == c_ast.EllipsisParam():
                continue
            if argument.name.startswith("_"):
                continue
            argument.name = "_" + argument.name
            if hasattr(argument, "declname"):
                argument.declname = "_" + argument.declname
            while hasattr(argument, "type"):
                if hasattr(argument.type, "declname"):
                    argument.type.declname = "_" + argument.type.declname
                    break
                argument = argument.type

    def build_ast(self, name, typename, arguments, return_type):
        # TODO: shouldn't there be a funcspec here?
        params = c_ast.ParamList(arguments)
        return_ref = return_type.get_reference()
        ret = return_ref(name)
        ast = c_ast.FuncDecl(params, ret)
        return ast

    def _setup(self):
        if self.return_type is None:
            self.return_type = self.get_return_type()
        if self.arguments is None:
            self.arguments = self.get_arguments()

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            self._setup()
            ast = self.build_ast(name, self.typename, self.arguments, self.return_type)
            self.status = TypeStatus.DONE
            return ast

        return ref

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        self._setup()
        ast = self.build_ast(name, self.typename, self.arguments, self.return_type)
        self.status = TypeStatus.DONE
        return ast

    def define(self, name=None, qualifiers=None, funcspec=None, storage=None):
        return self.declare(name, qualifiers, funcspec, storage)

    def call(self, name=None):
        name_id = c_ast.ID(name=name)
        arg_decls = []
        for arg in self.arguments:
            if type(arg) == c_ast.EllipsisParam:
                arg_name = "..."
                arg_type = None
                self.variadic = True
            else:
                arg_name = arg.name
                arg_type = arg.type
            arg_decl = c_ast.ID(arg_name, arg_type)
            arg_decls.append(arg_decl)
        args = c_ast.ExprList(arg_decls)
        func_call = c_ast.FuncCall(name=name_id, args=args)
        return func_call

    def generate_mutator(self):
        if self.mutator:
            return self.mutator
        self.mutator = FunctionMutatorTemplate().inject(self)
        return self.mutator


class DwarfModifierType(DwarfType):

    qualifiers = []
    funcspec = []
    storage = []

    underlying_type = None

    def add_dependency_on_declaration(self, dependency):
        # we can't add a dependency on nothing
        if not dependency.get_typename():
            return
        acceptable_states = [TypeStatus.DECLARED, TypeStatus.DONE]
        if dependency.get_status() not in acceptable_states:
            ast = dependency.declare()
            self.cu_object.inferred_header.add_type(None, dependency, ast)

    def add_dependency_on_definition(self, dependency):
        # we can't add a dependency on nothing
        if not dependency.get_typename():
            return
        acceptable_states = [TypeStatus.DONE]
        if dependency.get_status() not in acceptable_states:
            ast = dependency.define()
            self.cu_object.inferred_header.add_type(None, dependency, ast)

    def get_underlying_type_declaration(self):
        offset = self._get_offset_of_subtype(self.type_attribute)
        if not offset:
            return DwarfVoidType()
        subtype = self.cu_object.get_or_add_type(offset)
        if subtype.get_typename():
            self.add_dependency_on_declaration(subtype)
        return subtype

    def get_underlying_type_definition(self):
        offset = self._get_offset_of_subtype(self.type_attribute)
        if not offset:
            return DwarfVoidType()
        subtype = self.cu_object.get_or_add_type(offset)
        if subtype.get_typename():
            self.add_dependency_on_definition(subtype)
        return subtype

    def get_status(self):
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_declaration()
        return self.underlying_type.get_status()

    def get_typename(self):
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_declaration()
        return self.underlying_type.get_typename()

    def get_size(self):
        explicit_size = expect_int_attr(self.die, self.byte_size_attribute)
        if not explicit_size:
            return self.underlying_type.get_size()
        return explicit_size

    def build_ast(self, name, qualifiers, funcspec, storage):
        qualifiers = qualifiers or []
        qualifiers.extend(self.qualifiers)
        funcspec = funcspec or []
        storage = storage or []
        funcspec.extend(self.funcspec)
        storage.extend(self.storage)
        if self.underlying_type.get_typename():
            underlying_ref = self.underlying_type.get_reference()
            ast = underlying_ref(name, qualifiers, funcspec, storage)
        else:
            ast = self.underlying_type.define(name, qualifiers, funcspec, storage)
        return ast

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            if not self.underlying_type:
                self.underlying_type = self.get_underlying_type_definition()
            return self.build_ast(name, qualifiers, funcspec, storage)

        return ref

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        acceptable_states = [TypeStatus.DECLARED, TypeStatus.DONE]
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_declaration()
        if self.underlying_type.get_status() not in acceptable_states:
            self.underlying_type = self.get_underlying_type_definition()
        return self.build_ast(name, qualifiers, funcspec, storage)

    def define(self, name=None, qualifiers=None, funcspec=None, storage=None):
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_definition()
        if self.underlying_type.get_status() != TypeStatus.DONE:
            self.underlying_type = self.get_underlying_type_definition()
        return self.build_ast(name, qualifiers, funcspec, storage)

    def generate_mutator(self):
        if self.mutator:
            return self.mutator
        self.mutator = ModifierMutatorTemplate().inject(self)
        return self.mutator


class DwarfQualifiedType(DwarfModifierType):
    def build_ast(self, name, qualifiers, funcspec, storage):
        funcspec = funcspec or []
        storage = storage or []
        funcspec.extend(self.funcspec)
        storage.extend(self.storage)
        if self.underlying_type.get_typename():
            underlying_ref = self.underlying_type.get_reference()
            ast = underlying_ref(name, qualifiers, funcspec, storage)
        else:
            ast = self.underlying_type.define(name, qualifiers, funcspec, storage)
        return ast

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            if not self.underlying_type:
                self.underlying_type = self.get_underlying_type_definition()
            qualifiers = qualifiers or []
            qualifiers.extend(self.qualifiers)
            return self.build_ast(name, qualifiers, funcspec, storage)

        return ref


class DwarfArrayType(DwarfModifierType):

    array_sizes = None

    subrange_tag = "DW_TAG_subrange_type"
    upper_bound_attribute = "DW_AT_upper_bound"
    count_attribute = "DW_AT_count"

    def get_underlying_type_definition(self):
        offset = self._get_offset_of_subtype(self.type_attribute)
        if not offset:
            return DwarfVoidType()
        subtype = self.cu_object.get_or_add_type(offset)
        if subtype.get_typename():
            self.add_dependency_on_definition(subtype)
        return subtype

    def get_dimensions(self):
        self.array_sizes = []
        for subrange in self._get_child_elements_by_tag(self.subrange_tag):
            size = expect_int_attr(subrange, self.upper_bound_attribute, -1)
            count = expect_int_attr(subrange, self.count_attribute, 0)
            if type(size) == ListContainer:
                self.array_sizes.extend(size)
            elif (size >= 0) and not count:
                self.array_sizes.append(size + 1)
            elif count and not (size >= 0):
                self.array_sizes.append(count)
            elif count and (size >= 0):
                raise Exception("Received both count and size for array.", self)

    def build_ast(self, name, qualifiers, funcspec, storage):
        if not self.array_sizes:
            self.get_dimensions()
        qualifiers = qualifiers or []
        funcspec = funcspec or []
        storage = storage or []
        qualifiers.extend(self.qualifiers)
        funcspec.extend(self.funcspec)
        storage.extend(self.storage)
        if self.underlying_type.get_typename():
            underlying_ref = self.underlying_type.get_reference()
            ast = underlying_ref(name, qualifiers, funcspec, storage)
        else:
            ast = self.underlying_type.define(name, qualifiers, funcspec, storage)
        for size in self.array_sizes:
            ast = c_ast.ArrayDecl(
                ast, c_ast.Constant("int", str(size)), self.qualifiers
            )
        if not self.array_sizes:
            ast = c_ast.ArrayDecl(ast, None, self.qualifiers)
        return ast

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        acceptable_states = [TypeStatus.DECLARED, TypeStatus.DONE]
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_declaration()
        if self.underlying_type.get_status() not in acceptable_states:
            self.underlying_type = self.get_underlying_type_declaration()
        if not name:
            return None
        return self.build_ast(name, qualifiers, funcspec, storage)

    def define(self, name=None, qualifiers=None, funcspec=None, storage=None):
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_definition()
        if self.underlying_type.get_status() != TypeStatus.DONE:
            self.underlying_type = self.get_underlying_type_definition()
        if not name:
            return None
        return self.build_ast(name, qualifiers, funcspec, storage)

    def generate_mutator(self):
        if self.mutator:
            return self.mutator
        self.mutator = ArrayMutatorTemplate().inject(self)
        return self.mutator


class DwarfAtomicType(DwarfQualifiedType):
    qualifiers = ["_Atomic"]


class DwarfConstType(DwarfQualifiedType):
    qualifiers = ["const"]


class DwarfPointerType(DwarfModifierType):
    def get_size(self):
        # XXX assuming 64 bit
        return 8

    def add_dependency_on_definition(self, dependency):
        # pointers are the type which breaks the declared->declared chain.
        # what that means is that a pointer can decide to only depend on a
        # declaration, where all other types require the full definition.
        # this is what lets us break circular dependencies, and it is critically
        # important.
        self.add_dependency_on_declaration(dependency)

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            if not self.underlying_type:
                self.underlying_type = self.get_underlying_type_declaration()
            qualifiers = qualifiers or []
            qualifiers.extend(self.qualifiers)
            if self.underlying_type.get_typename():
                underlying_ref = self.underlying_type.get_reference()
                ast = underlying_ref(name)
            else:
                ast = self.underlying_type.define(name)
            tdecl = c_ast.PtrDecl(qualifiers, ast)
            return tdecl

        return ref

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        acceptable_states = [TypeStatus.DECLARED, TypeStatus.DONE]
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_declaration()
        if self.underlying_type.get_status() not in acceptable_states:
            self.underlying_type = self.get_underlying_type_declaration()
        if self.underlying_type.get_typename():
            ast = self.underlying_type.declare(name)
        else:
            ast = self.underlying_type.define(name)
        # XXX need some way to pass qualifiers through
        return c_ast.PtrDecl(None, ast)

    def define(self, name=None, qualifiers=None, funcspec=None, storage=None):
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_definition()
        if self.underlying_type.get_status() != TypeStatus.DONE:
            self.underlying_type = self.get_underlying_type_definition()
        if self.underlying_type.get_typename():
            return None
        else:
            ast = self.underlying_type.define(name)
        # XXX need some way to pass qualifiers through
        return c_ast.PtrDecl(None, ast)

    def generate_mutator(self):
        if self.mutator:
            return self.mutator
        self.mutator = PointerMutatorTemplate().inject(self)
        return self.mutator


class DwarfRestrictedType(DwarfQualifiedType):
    qualifiers = ["restrict"]


class DwarfTypedefType(DwarfModifierType):
    def __init__(self, die, cu):
        self.die = die
        self.cu_object = cu
        self.typename = self._get_type_name()
        self.status = TypeStatus.NEW

    def get_status(self):
        return self.status

    def get_typename(self):
        return self.typename

    def get_reference(self):
        def ref(name=None, qualifiers=None, funcspec=None, storage=None):
            if not self.underlying_type:
                self.underlying_type = self.get_underlying_type_definition()
            return c_ast.TypeDecl(
                name, qualifiers, c_ast.IdentifierType([self.typename])
            )

        return ref

    def emit_ref_or_defn(self, name, qualifiers):
        qualifiers = qualifiers or []
        qualifiers.extend(self.qualifiers)
        # if we don't have a typename under here, it's anonymous and we need to
        # define the structure. This is what produces those nicely nested
        # typedefs.
        # otherwise we can just emit a reference
        if not self.underlying_type.get_typename():
            ast = self.underlying_type.define(self.typename)
            self.underlying_type.status = TypeStatus.DONE
        else:
            underlying_ref = self.underlying_type.get_reference()
            ast = underlying_ref(self.typename)
        return c_ast.Typedef(self.typename, qualifiers, ["typedef"], ast)

    def declare(self, name=None, qualifiers=None, funcspec=None, storage=None):
        acceptable_states = [TypeStatus.DECLARED, TypeStatus.DONE]
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_declaration()
        if self.underlying_type.get_status() not in acceptable_states:
            self.underlying_type = self.get_underlying_type_definition()
        self.status = TypeStatus.DECLARED
        return self.emit_ref_or_defn(name, qualifiers)

    def define(self, name=None, qualifiers=None, funcspec=None, storage=None):
        # if we don't have the type, or the type hasn't been fully defined yet,
        # go ahead and force a definition.
        if not self.underlying_type:
            self.underlying_type = self.get_underlying_type_definition()
        if self.underlying_type.get_status() != TypeStatus.DONE:
            self.underlying_type = self.get_underlying_type_definition()
        self.status = TypeStatus.DONE
        return self.emit_ref_or_defn(name, qualifiers)


class DwarfVolatileType(DwarfQualifiedType):
    qualifiers = ["volatile"]


class InferredHeader:
    def __init__(self, cu, outdir):
        self.named = {}
        self.defined = {}
        self.statements = []
        self.output_dir = outdir
        self.header_name = self.get_header_name(cu.get_name(), cu.get_cu_offset())
        cu.inferred_header = self

    def get_header_name(self, cu_name, cu_offset):
        munged_name = cu_name.replace(os.path.sep, "_")
        munged_header_name = munged_name.replace(".c", ".h")
        return "_".join([str(cu_offset), munged_header_name])

    def add_type(self, name, t, ast):
        tname = t.get_typename()
        self.named[name or tname] = t
        self.defined[name or tname] = t.get_status()
        # Don't emit builtin types
        if tname and tname.startswith("__builtin"):
            return
        self.statements.append(ast)

    def generate_header(self):
        return build_source(self.statements)

    def write_header(self):
        header_outfile = open(str(self.output_dir / self.header_name), "w")
        header_outfile.write(self.generate_header())
        header_outfile.close()
        return self.header_name


class DwarfCompileUnit:

    cu = None
    offset_to_die_map = None
    offset_to_type_map = None

    # useful properties of the CU
    name_attribute = "DW_AT_name"
    language_attribute = "DW_AT_language"
    producer_attribute = "DW_AT_producer"

    # basic allowed types
    base_type_tag = "DW_TAG_base_type"
    enumeration_tag = "DW_TAG_enumeration_type"
    structure_tag = "DW_TAG_structure_type"
    union_tag = "DW_TAG_union_type"
    subroutine_tag = "DW_TAG_subroutine_type"
    subprogram_tag = "DW_TAG_subprogram"

    # allowed types-that-are-actually-modifiers
    atomic_tag = "DW_TAG_atomic_type"
    array_tag = "DW_TAG_array_type"
    const_tag = "DW_TAG_const_type"
    pointer_tag = "DW_TAG_pointer_type"
    restrict_tag = "DW_TAG_restrict_type"
    typedef_tag = "DW_TAG_typedef"
    volatile_tag = "DW_TAG_volatile_type"

    # acceptable language types
    dwarf_k_and_r_id = 0x01
    dwarf_ansi_id = 0x02
    dwarf_c99_id = 0x0C
    dwarf_cpp98_id = 0x04
    dwarf_cpp03_id = 0x19
    dwarf_cpp11_id = 0x1a
    dwarf_cpp14_id = 0x21

    cpp_languages = {dwarf_cpp98_id, dwarf_cpp03_id, dwarf_cpp11_id, dwarf_cpp14_id}

    acceptable_languages = {dwarf_k_and_r_id, dwarf_ansi_id, dwarf_c99_id}

    type_tags = {
        base_type_tag: DwarfBaseType,
        enumeration_tag: DwarfEnumType,
        structure_tag: DwarfStructType,
        union_tag: DwarfUnionType,
        subroutine_tag: DwarfFunctionType,
        subprogram_tag: DwarfFunctionType,
        atomic_tag: DwarfAtomicType,
        array_tag: DwarfArrayType,
        const_tag: DwarfConstType,
        pointer_tag: DwarfPointerType,
        restrict_tag: DwarfRestrictedType,
        typedef_tag: DwarfTypedefType,
        volatile_tag: DwarfVolatileType,
    }

    def __init__(self, cu):
        self.cu = cu
        self.cu_die = self.cu.get_top_DIE()
        self.offset_to_die_map = {}
        self.offset_to_type_map = {}
        self.name = self.get_name()
        self.language = expect_string_attr(self.cu_die, self.language_attribute)
        if self.language not in self.acceptable_languages:
            raise NotWrittenInC(self.name)
        self.compiler = self.get_compiler()
        self.build_dies()

    def get_compiler(self):
        compiler = expect_string_attr(self.cu_die, self.producer_attribute)
        if "GNU" in compiler:
            # we're gcc... but what gcc?
            version = compiler.split(" ")[2]
            major_version = version.split(".")[0]
            return ("gcc", int(major_version))
        if "clang" in compiler:
            # we can do the same parsing as above, but don't need to yet
            return ("clang", 0)
        raise Exception("Unknown compiler")

    def get_cu_offset(self):
        return hex(self.cu.cu_offset)

    def get_name(self):
        return expect_string_attr(self.cu_die, self.name_attribute)

    def add_type_object_from_die(self, die):
        t = self.type_tags[die.tag](die, self)
        self.offset_to_type_map[self.get_offset(die)] = t
        return t

    def get_or_add_type(self, offset):
        try:
            return self.offset_to_type_map[offset]
        except KeyError:
            die = self.offset_to_die_map[offset]
            return self.add_type_object_from_die(die)

    def format_offset(self, raw_offset):
        return str(hex(raw_offset))

    def get_offset(self, die):
        return self.format_offset(die.offset)

    def build_dies(self):
        for die in self.cu.iter_DIEs():
            offset = self.get_offset(die)
            self.offset_to_die_map[offset] = die

    def get_named_types(self):
        for offset, die in self.offset_to_die_map.items():
            name = expect_string_attr(die, "DW_AT_name")
            if not name:
                continue
            if die.tag not in self.type_tags:
                continue
            t = self.get_or_add_type(offset)
            if t.get_status() == TypeStatus.DONE:
                continue
            tdecl = t.define(name)
            self.inferred_header.add_type(name, t, tdecl)

    def get_mutable_types(self):
        for t in self.offset_to_type_map.values():
            if t.get_status() != TypeStatus.DONE:
                continue
            if type(t) == DwarfVoidType:
                continue
            if type(t) == DwarfBaseType:
                continue
            yield t

    def get_runnable_functions(self):
        for t in self.offset_to_type_map.values():
            if t.get_status() != TypeStatus.DONE:
                continue
            if type(t) != DwarfFunctionType:
                continue
            if t.variadic:
                continue
            name = expect_string_attr(t.die, "DW_AT_name")
            if not name:
                continue
            if not t.external:
                continue
            if not t.arguments:
                continue
            yield name, t

    def get_builtin_types(self):
        for t in self.offset_to_type_map.values():
            name = t.get_typename()
            if name and "__builtin" in name:
                yield t


class Runner:

    # This is the directory into which we build (and eventually run)
    output_dir = None

    # The target information is used to build the runner scripts. And debug.
    target_function = None  # AST of the target function

    inferred_header = None  # This is what is generated from the CU by the above.

    # Compiling the runner is pretty simple, since everything we need is already
    # in the headers above.
    compile_command = "gcc -O0 -g -fPIC -c -o {out} {source}"

    def __init__(self, name, target_function, target_path, exe_path, inferred_basename, outdir, pie):
        self.target_name = name
        self.target_path = target_path
        self.exe_path = exe_path
        self.output_dir = outdir
        self.target_function = target_function
        self.inferred_header = '#include "%s"\n\n' % inferred_basename
        self.runlib_name = self.build_name()
        self.target_name = expect_string_attr(target_function.die, "DW_AT_name")
        self.rewrite_argument_names()
        self.pie = pie

    def rewrite_argument_names(self):
        # We want to make sure our arg names don't conflict with anything in-scope
        # for the mutator. So, we rewrite all the arguments as _<argname>.
        self.target_function.rewrite_argument_names()

    def build_name(self):
        return self.output_dir / self.target_name

    def write_source(self):
        runlib_source_path = self.runlib_name.with_suffix(".c")
        tmpl = RunnerTemplate(
            self.target_function, self.target_name, os.path.abspath(self.target_path), os.path.abspath(self.exe_path), self.inferred_header, self.pie
        )
        _, runlib_source = tmpl.inject()
        with open(str(runlib_source_path), "w") as f:
            f.write(runlib_source)

    def get_compile_command(self):
        source = str(self.runlib_name.with_suffix(".c"))
        out = str(self.runlib_name.with_suffix(".o"))
        cmd = self.compile_command.format(out=out, source=source)
        return out, cmd


class Mutator:

    output_dir = None
    inferred_header_path = None
    compile_command = "gcc -fPIC -O0 -g -Wno-visibility -Wno-incompatible-pointer-types-discards-qualifiers -c -o {out} {source}"

    decls = None
    defns = None

    def __init__(self, header_path, outdir):
        self.mutated = set()
        self.output_dir = outdir
        self.inferred_header_path = header_path
        self.source_path = outdir / self.inferred_header_path.replace(
            ".h", "_mutator.c"
        )
        self.header_path = self.output_dir / "mutator.h"
        self.decls = []
        self.defns = []

    def generate_include(self, filename):
        return '#include "' + filename + '"\n'

    def add_mutator(self, type_object):
        self.mutated.add(type_object.get_typename())
        decls, defn = type_object.generate_mutator()
        if all((decls, defn)):
            self.decls.extend(decls)
            self.defns.append(defn)

    def write_header(self):
        seen = set()
        with open(str(self.header_path), "a+") as f:
            for decl in self.decls:
                if decl not in seen:
                    f.write(decl + ";\n\n")
                seen.add(decl)

    def write_source(self):
        seen = set()
        with open(str(self.source_path), "w+") as source_file:
            inferred_include = self.generate_include(self.inferred_header_path)
            runtime_include = self.generate_include("fffc_runtime.h")
            mutator_include = self.generate_include("mutator.h")
            source_file.write(runtime_include)
            source_file.write(inferred_include)
            source_file.write(mutator_include)
            source_file.write("\n")
            for defn in self.defns:
                if defn in seen:
                    continue
                source_file.write(defn + "\n\n")
                seen.add(defn)

    def get_compile_command(self):
        # We're actually replacing the last ".c" with ".o" here, but it's a bit
        # screwy-- it might be worth fixing this for souce files with odd names.
        outfile = str(self.source_path)[:-2] + ".o"
        cmd = self.compile_command.format(out=outfile, source=self.source_path)
        return outfile, cmd


class Executable:

    target_path = None
    exe_path = None
    elf_info = None
    dwarf_info = None
    compile_units = None

    # XXX this should probably be a global
    oneshot_compile_command = "gcc -Og -g -fPIC -o {out} {source}"
    compile_command = "gcc -Og -g -fPIC -c -o {out} {source}"
    link_command = (
        "gcc -shared -Og -g -fPIC -o {out} {sources} -lsubhook"
    )

    # getting the location of the asan runtime is really expensive,
    # so we share this state. It isn't ideal, but... it works.
    asan_location_cache = {}

    commands_run = None

    def __init__(self, exe, target, output_dir, headers_only):
        self.target_path = Path(target)
        self.exe_path = Path(exe)
        self.output_dir = Path(output_dir)
        try:
            os.makedirs(str(output_dir))
        except FileExistsError:
            pass
        self.headers_only = headers_only
        self.target_file = self.target_path.open("rb")
        self.exe_file = self.exe_path.open("rb")
        self.elf_info = ELFFile(self.target_file)
        if not self.elf_info.has_dwarf_info():
            raise NotCompiledWithDWARF(self.target)
        self.dwarf_info = self.elf_info.get_dwarf_info()
        self.compile_units = self.get_compile_units()
        self.asan_location = self.get_asan_lib()
        self.named_types = {}
        self.mutated_types = set()
        self.commands_run = []
        self.pie = self.is_pie()

    def run(self, *args, **kwargs):
        self.commands_run.append(" ".join(args[0]))
        subprocess.run(*args, **kwargs)

    def build_debuggable_libs(self):
        compiler = self.compile_units[0][1].compiler[0]
        libnames = []
        dyn = self.elf_info.get_section_by_name(".dynamic")
        for tag in dyn.iter_tags():
            if tag.entry.d_tag == "DT_NEEDED":
                cmd = [compiler, "-print-file-name=" + tag.needed]
                result = subprocess.run(cmd, universal_newlines=True, stdout=subprocess.PIPE)
                if result.returncode != 0:
                    raise Exception("Unable to run compiler; do you have a working build environment?")
                libname = result.stdout.strip()
                # this is a problem with old clang installs on debian
                if libname != tag.needed:
                    libnames.append(libname)
        for libname in libnames:
            try:
                exe = Executable(str(self.exe_path), libname, self.output_dir, self.headers_only)
                exe.generate_sources()
            except NotCompiledWithASAN as exc:
                # generally speaking people aren't trying to fuzz these. Suppress the output for
                # legibility.
                if "asan" not in libname:
                    if "libc.so" not in libname:
                        if "libm.so" not in libname:
                            print(exc)
            except NotCompiledWithDWARF as exc:
                # generally speaking people aren't trying to fuzz these. Suppress the output for
                # legibility.
                if "asan" not in libname:
                    if "libc.so" not in libname:
                        if "libm.so" not in libname:
                            print(exc)


    def get_asan_lib(self):
        try:
            compiler = self.compile_units[0][1].compiler[0]
        except IndexError as error:
            raise NotCompiledWithASAN(str(self.target_path)) from error
        if compiler not in self.asan_location_cache:
            lib = None
            dyn = self.elf_info.get_section_by_name(".dynamic")
            err = False
            for tag in dyn.iter_tags():
                try:
                    if "asan" in tag.needed:
                        lib = tag.needed
                        break
                except Exception:
                    err = True
            if not lib:
                raise Exception("Didn't find libasan dependency; please recompile %s with -fsanitize=address" % self.target_path)
            cmd = [compiler, "-print-file-name=" + lib]
            result = subprocess.run(cmd, universal_newlines=True, stdout=subprocess.PIPE)
            if result.returncode != 0:
                raise Exception("Unable to run compiler; do you have a working build environment?")
            libname = result.stdout.strip()
            if (libname == lib) and ("clang" in compiler):
                # this appears to be a bug in older clang versions. Make one last attempt.
                possible_paths = glob.glob("/usr/lib/clang/*/lib/linux/" + lib)
                if possible_paths:
                    libname = possible_paths[0]
            self.asan_location_cache[compiler] = os.path.realpath(libname)
        return self.asan_location_cache[compiler]

    def is_pie(self):
        e_type = self.elf_info.header['e_type']
        if e_type == 'ET_DYN':
            return True
        elif e_type == 'ET_EXEC':
            return False
        else:
            raise Exception("Couldn't determine whether the binary was PIE or not!")

    def process_compile_unit(self, cu):
        offset = cu.cu_offset
        dcu = DwarfCompileUnit(cu)
        return (offset, dcu)

    def get_compile_units(self):
        compile_units = []
        for cu in self.dwarf_info.iter_CUs():
            try:
                compile_units.append(self.process_compile_unit(cu))
            except NotWrittenInC as err:
                if "asan" not in err.elf:
                    print(err)
        return sorted(compile_units)

    def generate_base_mutators(self):
        # XXX this is pretty hacky
        open(str(self.output_dir / "base.h"), "w+").close()
        base_decls, base_defns = BaseMutatorTemplate().inject()
        m = Mutator("base.h", self.output_dir)
        m.decls = base_decls
        m.defns = base_defns
        m.write_header()
        m.write_source()
        outfile, build_command = m.get_compile_command()
        self.run(build_command.split(), check=True)
        return outfile

    # XXX these should be split into a separate runtime class
    def generate_runtime(self):
        with open(str(self.output_dir / "fffc_runtime.c"), "wb+") as f:
            f.write(read_runtime_source())
        with open(str(self.output_dir / "fffc_runtime.h"), "wb+") as f:
            f.write(read_runtime_header())

    def generate_env_adjuster(self):
        with open(str(self.output_dir / "env_adjuster.c"), "wb+") as f:
            f.write(read_env_adjuster())

    def compile_runtime(self):
        source = str(self.output_dir / "fffc_runtime.c")
        out = str(self.output_dir / "fffc_runtime.o")
        cmd = self.compile_command.format(out=out, source=source)
        self.run(cmd.split(), check=True)
        return str(out)

    def compile_env_adjuster(self):
        source = str(self.output_dir / "env_adjuster.c")
        out = str(self.output_dir / "env_adjuster")
        cmd = self.oneshot_compile_command.format(out=out, source=source)
        self.run(cmd.split(), check=True)
        return str(out)

    def generate_runners_for_cu(self, inferred_header, cu):
        header = inferred_header.header_name
        for name, runnable in cu.get_runnable_functions():
            yield Runner(name, runnable, str(self.target_path), str(self.exe_path), header, self.output_dir, self.pie)

    def generate_mutator_for_cu(self, inferred_header, cu):
        header = inferred_header.header_name
        mutator = Mutator(header, self.output_dir)
        for t in cu.get_mutable_types():
            mutator.add_mutator(t)
        return mutator

    def generate_header_for_cu(self, cu):
        ih = InferredHeader(cu, self.output_dir)
        cu.get_named_types()
        return ih

    def extend_exceptions(self, header, mutator):
        self.named_types.update(header.named)
        self.mutated_types |= mutator.mutated
        header.named.clear()
        mutator.mutated.clear()

    def define_exceptions(self):
        exception_mutator_decls = []
        exception_mutator_defns = []
        for ex in self.named_types.keys() - self.mutated_types:
            t = self.named_types[ex]
            decls, defn = DoNothingMutatorTemplate().inject(t)
            exception_mutator_decls.extend(decls)
            exception_mutator_defns.append(defn)
        do_nothing_path = str(self.output_dir / "do_nothing.c")
        with open(str(do_nothing_path), "w+") as f:
            f.write('#include "mutator.h"\n\n')
            for defn in exception_mutator_defns:
                f.write(defn)
        with open(str(self.output_dir / "mutator.h"), "a+") as f:
            for decl in exception_mutator_decls:
                f.write(decl + ";\n\n")
        # Again, we're actually just replacing ".c" with ".o" here
        do_nothing_binary = do_nothing_path[:-2] + ".o"
        cmd = self.compile_command.format(out=do_nothing_binary, source=do_nothing_path)
        self.run(cmd.split(), check=True)
        return do_nothing_binary

    def make_rebuilder_script(self):
        shell = "#! /bin/sh"
        return "\n".join([shell] + self.commands_run) + "\n\n"

    def make_run_script(self, lib, env_adjuster):
        shell = "#! /bin/bash"
        tracer = "export FFFC_TRACING=Fals"
        replay = "export FFFC_DEBUG_REPLAY=" + ('/' * 4096)
        preload = 'ASAN_OPTIONS=detect_leaks=0 LD_PRELOAD="' + " ".join([self.asan_location, lib]) + '"'
        return shell + "\n" + tracer + "\n" + replay + "\n" + preload + " " + str(self.exe_path.absolute()) + ' "$@"' + "\n" # actually the exe path

    def make_debugger_script(self, lib):
        # makes the gdb script itself, ie, the one the gdb runs
        preload = 'ASAN_OPTIONS=detect_leaks=0 LD_PRELOAD="' + " ".join([self.asan_location, lib]) + '"'
        env_setter = "set exec-wrapper env %s" % preload
        runner = "target exec %s" % str(self.exe_path.absolute()) # actually the exe path
        fork_follow = "set follow-fork-mode child"
        unset_lines = "unset env LINES"
        unset_columns = "unset env COLUMNS"
        set_progname = "set env _ " + str(self.exe_path.absolute()) # actually the exe path
        run_cmd = "run\n"
        return "\n".join([env_setter, runner, fork_follow, unset_lines, unset_columns, set_progname, run_cmd])

    def make_debugger_script_runner(self, lib, env_adjuster, gdb_script_path):
        # make the script that runs the gdb script (I know, it's a bit of a two-step)
        shell = "#! /bin/bash"
        tracer = "export FFFC_TRACING=True"
        replay = """export FFFC_DEBUG_REPLAY=$(PAD=$(printf '%0.1s' "/"{1..4096}) ; echo $FFFC_DEBUG_REPLAY${PAD:${#FFFC_DEBUG_REPLAY}})"""
        shell_script = shell + "\n" + tracer + "\n" + replay + "\ngdb -x " + str(gdb_script_path) + " " + str(self.exe_path.absolute()) + ' "$@"' # actually the exe path
        return shell_script

    def do_link(self, linkage):
        link_command, name, runtime, base, mutator_out, mutator_cmd, runner_out, runner_cmd, do_nothing_binary = (
            linkage
        )
        self.run(mutator_cmd.split(), check=True)
        self.run(runner_cmd.split(), check=True)
        binaries = " ".join([runtime, base, mutator_out, runner_out, do_nothing_binary])
        cmd = link_command.format(out=name, sources=binaries)
        self.run(cmd.split(), check=True)
        return name

    def make_executable(self, strpath):
        os.chmod(strpath, os.stat(strpath).st_mode | 0o111)

    def generate_sources(self):
        # Build the runtime itself
        self.generate_runtime()
        runtime = self.compile_runtime()
        base = self.generate_base_mutators()

        # Build the miscellaneous tools
        self.generate_env_adjuster()
        env_adjuster = self.compile_env_adjuster()
        self.make_executable(env_adjuster)

        # Now build all the inferred pieces
        linkages = []
        for off, cu in self.compile_units:
            inferred_header = self.generate_header_for_cu(cu)
            inferred_header.write_header()
            cu.get_builtin_types()
            mutator = self.generate_mutator_for_cu(inferred_header, cu)
            self.extend_exceptions(inferred_header, mutator)
            mutator.write_header()
            mutator.write_source()
            mutator_out, mutator_cmd = mutator.get_compile_command()
            for runner in self.generate_runners_for_cu(inferred_header, cu):
                runner.write_source()
                runner_out, runner_cmd = runner.get_compile_command()
                name = str(self.output_dir / (runner.target_name + ".so"))
                linkages.append(
                    [
                        self.link_command,
                        name,
                        runtime,
                        base,
                        mutator_out,
                        mutator_cmd,
                        runner_out,
                        runner_cmd,
                    ]
                )

        # now build the exceptions
        do_nothing_binary = self.define_exceptions()
        for linkage in linkages:
            linkage.append(do_nothing_binary)

        # build everything else
        outlibs = []
        for l in linkages:
            outlib = self.do_link(l)
            # drop the .so and add "_runner.sh"
            run_script_name = outlib[:-3] + "_runner.sh"
            with open(str(run_script_name), "w") as f:
                print("Generating", f.name, "...")
                f.write(self.make_run_script(outlib, env_adjuster))
            self.make_executable(run_script_name)
            outlibs.append(outlib)

        # build the rebuilder script
        for outlib in outlibs:
            rebuilder_script_name = outlib[:-3] + "_rebuild.sh"
            with open(str(rebuilder_script_name), "w") as f:
                f.write(self.make_rebuilder_script())
                self.make_executable(rebuilder_script_name)

        # build the debugger script
        for outlib in outlibs:
            debugger_script_name = outlib[:-3] + "_debug.gdb"
            debugger_script_runner_name = outlib[:-3] + "_debug.sh"
            with open(str(debugger_script_name), "w") as f:
                f.write(self.make_debugger_script(outlib))
            with open(str(debugger_script_runner_name), "w") as f:
                f.write(self.make_debugger_script_runner(outlib, env_adjuster, debugger_script_name))
                self.make_executable(debugger_script_runner_name)

        # and build for all the depended-upon libraries
        self.build_debuggable_libs()
