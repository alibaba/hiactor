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
    def __init__(self, actor_source_dir, h_file, autogen_cc_file, include_list):
        self.actor_source_dir = actor_source_dir
        self.h_file = h_file
        self.autogen_cc_file = autogen_cc_file
        self.include_list = include_list


class ActorGroupCodeGenInfo:
    def __init__(self, name, ns_list, type_id):
        self.name = name
        self.ns_list = ns_list
        self.type_id = type_id


class ActorProcessArgument:
    def __init__(self, actor_source_dir, h_file, ref_h_file, autogen_cc_file, include_list):
        self.actor_source_dir = actor_source_dir
        self.h_file = h_file
        self.ref_h_file = ref_h_file
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
    def __init__(self, class_name, method_name, return_info, args):
        self.class_name = class_name
        self.method_name = method_name
        self.return_info = return_info
        self.args = args

    def format_args(self):
        return ", ".join(["{type} {name}".format(type=arg.arg_type, name=arg.arg_name) for arg in self.args])

    def enum_type_name(self):
        return "k_{}_{}".format(self.class_name, self.method_name)


class ActorCodeGenInfo:
    def __init__(self, name, ns_list, type_id, methods):
        self.name = name
        self.ref_name = name + "_ref"
        self.ns_list = ns_list
        self.type_id = type_id
        self.methods = methods
