#! /usr/bin/env python3

# Copyright (C) 2020 Intel Corporation.
# SPDX-License-Identifier: MIT

import base64
import copy
import enum
from pathlib import Path
import pickle
import pkgutil
import subprocess
import traceback

from pycparser import c_ast
from pycparser.c_parser import CParser
from pycparser.c_generator import CGenerator

from . import dwarf_to_c


def encode_hash(obj):
    h = abs(hash(obj))
    b = h.to_bytes(8, byteorder="big")
    e = base64.b16encode(b)
    return str(e, "utf-8")


def change_declname(obj, new_name):
    if hasattr(obj, "declname"):
        obj.declname = new_name
    while hasattr(obj, "type"):
        if hasattr(obj.type, "declname"):
            obj.type.declname = new_name
            break
        obj = obj.type


def make_mutator_decl_from_arg_type(
    arg_type, generator=CGenerator(), seen={}, point=True
):
    # memoize
    if arg_type in seen:
        return seen[arg_type]
    mut_name = "fffc_mutator_for_target_type"
    # change the type declname
    change_declname(arg_type, "storage")
    # first, wrap the type in a pointer to match the necessary mutator semantics
    if point:
        arg_type_ptr = c_ast.PtrDecl([], arg_type)
    else:
        arg_type_ptr = arg_type
    # next, wrap that in a decl with the right name
    arg_decl = c_ast.ParamList(
        [c_ast.Decl("storage", [], [], [], arg_type_ptr, None, None)]
    )
    # next, generate the desired decl
    ret_type = c_ast.IdentifierType(["int"])
    ret_decl = c_ast.TypeDecl(mut_name, [], ret_type)
    desired_decl = c_ast.FuncDecl(arg_decl, ret_decl)
    # now build the mangled name
    desired_name = generator.visit(desired_decl)
    suffix = encode_hash(desired_name)
    actual_name = "_Z_fffc_mutator_" + suffix
    desired_decl.type.declname = actual_name
    # build the output
    out = c_ast.Decl(actual_name, [], [], [], desired_decl, None, None)
    # save the result
    seen[arg_type] = (desired_name, out)
    # and go home
    return desired_name, out


def make_call_from_mutator_decl(arg_name, decl):
    name = c_ast.ID(decl.name)
    args = c_ast.ExprList(exprs=[c_ast.UnaryOp("&", c_ast.ID(arg_name))])
    call = c_ast.FuncCall(name, args)
    return call


def make_commented_mutator_call_from_var(var_name, var_type, generator=CGenerator()):
    desired_name, mutator_decl = make_mutator_decl_from_arg_type(var_type)
    mutator_call = make_call_from_mutator_decl(var_name, mutator_decl)
    comment = "/* " + desired_name + "*/\n"
    call = comment + generator.visit(mutator_call) + ";"
    return call


def make_commented_mutator_defn(node, generator=CGenerator()):
    desired_name, decl = make_mutator_decl_from_arg_type(
        node.decl.type.args.params[0].type
    )
    node.decl = decl
    comment = "/* " + desired_name + "*/\n"
    defn = comment + generator.visit(node)
    # make this a k&r style decl, 'cause cheating is sometimes winning after all
    decl.type.args = c_ast.ParamList([])
    decl = comment + generator.visit(decl)
    return decl, defn


def read_runtime_header():
    template_path = str(Path("templates") / "fffc_runtime.h")
    return pkgutil.get_data("fffc", str(template_path))


def read_runtime_source():
    template_path = str(Path("templates") / "fffc_runtime.c")
    return pkgutil.get_data("fffc", str(template_path))


def read_env_adjuster():
    template_path = str(Path("templates") / "env_adjuster.c")
    return pkgutil.get_data("fffc", str(template_path))


def get_sizeof_pointer_to_type(t, reference_ast):
    if not t.get_typename():
        return c_ast.FuncCall(
            c_ast.ID("fffc_estimate_allocation_size"), c_ast.ID("storage")
        )
    if t.get_typename():
        argument_ast = t.get_reference()("storage")
    else:
        argument_ast = t.define("storage")
    prefix = "fffc_get_sizeof_"
    desired_name = CGenerator().visit(argument_ast)
    suffix = encode_hash(desired_name)
    function_name = prefix + suffix
    call = c_ast.FuncCall(c_ast.ID(function_name), reference_ast)
    return call


def define_sizeof_type_from_ast(argument_ast):
    prefix = "fffc_get_sizeof_"
    desired_name = CGenerator().visit(argument_ast)
    suffix = encode_hash(desired_name)
    function_name = prefix + suffix
    storage_tdecl = c_ast.Decl(
        "storage", [], [], [], c_ast.PtrDecl([], argument_ast), None, None
    )
    func_tdecl = c_ast.TypeDecl(
        function_name, [], c_ast.IdentifierType(["long", "long", "unsigned"])
    )
    funcdecl = c_ast.FuncDecl(c_ast.ParamList([storage_tdecl]), func_tdecl)
    funcdef = c_ast.FuncDef(
        c_ast.Decl(function_name, [], [], [], funcdecl, None, None),
        None,
        c_ast.Compound(
            [
                c_ast.Return(
                    c_ast.UnaryOp("sizeof", c_ast.UnaryOp("*", c_ast.ID("storage")))
                )
            ]
        ),
    )
    comment = "/* " + desired_name + "*/\n"
    kr_funcdecl = c_ast.FuncDecl(c_ast.ParamList([]), func_tdecl)
    return comment, kr_funcdecl, funcdef


def define_sizeof_type(t):
    if t.get_typename():
        argument_ast = t.get_reference()("storage")
    else:
        argument_ast = t.define("storage")
    return define_sizeof_type_from_ast(argument_ast)


def define_sizeof_modifier_type(t):
    # setup the toplevel call
    if t.get_typename():
        argument_ast = t.get_reference()("storage")
    else:
        argument_ast = t.define("storage")
    prefix = "fffc_get_sizeof_"
    desired_name = CGenerator().visit(argument_ast)
    suffix = encode_hash(desired_name)
    function_name = prefix + suffix

    # build the underlying function call
    underlying_call = get_sizeof_pointer_to_type(t.underlying_type, c_ast.ID("storage"))

    # build this just as above, except with the call in place of the sizeof
    storage_tdecl = c_ast.Decl(
        "storage", [], [], [], c_ast.PtrDecl([], argument_ast), None, None
    )
    func_tdecl = c_ast.TypeDecl(
        function_name, [], c_ast.IdentifierType(["long", "long", "unsigned"])
    )
    funcdecl = c_ast.FuncDecl(c_ast.ParamList([storage_tdecl]), func_tdecl)
    funcdef = c_ast.FuncDef(
        c_ast.Decl(function_name, [], [], [], funcdecl, None, None),
        None,
        c_ast.Compound([c_ast.Return(underlying_call)]),
    )
    comment = "/* " + desired_name + "*/\n"
    kr_funcdecl = c_ast.FuncDecl(c_ast.ParamList([]), func_tdecl)
    return comment, kr_funcdecl, funcdef


def define_sizeof_do_nothing_type(t):
    comment, funcdecl, funcdef = define_sizeof_type(t)
    funcdef.body = c_ast.Compound([c_ast.Return(c_ast.Constant(value="0", type="int"))])
    return comment, funcdecl, funcdef


class Template:

    template_name = None
    text = None
    ast = None
    saved_ast = None

    _preprocessor_command = ["gcc", "-E", "-xc", "-"]

    @classmethod
    def _clean_text(cls, text):
        output_text = []
        for line in text.splitlines():
            if not line:
                continue
            if line.startswith("#"):
                continue
            output_text.append(line)
        return "\n".join(output_text)

    @classmethod
    def _load_template(cls):
        template_path = str(Path("templates") / cls.template_name)
        raw_data = pkgutil.get_data("fffc", str(template_path))
        gcc_run = subprocess.run(
            cls._preprocessor_command,
            input=raw_data,
            stdout=subprocess.PIPE,
            check=True,
        )
        return cls._clean_text(gcc_run.stdout.decode())

    def __new__(cls):
        if not cls.text:
            cls.text = cls._load_template()
            cls.parser = CParser()
            cls.generator = CGenerator()
            cls.saved_ast = cls.parser.parse(cls.text)
        return super().__new__(cls)

    def __init__(self):
        self.ast = pickle.loads(pickle.dumps(self.saved_ast))

    def get_nodes(self, node):
        for child in node:
            yield child
            yield from self.get_nodes(child)

    def replace_placeholder_type(self, dwarf_type):
        for node in self.get_nodes(self.ast):
            try:
                if type(node) == c_ast.Decl:
                    if self.placeholder_type_name in node.type.type.names:
                        if dwarf_type.get_typename():
                            replacement = dwarf_type.get_reference()(node.type.declname)
                        else:
                            replacement = dwarf_type.define(node.type.declname)
                        node.type = replacement
                if type(node) == c_ast.Typename:
                    if self.placeholder_type_name in node.type.type.names:
                        if dwarf_type.get_typename():
                            replacement = dwarf_type.get_reference()()
                        else:
                            replacement = dwarf_type.define()
                        node.type = replacement
            except AttributeError:
                continue

    def replace_funcs(self, dwarf_type):
        decls = []
        defn = None
        # replace the underlying call
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncCall:
                if node.name.name == self.underlying_mutator_name:
                    if dwarf_type.get_typename():
                        ut = dwarf_type.get_reference()()
                    else:
                        ut = dwarf_type.define()
                    underlying_mutator_name, underlying_decl_ast = make_mutator_decl_from_arg_type(
                        ut, point=True
                    )
                    comment = "/* " + underlying_mutator_name + "*/\n"
                    underlying_mutator_call = make_call_from_mutator_decl(
                        "tmp", underlying_decl_ast
                    )
                    node.name = underlying_mutator_call.name
                    # make this a k&r style decl, 'cause cheating is sometimes winning after all
                    underlying_decl_ast.type.args = c_ast.ParamList([])
                    decls.append(comment + CGenerator().visit(underlying_decl_ast))
        # build the decl and defn
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncDef:
                if not defn:
                    decl, defn = make_commented_mutator_defn(node)
                    decls.append(decl)
        return decls, defn


class BaseMutatorTemplate(Template):

    template_name = "base_mutators.c"
    underlying_mutator_name = "fffc_mutator_for_underlying_type"

    decls = None
    defns = None

    def build_sizeof(self, argument_ast):
        comment, decl, defn = define_sizeof_type_from_ast(argument_ast)
        return comment, decl, defn

    def inject(self):
        if self.decls:
            return self.decls, self.defns
        self.decls = []
        self.defns = []
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncDef:
                comment, decl, defn = self.build_sizeof(
                    node.decl.type.args.params[0].type
                )
                self.decls.append(CGenerator().visit(decl))
                self.defns.append(CGenerator().visit(defn))
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncDef:
                decl, defn = make_commented_mutator_defn(node)
                self.decls.append(decl)
                self.defns.append(defn)
        return self.decls, self.defns


class DoNothingMutatorTemplate(Template):

    template_name = "do_nothing_mutator.c"
    placeholder_type_name = "__TARGET_TYPE__"

    # XXX this should go away
    pointer_depth = 6

    def replace_placeholder_type(self, dwarf_type):
        for node in self.get_nodes(self.ast):
            try:
                if self.placeholder_type_name in node.type.type.names:
                    if dwarf_type.get_typename():
                        replacement = dwarf_type.get_reference()(node.type.declname)
                    else:
                        replacement = dwarf_type.define(node.type.declname)
                    replacement_pointer = c_ast.PtrDecl([], replacement)
                    node.type = replacement_pointer
                if type(node) == c_ast.Typename:
                    if self.placeholder_type_name in node.type.type.names:
                        if dwarf_type.get_typename():
                            replacement = dwarf_type.get_reference()()
                        else:
                            replacement = dwarf_type.define()
                        replacement_pointer = c_ast.PtrDecl([], replacement)
                        node.type = replacement_pointer
            except AttributeError as ex:
                continue

    def replace_funcs(self, dwarf_type):
        decls = []
        defns = ""
        # build the decl and defn
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncDef:
                argtype = node.decl.type.args.params[0].type
                for i in range(self.pointer_depth):
                    node_copy = copy.deepcopy(node)
                    node_copy.decl.type.args.params[0].type = argtype
                    decl, defn = make_commented_mutator_defn(node_copy)
                    decls.append(decl)
                    defns += defn
                    argtype = c_ast.PtrDecl([], argtype)
                return decls, defns

    def inject(self, obj):
        if not obj.typename:
            # XXX the fact that we're refusing to generate mutators for anonymous types
            # XXX should probably be made more explicit
            return [], ""
        decls = [CGenerator().visit(obj.define(None))]
        self.replace_placeholder_type(obj)
        funcdecls, defn = self.replace_funcs(obj)
        comment, sizedecl, sizedef = define_sizeof_do_nothing_type(obj)
        decls.append(CGenerator().visit(sizedecl))
        defn += CGenerator().visit(sizedef)
        return decls + funcdecls, defn


class ArrayMutatorTemplate(Template):

    template_name = "array_mutator.c"
    placeholder_type_name = "__TARGET_TYPE__"
    underlying_mutator_name = "fffc_mutator_for_underlying_type"

    def replace_underlying_sizeof(self, ut):
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.BinaryOp:
                try:
                    if node.right.name.name == "fffc_get_sizeof_type":
                        node.right = get_sizeof_pointer_to_type(
                            ut, c_ast.UnaryOp("*", c_ast.ID("storage"))
                        )
                except AttributeError:
                    pass

    def inject(self, pointer_type):
        if not pointer_type.underlying_type:
            return None, None
        self.replace_placeholder_type(pointer_type)
        self.replace_underlying_sizeof(pointer_type.underlying_type)
        decls, defn = self.replace_funcs(pointer_type.underlying_type)
        if pointer_type.array_sizes:
            comment, sizedecl, sizedef = define_sizeof_type(pointer_type)
            decls.append(CGenerator().visit(sizedecl))
            defn += CGenerator().visit(sizedef)
        return decls, defn


class PointerMutatorTemplate(Template):

    template_name = "pointer_mutator.c"
    placeholder_type_name = "__TARGET_TYPE__"
    underlying_mutator_name = "fffc_mutator_for_underlying_type"

    def replace_underlying_sizeof(self, ut):
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.Decl:
                try:
                    if node.init.name.name == "fffc_get_sizeof_type":
                        node.init = get_sizeof_pointer_to_type(
                            ut, c_ast.UnaryOp("*", c_ast.ID("storage"))
                        )
                except AttributeError:
                    pass

    def inject(self, pointer_type):
        if not pointer_type.underlying_type:
            return None, None
        self.replace_placeholder_type(pointer_type)
        self.replace_underlying_sizeof(pointer_type.underlying_type)
        decls, defn = self.replace_funcs(pointer_type.underlying_type)
        # XXX This is a hack, because it's easier to just remove the offending
        # XXX inner mutation bits than to mess with the AST. The issue here is
        # XXX indexing into a function pointer, which is something the basic
        # XXX pointer mutator does and which is a no-go.
        if type(pointer_type.underlying_type) == dwarf_to_c.DwarfFunctionType:
            lines = defn.splitlines()
            defn = "\n".join(lines[:3] + lines[-3:]) + "\n"
        comment, sizedecl, sizedef = define_sizeof_type(pointer_type)
        decls.append(CGenerator().visit(sizedecl))
        defn += CGenerator().visit(sizedef)
        return decls, defn


class ModifierMutatorTemplate(Template):

    template_name = "modifier_mutator.c"
    placeholder_type_name = "__TARGET_TYPE__"
    underlying_mutator_name = "fffc_mutator_for_underlying_type"

    def inject(self, modifier_type):
        if not modifier_type.underlying_type:
            return None, None
        self.replace_placeholder_type(modifier_type)
        decls, defn = self.replace_funcs(modifier_type.underlying_type)
        comment, sizedecl, sizedef = define_sizeof_modifier_type(modifier_type)
        decls.append(CGenerator().visit(sizedecl))
        defn += CGenerator().visit(sizedef)
        return decls, defn


class FunctionMutatorTemplate(Template):

    template_name = "function_mutator.c"
    placeholder_type_name = "__TARGET_TYPE__"

    def inject(self, modifier_type):
        self.replace_placeholder_type(modifier_type)
        decls, defn = self.replace_funcs(modifier_type)
        comment, sizedecl, sizedef = define_sizeof_type(modifier_type)
        decls.append(CGenerator().visit(sizedecl))
        defn += CGenerator().visit(sizedef)
        return decls, defn


# This is just a global used to track context for the structure and union templates
# It's necessary for anything that nests (structs, enums, unions)
class NestingContext:
    tmp_count = 1
    rnd_count = 0
    current_rnd_value = 0
    values_count = 1


nesting_context = NestingContext()


class EnumMutatorTemplate(Template):

    template_name = "enum_mutator.c"
    placeholder_type_name = "__TARGET_TYPE__"

    decls = None
    defn = None

    def replace_placeholder_type(self, dwarf_type):
        for node in self.get_nodes(self.ast):
            try:
                if type(node) == c_ast.Decl:
                    if self.placeholder_type_name in node.type.type.names:
                        if dwarf_type.get_typename():
                            replacement = dwarf_type.get_reference()(node.type.declname)
                        else:
                            replacement = dwarf_type.define(node.type.declname)
                        node.type = replacement
                if type(node) == c_ast.Typename:
                    if self.placeholder_type_name in node.type.type.names:
                        if dwarf_type.get_typename():
                            replacement = dwarf_type.get_reference()()
                        else:
                            replacement = dwarf_type.define()
                        node.type = replacement
            except AttributeError:
                continue

    def replace_enum_values(self, enum_object, storage_lvalue=None):
        constants = []
        for enum in enum_object.members.enumerators:
            constants.append(c_ast.Constant(type="int", value=str(enum.value.value)))
        init_list = c_ast.InitList(exprs=constants)
        init_count = c_ast.Constant(type="int", value=str(len(constants)))
        values = None
        values_len = None
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.Decl:
                if node.name == "values":
                    values = node
                if node.name == "values_len":
                    values_len = node
                if node.name == "idx":
                    idx = node
            if type(node) == c_ast.ID:
                if node.name == "values_len":
                    values_len_id = node
                if node.name == "values":
                    values_id = node
                if node.name == "idx":
                    idx_id = node
            if type(node) == c_ast.Assignment:
                if node.lvalue.expr.name == "storage":
                    storage_expr = node
        values.init = init_list
        new_values_name = values.name + str(nesting_context.values_count)
        values.name = new_values_name
        change_declname(values, new_values_name)
        values_id.name = new_values_name
        values_len.init = init_count
        new_values_len_name = values_len.name + str(nesting_context.values_count)
        values_len.name = new_values_len_name
        change_declname(values_len, new_values_len_name)
        values_len_id.name = new_values_len_name
        new_idx_name = idx.name + str(nesting_context.values_count)
        idx.name = new_values_name
        change_declname(idx, new_idx_name)
        idx_id.name = new_idx_name
        if storage_lvalue:
            storage_expr.lvalue = storage_lvalue
        nesting_context.values_count += 1

    def replace_funcs(self, dwarf_type):
        decls = []
        defn = None
        # build the decl and defn
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncDef:
                if not defn:
                    decl, defn = make_commented_mutator_defn(node)
                    decls.append(decl)
        return decls, defn

    def do_replacements(self, obj):
        self.replace_placeholder_type(obj)
        self.replace_enum_values(obj)
        decls, defn = self.replace_funcs(obj)
        return decls, defn

    def build_nested_mutator(self, obj, member_reference):
        self.replace_placeholder_type(obj)
        self.replace_enum_values(obj, member_reference)
        return self.ast.ext[1].body

    def inject(self, enum_object):
        decls, defn = self.do_replacements(enum_object)
        comment, sizedecl, sizedef = define_sizeof_type(enum_object)
        decls.append(CGenerator().visit(sizedecl))
        defn += CGenerator().visit(sizedef)
        global nesting_context
        nesting_context = NestingContext()
        return decls, defn


class StructureMutatorTemplate(Template):

    template_name = "structure_mutator.c"
    placeholder_type_name = "__TARGET_TYPE__"
    struct_name = "storage"

    decls = None
    defn = None

    def build_ref(self, member_name, ref_type, struct_id=None):
        if not struct_id:
            struct_id = c_ast.ID(self.struct_name)
        member_id = c_ast.ID(member_name)
        struct_ref = c_ast.StructRef(struct_id, ref_type, member_id)
        return struct_ref

    def build_dot_ref(self, member_name, struct_name=None):
        return self.build_ref(member_name, ".", struct_name)

    def build_arrow_ref(self, member_name, struct_name=None):
        return self.build_ref(member_name, "->", struct_name)

    def take_address_of(self, expr):
        return c_ast.UnaryOp("&", expr)

    def build_member_mutator(
        self, struct_object, member_ast, member_type, member_reference=None
    ):
        # There are a few different possibilities here that prevent us from doing the straightforward
        # thing.
        #
        # One is that a type may have a bitlength, which means that we can't just take a pointer to it
        # and go. The solution to that is to declare a new type of that and copy it over. The problem
        # with this is that we go from requiring a decl of that type to requring a definition of that
        # type. However, since a bitfield is required to be of type _Bool, unsigned int, signed int,
        # 'or some other implementation defined type' this is generally not problematic. I'm sure there
        # will be some implementation defined type for which this *is* problematic, however. Atomics
        # for sure come to mind.
        #
        # The second is the case where we don't have a name for the struct member. This is problematic
        # because we might still need to fuzz that, and it might be a bitfield, and so we might need
        # a copier for that, and we currently name the temporary variable the same thing as the member.
        # The solution to this is to name it something like 'tmp_N' where N is the count of the anon
        # members we've needed copiers for.
        #
        # The third is the hardest: anonymous types. We can't simply farm out to the next level of
        # mutator for these because there may not be a defined mutator for them. The options are to
        # either come up with a name for the type and define it, which is what I used to do in prior
        # versions of this software, or to mutate it in place.
        #
        # Generating a name for the type is pretty straightfoward given its definition: we simply
        # hash the text of it to produce the name. However, it does add types that do not exist in
        # the original program and this causes all kinds of problems. It also looks hideous. As a
        # result, I've taken the other approach here. Time will tell if it's a terrible mistake, but
        # at least it's a new one.
        #
        # We also have to deal with the case where you have an anonymous struct with no member name.
        # Fortunately, we can simply recurse in that case-- it's legal to refer to their members as
        # though they were top level members.
        #
        # There are yet more cases: doubly anonymous enums and unions. Dealing with a doubly
        # anonymous enum only makes sense in the event that there's behavior in the program which
        # isn't exposed via its typesystem-- the common case of a union will already be handled
        # at a higher level. At the moment, I simply don't mutate these cases.
        #
        # There's yet another wrinkle in packed or aligned structs. I can't currently detect a
        # struct aligned to a specific value, but it is possible to do so. More on that piece
        # later. I can, however, detect a packed struct. The problem is what to do then.
        #
        # Specifically, writing through a pointer to a member of a packed struct is a no-no. It
        # will (probably) not be memory safe to do so. The correct thing then is to never take
        # the address of a member of a packed struct. Unfortunately, this is how the rest of the
        # system works.
        #
        # You might think at this point that you can simply create a new value of the appropriate
        # type, copy the member value to that new value, take the address of that, send it off to
        # be mutated, and copy the result back. This is very very wrong, because while we have the
        # ability to mutate the datatype we can't count on having a definition for it here. Doing
        # anything which blurs the lines across CUs is the way of pain, so that's right out.
        #
        # One way to get around that is to add an allocator API for types. I could then return a
        # pointer to the allocated type, then copy?
        #
        mutator_desired_name, mutator_decl_ast = make_mutator_decl_from_arg_type(
            member_ast.type
        )
        mutator_id = c_ast.ID(mutator_decl_ast.name)
        if not member_reference:
            member_reference = self.build_arrow_ref(member_ast.name)
        else:
            member_reference = self.build_dot_ref(member_ast.name, member_reference)

        is_anonymous = False
        if type(member_type) in {
            dwarf_to_c.DwarfStructType,
            dwarf_to_c.DwarfEnumType,
            dwarf_to_c.DwarfUnionType,
        }:
            if not member_type.typename:
                is_anonymous = True

        if member_ast.bitsize:
            # XXX we special case arrays because it's inconvenient to handle them, not because we can't.
            # XXX There's an implicit TODO here: support arrays/VLAIS in packed structs
            if type(member_type) == dwarf_to_c.DwarfArrayType:
                comment = c_ast.ID("/* Skipping VLAIS in packed struct */")
                return c_ast.Compound([comment])
            comment = c_ast.ID("/* " + self.generator.visit(member_ast) + " */")
            tmp_name = "tmp_" + str(nesting_context.tmp_count)
            tmp_id = c_ast.ID(tmp_name)
            nesting_context.tmp_count += 1
            tmp_decl = c_ast.Decl(
                tmp_id, None, None, None, member_ast.type, member_reference, None
            )
            change_declname(tmp_decl, tmp_name)
            tmp_addr = self.take_address_of(tmp_id)
            args = c_ast.ExprList(exprs=[tmp_addr])
            call = c_ast.FuncCall(mutator_id, args)
            writeback = c_ast.Assignment("=", member_reference, tmp_id)
            return c_ast.Compound([comment, tmp_decl, call, writeback])
        elif is_anonymous:
            if type(member_type) == dwarf_to_c.DwarfEnumType:
                tmpl = EnumMutatorTemplate()
                body = tmpl.build_nested_mutator(member_type, member_reference)
                return c_ast.Compound(body.block_items[:-1])
            elif type(member_type) == dwarf_to_c.DwarfUnionType:
                tmpl = UnionMutatorTemplate()
                rnd = tmpl.build_random_value(member_type)
                nested_muts = [rnd]
                for compound in tmpl.build_all_member_mutators(
                    member_type, member_reference
                ):
                    nested_muts.extend(compound.block_items)
                out = c_ast.Compound(nested_muts)
                return out
            else:
                if not member_ast.name:
                    # this is the doubly anonymous case, which means we can refer to the nested members as
                    # though they were toplevel members (because C is kinda weird)
                    comment = c_ast.ID("/* " + self.generator.visit(member_ast) + " */")
                    nested_muts = [comment]
                    for compound in self.build_all_member_mutators(member_type):
                        nested_muts.extend(compound.block_items)
                    return c_ast.Compound(nested_muts)
                else:
                    # in this case we want to recurse as well, but we need to reference the intermediate name.
                    comment = c_ast.ID("/* " + self.generator.visit(member_ast) + " */")
                    nested_muts = [comment]
                    for compound in self.build_all_member_mutators(
                        member_type, member_reference
                    ):
                        nested_muts.extend(compound.block_items)
                    return c_ast.Compound(nested_muts)
        else:
            comment = c_ast.ID("/* " + self.generator.visit(member_ast) + " */")
            member_addr = self.take_address_of(member_reference)
            args = c_ast.ExprList(exprs=[member_addr])
            call = c_ast.FuncCall(mutator_id, args)
            return c_ast.Compound([comment, call])

    def build_all_member_mutators(self, struct_object, member_reference=None):
        for member_ast, member_type in zip(
            struct_object.members, struct_object.member_types
        ):
            yield self.build_member_mutator(
                struct_object, member_ast, member_type, member_reference
            )

    def inject(self, struct_object):
        if self.decls:
            return self.decls, self.defn
        self.decls = []
        if not struct_object.typename:
            # XXX the fact that we're refusing to generate mutators for anonymous types
            # XXX should probably be made more explicit
            return [], ""
        self.replace_placeholder_type(struct_object)
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncDef:
                if not self.defn:
                    for memb_mut in self.build_all_member_mutators(struct_object):
                        node.body.block_items[-1:-1] = memb_mut
                    decl, defn = make_commented_mutator_defn(node)
                    self.decls.append(decl)
                    self.defn = defn
                    break
        comment, sizedecl, sizedef = define_sizeof_type(struct_object)
        self.decls.append(CGenerator().visit(sizedecl))
        self.defn += CGenerator().visit(sizedef)
        global nesting_context
        nesting_context = NestingContext()
        return self.decls, self.defn


class UnionMutatorTemplate(StructureMutatorTemplate):

    template_name = "union_mutator.c"

    def build_member_mutator(
        self, union_object, member_ast, member_type, member_reference=None
    ):
        # The issue with unions is that we don't actually know which datatype to
        # fuzz. That's not great, but it does mimic the behavior you'll get if an
        # attacker really is able to hand you a union of pointers.
        mutator_desired_name, mutator_decl_ast = make_mutator_decl_from_arg_type(
            member_ast.type
        )
        mutator_id = c_ast.ID(mutator_decl_ast.name)
        if not member_reference:
            member_reference = self.build_arrow_ref(member_ast.name)
        else:
            member_reference = self.build_dot_ref(member_ast.name, member_reference)

        # Like structs, enums can also contain anonymous types.
        is_anonymous = False
        if type(member_type) in {
            dwarf_to_c.DwarfStructType,
            dwarf_to_c.DwarfEnumType,
            dwarf_to_c.DwarfUnionType,
        }:
            if not member_type.typename:
                is_anonymous = True

        # If the member is anonymous, we have to do basically the same thing
        # we do for structures above: recurse and hope for the best.
        nested_structure_template = StructureMutatorTemplate()

        if is_anonymous:
            if type(member_type) == dwarf_to_c.DwarfEnumType:
                tmpl = EnumMutatorTemplate()
                body = tmpl.build_nested_mutator(member_type, member_reference)
                return c_ast.Compound(body.block_items[:-1])
            elif type(member_type) == dwarf_to_c.DwarfStructType:
                nested_muts = []
                for compound in nested_structure_template.build_all_member_mutators(
                    member_type, member_reference
                ):
                    nested_muts.extend(compound.block_items)
                out = c_ast.Compound(nested_muts)
                return out
            else:
                if not member_ast.name:
                    comment = c_ast.ID("/* " + self.generator.visit(member_ast) + " */")
                    nested_muts = [comment]
                    for compound in self.build_all_member_mutators(member_type):
                        nested_muts.extend(compound.block_items)
                    return c_ast.Compound(nested_muts)
                else:
                    comment = c_ast.ID("/* " + self.generator.visit(member_ast) + " */")
                    nested_muts = [comment]
                    for compound in self.build_all_member_mutators(
                        member_type, member_reference
                    ):
                        nested_muts.extend(compound.block_items)
                    return c_ast.Compound(nested_muts)
        else:
            comment = c_ast.ID("/* " + self.generator.visit(member_ast) + " */")
            member_addr = self.take_address_of(member_reference)
            args = c_ast.ExprList(exprs=[member_addr])
            call = c_ast.FuncCall(mutator_id, args)
            whole_shebang = c_ast.Compound([comment, call])
            condition = c_ast.BinaryOp(
                op="==",
                left=c_ast.ID(name="rnd" + str(nesting_context.rnd_count)),
                right=c_ast.Constant(
                    type="int", value=str(nesting_context.current_rnd_value)
                ),
            )
            if_statement = c_ast.If(cond=condition, iftrue=whole_shebang, iffalse=None)
            compound = c_ast.Compound([if_statement])
        nesting_context.tmp_count += 1
        nesting_context.current_rnd_value += 1
        return compound

    def build_random_value(self, union_object):
        global nesting_context
        nesting_context.rnd_count += 1
        nesting_context.current_rnd_value = 0
        random_tdecl = c_ast.TypeDecl(
            "rnd" + str(nesting_context.rnd_count),
            [],
            type=c_ast.IdentifierType(["int"]),
        )
        random_call = c_ast.FuncCall(c_ast.ID("fffc_get_random"), args=None)
        random_range = c_ast.Constant(type="int", value=str(len(union_object.members)))
        random_reduce = c_ast.BinaryOp("%", random_call, random_range)
        return c_ast.Decl(
            "rnd" + str(nesting_context.rnd_count),
            [],
            [],
            [],
            random_tdecl,
            random_reduce,
            None,
        )

    def inject(self, union_object, body_only=False):
        if self.decls:
            return self.decls, self.defn
        self.decls = []
        if not union_object.typename and not body_only:
            # XXX the fact that we're refusing to generate mutators for anonymous types
            # XXX should probably be made more explicit
            return [], ""
        self.replace_placeholder_type(union_object)
        for node in self.get_nodes(self.ast):
            if type(node) == c_ast.FuncDef:
                if not self.defn:
                    rnd = self.build_random_value(union_object)
                    for memb_mut in self.build_all_member_mutators(union_object):
                        node.body.block_items[-1:-1] = memb_mut
                    node.body.block_items.insert(0, rnd)
                    if body_only:
                        # remove the return statement
                        node.body.block_items.pop(-1)
                        return node.body
                    decl, defn = make_commented_mutator_defn(node)
                    self.decls.append(decl)
                    self.defn = defn
                    break
        comment, sizedecl, sizedef = define_sizeof_type(union_object)
        self.decls.append(CGenerator().visit(sizedecl))
        self.defn += CGenerator().visit(sizedef)
        global nesting_context
        nesting_context = NestingContext()
        return self.decls, self.defn


class RunnerTemplate:

    template_name = "fffc_runner.c"

    def __init__(self, func, name, binary_path, executable_path, inferred_header_include, pie):
        self.generator = CGenerator()
        self.func = func
        self.name = name
        self.binary_path = binary_path
        self.exe_path = executable_path
        self.pie = pie
        self.template_path = self._get_template_path()
        self.template_data = pkgutil.get_data("fffc", str(self.template_path))
        self.inferred_header_include = inferred_header_include
        self.hook_sig = self.generator.visit(func.define("FFFC_replacement"))
        self.parallel_sig = self.generator.visit(func.define("FFFC_parallel_replacement"))
        self.proxy_sig = self.generator.visit(
                                                    func.build_ast(
                                                                    "FFFC_proxy_target",
                                                                    func.typename,
                                                                    func.arguments,
                                                                    dwarf_to_c.DwarfVoidType())
                                                )
        self.worker_sig = self.generator.visit(
                                                    func.build_ast(
                                                                    "FFFC_worker_target",
                                                                    func.typename,
                                                                    func.arguments,
                                                                    dwarf_to_c.DwarfVoidType())
                                                )

    def replace_target_name(self):
        raw = self.template_data
        placeholder = b"___FFFC_TARGET_NAME___"
        raw = raw.replace(placeholder, bytes(self.name, "utf-8"))
        self.template_data = raw

    def replace_hook_sig(self):
        raw = self.template_data
        placeholder = b"___FFFC_HOOK_SIG___"
        raw = raw.replace(placeholder, bytes(self.hook_sig, "utf-8"))
        self.template_data = raw

    def replace_parallel_sig(self):
        raw = self.template_data
        placeholder = b"___FFFC_PARALLEL_SIG___"
        raw = raw.replace(placeholder, bytes(self.parallel_sig, "utf-8"))
        self.template_data = raw

    def replace_proxy_sig(self):
        raw = self.template_data
        placeholder = b"___FFFC_PROXY_SIG___"
        raw = raw.replace(placeholder, bytes(self.proxy_sig, "utf-8"))
        self.template_data = raw

    def replace_worker_sig(self):
        raw = self.template_data
        placeholder = b"___FFFC_WORKER_SIG___"
        raw = raw.replace(placeholder, bytes(self.worker_sig, "utf-8"))
        self.template_data = raw

    def replace_inferred_header(self):
        raw = self.template_data
        placeholder = b"___FFFC_INFERRED_HEADER___"
        raw = raw.replace(placeholder, bytes(self.inferred_header_include, "utf-8"))
        self.template_data = raw

    def replace_call(self):
        raw = self.template_data
        placeholder = b"___FFFC_CALL___"
        if type(self.func.return_type) != dwarf_to_c.DwarfVoidType:
            tdecl = self.func.return_type.get_reference()("retval")
            init = self.func.call("FFFC_target")
            decl = c_ast.Decl("retval", [], [], [], tdecl, init, None)
            func_call = self.generator.visit(decl)
        else:
            func_call = self.generator.visit(self.func.call("FFFC_target"))
        raw = raw.replace(placeholder, bytes(func_call + ";", "utf-8"))
        self.template_data = raw

    def replace_proxy_call(self):
        raw = self.template_data
        placeholder = b"___FFFC_PROXY_CALL___"
        func_call = self.generator.visit(self.func.call("FFFC_proxy_target"))
        raw = raw.replace(placeholder, bytes(func_call + ";", "utf-8"))
        self.template_data = raw

    def replace_worker_call(self):
        raw = self.template_data
        placeholder = b"___FFFC_WORKER_CALL___"
        func_call = self.generator.visit(self.func.call("FFFC_worker_target"))
        raw = raw.replace(placeholder, bytes(func_call + ";", "utf-8"))
        self.template_data = raw

    def replace_target_decl(self):
        raw = self.template_data
        placeholder = b"___FFFC_TARGET_DECL___"
        # Now you need to declare a pointer to the function whose name is FFFC_replacement
        funcref = self.func.declare("FFFC_target")
        funcptr = c_ast.PtrDecl([], funcref)
        ast = c_ast.Decl("FFFC_target", [], [], [], funcptr, None, None)
        replacement = self.generator.visit(ast) + ";"
        raw = raw.replace(placeholder, bytes(replacement, "utf-8"))
        self.template_data = raw

    def replace_offset(self):
        offset = hex(self.func.low_pc)
        raw = self.template_data
        placeholder = b"___FFFC_OFFSET__"
        raw = raw.replace(placeholder, bytes(offset, "utf-8"))
        placeholder = b"___FFFC_RECALCULATE_OFFSET___"
        raw = raw.replace(placeholder, bytes(hex(self.pie), "utf-8"))
        self.template_data = raw

    def replace_return(self):
        raw = self.template_data
        placeholder = b"___FFFC_RETURN___"
        if type(self.func.return_type) != dwarf_to_c.DwarfVoidType:
            ret = c_ast.Return(expr=c_ast.ID("retval"))
            raw = raw.replace(placeholder, bytes(self.generator.visit(ret), "utf-8"))
        else:
            raw = raw.replace(placeholder, b"return;")
        self.template_data = raw

    def replace_argument_mutators(self):
        raw = self.template_data
        placeholder = b"___FFFC_ARGUMENT_MUTATORS___"
        mutators = []
        for arg in self.func.arguments:
            if type(arg) == c_ast.EllipsisParam:
                continue
            call = make_commented_mutator_call_from_var(arg.name, arg.type)
            call = "\n".join("\t" + line for line in call.splitlines())
            mutators.append(call)
        raw = raw.replace(placeholder, bytes("\n".join(mutators), "utf-8"))
        self.template_data = raw

    def replace_binary_path(self):
        bin_path = str(self.binary_path)
        exe_path = str(self.exe_path)
        if (bin_path == exe_path):
            # This deals with the weirdness of dl_iterate_phdr, which notates
            # the main executable as an empty string
            target_path = ""
        else:
            target_path = bin_path
        raw = self.template_data
        placeholder = b"___FFFC_BINARY_PATH__"
        raw = raw.replace(placeholder, bytes(target_path, "utf-8"))
        self.template_data = raw

    def _get_template_path(self):
        return str(Path("templates") / self.template_name)

    def inject(self):
        self.replace_target_decl()
        self.replace_inferred_header()
        self.replace_target_name()
        self.replace_hook_sig()
        self.replace_parallel_sig()
        self.replace_proxy_sig()
        self.replace_worker_sig()
        self.replace_call()
        self.replace_proxy_call()
        self.replace_worker_call()
        self.replace_return()
        self.replace_argument_mutators()
        self.replace_offset()
        self.replace_binary_path()
        return None, str(self.template_data, "utf-8")
