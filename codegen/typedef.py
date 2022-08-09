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

class ActorGroupProcessArgument:
    def __init__(self, filepath, source_dir, rel_dir, autogen_cc_file, include_list):
        self.filepath = filepath
        self.source_dir = source_dir
        self.rel_dir = rel_dir
        self.autogen_cc_file = autogen_cc_file
        self.include_list = include_list


class ActorGroupCodeGenInfo:
    def __init__(self, name, ns_list, type_id):
        self.name = name
        self.ns_list = ns_list
        self.type_id = type_id


class ActorProcessArgument:
    def __init__(self, filepath, source_dir, rel_dir, autogen_ref_h_file, autogen_cc_file, include_list):
        self.filepath = filepath
        self.source_dir = source_dir
        self.rel_dir = rel_dir
        self.autogen_ref_h_file = autogen_ref_h_file
        self.autogen_cc_file = autogen_cc_file
        self.include_list = include_list


class ActorMethodReturnInfo:
    def __init__(self, return_type, template_type):
        self.return_type = return_type
        self.template_type = template_type


class ActorMethodArgInfo:
    def __init__(self, arg_type, arg_name, no_ref_type):
        self.arg_type = arg_type
        self.arg_name = arg_name
        self.no_ref_type = no_ref_type


class ActorMethodInfo:
    def __init__(self, class_name, method_name, return_info, args,
                 is_virtual, is_pure_virtual, override_specifier):
        self.class_name = class_name
        self.method_name = method_name
        self.return_info = return_info
        self.args = args
        self.is_virtual = is_virtual
        self.is_pure_virtual = is_pure_virtual
        self.override_specifier = override_specifier

    def format_args(self):
        return ", ".join(["{type}{name}".format(type=arg.arg_type, name=arg.arg_name) for arg in self.args])

    def enum_type_name(self):
        return "k_{}_{}".format(self.class_name, self.method_name)

    def get_virtual_specifier(self):
        return "virtual " if self.is_virtual else ""

    def get_override_specifier(self):
        return " {}".format(self.override_specifier) if self.override_specifier != "" else ""

    def get_decl_end(self):
        return " = 0;" if self.is_pure_virtual else ";"


class BaseActorInfo:
    def __init__(self, type_name, is_template, location_file):
        self.is_template = is_template
        self.type_name = "::" + type_name
        self.location_file = location_file

    def get_actor_name(self):
        if self.is_template:
            return "::hiactor::actor"
        else:
            return self.type_name

    def get_ref_name(self):
        if self.is_template:
            return "::hiactor::reference_base"
        else:
            return self.type_name + "_ref"


class ActorCodeGenInfo:
    def __init__(self, name, ns_list, type_id, methods, base_info):
        self.name = name
        self.ref_name = name + "_ref"
        self.ns_list = ns_list
        self.type_id = type_id
        self.methods = methods
        self.base_info = base_info

    def is_abstract(self):
        for method in self.methods:
            if method.is_pure_virtual:
                return True
        return False
