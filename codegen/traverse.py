# Copyright 2021 Alibaba Group Holding Limited. All Rights Reserved.

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import threading
from clang.cindex import AccessSpecifier, CursorKind, TypeKind
from typedef import *
from utility import *

global_actg_tid = 0xF002
actg_tid_mutex = threading.Lock()

global_act_tid = 0
act_tid_mutex = threading.Lock()


def check_attribute(node, attr):
    for child in node.get_children():
        if child.kind == CursorKind.ANNOTATE_ATTR and child.spelling == attr:
            return True
    return False


def is_derived_from_template_actor_group(node):
    base_class_name = ["actor_group", "schedulable_actor_group"]
    if node.spelling in base_class_name:
        return True
    base_ptr = None
    is_valid = False
    for child in node.get_children():
        if child.kind == CursorKind.CXX_BASE_SPECIFIER:
            base_ptr = child.get_definition()
            if base_ptr.spelling in base_class_name:
                is_valid = True
            break
    if not is_valid and base_ptr is not None:
        return is_derived_from_template_actor_group(base_ptr)
    return is_valid


def is_template_actor_type(node):
    return node.spelling in ["reentrant_actor", "stateful_actor", "stateless_actor"]


def traverse_actor_group_types(node, filepath, actg_list, ns_list):
    global global_actg_tid
    if node.kind in [CursorKind.CLASS_DECL, CursorKind.CLASS_TEMPLATE, CursorKind.STRUCT_DECL]:
        if check_attribute(node, "actor:group"):
            if not is_derived_from_template_actor_group(node):
                raise RuntimeError("An customized actor group should be derived from template actor "
                                   "group types, error in {}\n".format(node.spelling))
            actg_tid_mutex.acquire()
            tid = global_actg_tid
            global_actg_tid += 1
            actg_tid_mutex.release()
            actg_list.append(ActorGroupCodeGenInfo(node.spelling, tuple(ns_list), tid))
    if node.kind in [CursorKind.TRANSLATION_UNIT, CursorKind.NAMESPACE]:
        if node.kind == CursorKind.NAMESPACE:
            ns_list.append(node.spelling)
        for child in node.get_children():
            if child.location.file.name == filepath:
                traverse_actor_group_types(child, filepath, actg_list, ns_list)
        if node.kind == CursorKind.NAMESPACE:
            ns_list.pop()


def check_and_parse_actor_method(class_name, node):
    if not node.access_specifier == AccessSpecifier.PUBLIC:
        raise RuntimeError("Method {} in actor class {} must be public!".format(node.spelling, class_name))

    return_type = node.result_type
    return_template_type = ""
    if return_type.spelling != "void":
        if not return_type.get_canonical().spelling.startswith("seastar::future"):
            raise RuntimeError("The return type {} of method {} in actor class {} should be void or seastar::future!"
                               .format(return_type.spelling, node.spelling, class_name))
        return_template_type = return_type.get_template_argument_type(0).spelling
    return_info = ActorMethodReturnInfo(return_type.spelling, return_template_type)

    arguments = []
    for arg in node.get_arguments():
        if not arg.type.kind == TypeKind.RVALUEREFERENCE:
            raise RuntimeError("Argument {} of method {} in actor class {} should be a rvalue-reference!"
                               .format(arg.spelling, node.spelling, class_name))
        arguments.append(ActorMethodArgInfo(arg.type.spelling, arg.spelling, get_type_ref_from_method_arg(arg)))
    if len(arguments) > 1:
        raise RuntimeError("Method {} in actor class {} should not contains more than one argument!"
                           .format(node.spelling, class_name))

    return ActorMethodInfo(class_name, node.spelling, return_info, arguments)


def traverse_actor_methods(node):
    derive_from_template = False
    frontiers = [node]
    methods = []
    method_names = []
    while len(frontiers) > 0:
        next_frontiers = []
        for front_node in frontiers:
            for child in front_node.get_children():
                if child.kind == CursorKind.CXX_METHOD and check_attribute(child, "actor:method"):
                    if child.spelling not in method_names and not child.is_pure_virtual_method():
                        methods.append(check_and_parse_actor_method(node.spelling, child))
                        method_names.append(child.spelling)
                elif child.kind == CursorKind.CXX_BASE_SPECIFIER:
                    if is_template_actor_type(child.get_definition()):
                        derive_from_template = True
                    else:
                        next_frontiers.append(child.get_definition())
        frontiers = next_frontiers
    if not derive_from_template:
        raise RuntimeError("Class {} is not derived from an template actor\n".format(node.spelling))

    def get_key(method):
        return method.method_name
    return sorted(methods, key=get_key)


def traverse_actor_types(node, filepath, act_list, ns_list):
    global global_act_tid
    if node.kind in [CursorKind.CLASS_DECL, CursorKind.CLASS_TEMPLATE, CursorKind.STRUCT_DECL]:
        if check_attribute(node, "actor:impl"):
            act_tid_mutex.acquire()
            tid = global_act_tid
            global_act_tid += 1
            act_tid_mutex.release()
            methods = traverse_actor_methods(node)
            act_list.append(ActorCodeGenInfo(node.spelling, tuple(ns_list), tid, methods))
    if node.kind in [CursorKind.TRANSLATION_UNIT, CursorKind.NAMESPACE]:
        if node.kind == CursorKind.NAMESPACE:
            ns_list.append(node.spelling)
        for child in node.get_children():
            if child.location.file.name == filepath:
                traverse_actor_types(child, filepath, act_list, ns_list)
        if node.kind == CursorKind.NAMESPACE:
            ns_list.pop()


def get_type_ref_from_method_arg(arg_node):
    for child in arg_node.get_children():
        if child.kind == CursorKind.TYPE_REF:
            return remove_class_prefix(child.spelling)
    return None
