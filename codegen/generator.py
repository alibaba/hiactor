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

from utility import *


def get_license():
    return "/** Copyright 2021 Alibaba Group Holding Limited. All Rights Reserved.\n" \
           " * Licensed under the Apache License, Version 2.0 (the \"License\");\n" \
           " * you may not use this file except in compliance with the License.\n" \
           " * You may obtain a copy of the License at\n" \
           " *\n" \
           " * https://www.apache.org/licenses/LICENSE-2.0\n" \
           " *\n" \
           " * Unless required by applicable law or agreed to in writing, software\n" \
           " * distributed under the License is distributed on an \"AS IS\" BASIS,\n" \
           " * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n" \
           " * See the License for the specific language governing permissions and\n" \
           " * limitations under the License.\n" \
           " */\n\n"


def generate_auto_registration(class_name, name, type_id):
    return "hiactor::registration::actor_registration<{class_name}> _{name}_auto_registration({tid});\n".format(
        class_name=class_name, name=name, tid=type_id)


def generate_actg_type_getter_func(actg_info):
    return "\ntemplate <>\n" \
           "uint16_t hiactor::get_actor_group_type_id<{}>() {{\n" \
           "\treturn {};\n" \
           "}}\n".format(get_name_with_namespace(actg_info.name, actg_info.ns_list, "::"), actg_info.type_id)


def generate_actor_ref_class_def(act_info):
    method_decls = ""
    for method in act_info.methods:
        method_decls += "\t{vs}{return_type} {method_name}({args}){os}{end}\n".format(
            vs=method.get_virtual_specifier(),
            return_type=method.return_info.return_type,
            method_name=method.method_name,
            args=method.format_args(),
            os=method.get_override_specifier(),
            end=method.get_decl_end())
    base_ref_name = act_info.base_info.get_ref_name()
    ref_class_def = "class {ref_name} : public {base} {{\n" \
                    "public:\n" \
                    "\t{ref_name}();\n" \
                    "\t~{ref_name}() override = default;\n" \
                    "\t/// actor methods\n" \
                    "{methods}" \
                    "}};\n\n".format(ref_name=act_info.ref_name, methods=method_decls, base=base_ref_name)
    return ref_class_def


def generate_actor_method_enum(act_info):
    types = ""
    enum_id = 0
    for method in act_info.methods:
        types += "\t{} = {},\n".format(method.enum_type_name(), enum_id)
        enum_id += 1
    enum_def = "enum : uint8_t {{\n" \
               "{types}" \
               "}};\n\n".format(types=types)
    return enum_def


def generate_actor_ref_method_defs(act_info):
    construction_def = "{name}::{name}() : {base}() {{ actor_type = {actor_tid}; }}\n\n".format(
        name=act_info.ref_name, base=act_info.base_info.get_ref_name(), actor_tid=act_info.type_id)
    method_defs = []
    for method in act_info.methods:
        if method.is_pure_virtual:
            continue
        client_func_arguments = ["addr", method.enum_type_name()]
        optional_data_type = ""
        for arg in method.args:
            optional_data_type += ", {}".format(arg.no_ref_type)
            client_func_arguments.append("std::forward<{}>({})".format(arg.no_ref_type, arg.arg_name))
        if method.return_info.return_type == "void":
            actor_client_func = "hiactor::actor_client::send({args});".format(
                args=", ".join(client_func_arguments))
        else:
            actor_client_func = "return hiactor::actor_client::request<{result_type}{opt_data_type}>({args});".format(
                result_type=method.return_info.template_type,
                opt_data_type=optional_data_type,
                args=", ".join(client_func_arguments))
        method_def = "{return_type} {class_name}::{method_name}({method_args}) {{\n" \
                     "\taddr.set_method_actor_tid({actor_tid});\n" \
                     "\t{client_func}\n" \
                     "}}\n".format(return_type=method.return_info.return_type,
                                   class_name=act_info.ref_name,
                                   method_name=method.method_name,
                                   method_args=method.format_args(),
                                   actor_tid=act_info.type_id,
                                   client_func=actor_client_func)
        method_defs.append(method_def)
    return construction_def + "\n".join(method_defs) + "\n"


def generate_actor_do_work(act_info):
    base_action = ""
    base_info = act_info.base_info
    if not base_info.is_template:
        base_action = "\tif (__builtin_expect(msg->hdr.addr.get_method_actor_tid() != actor_type_id(), false)) {{\n" \
                      "\t\treturn {base}::do_work(msg);\n" \
                      "\t}}\n".format(base=base_info.get_actor_name())

    method_processors = ""
    for method in act_info.methods:
        if method.is_pure_virtual:
            continue
        if method.return_info.return_type != "void":
            return_specifier = "return "
            after_calling = ".then_wrapped([msg] (seastar::future<{result_type}> fut) {{\n" \
                            "\t\t\t\tif (__builtin_expect(fut.failed(), false)) {{\n" \
                            "\t\t\t\t\tauto* ex_msg = hiactor::make_response_message(\n" \
                            "\t\t\t\t\t\tmsg->hdr.src_shard_id,\n" \
                            "\t\t\t\t\t\thiactor::simple_string::from_exception(fut.get_exception()),\n" \
                            "\t\t\t\t\t\tmsg->hdr.pr_id,\n" \
                            "\t\t\t\t\t\thiactor::message_type::EXCEPTION_RESPONSE);\n" \
                            "\t\t\t\t\treturn hiactor::actor_engine().send(ex_msg);\n" \
                            "\t\t\t\t}}\n" \
                            "\t\t\t\tauto* response_msg = hiactor::make_response_message(\n" \
                            "\t\t\t\t\tmsg->hdr.src_shard_id, fut.get0(), " \
                            "msg->hdr.pr_id, hiactor::message_type::RESPONSE);\n" \
                            "\t\t\t\treturn hiactor::actor_engine().send(response_msg);\n" \
                            "\t\t\t}}).then([] {{\n" \
                            "\t\t\t\treturn seastar::make_ready_future<hiactor::stop_reaction>(" \
                            "hiactor::stop_reaction::no);\n" \
                            "\t\t\t}});\n".format(result_type=method.return_info.template_type)
        else:
            return_specifier = ""
            after_calling = ";\n\t\t\treturn seastar::make_ready_future<hiactor::stop_reaction>(" \
                            "hiactor::stop_reaction::no);\n"

        if len(method.args) > 0:
            calling = "\t\t\tstatic auto apply_{method} = [] (hiactor::actor_message *a_msg, {class_name} *self) {{\n" \
                      "\t\t\t\tif (!a_msg->hdr.from_network) {{\n" \
                      "\t\t\t\t\t{return_tag}self->{method}(std::move(reinterpret_cast<\n" \
                      "\t\t\t\t\t\thiactor::actor_message_with_payload<{data_type}>*>(a_msg)->data));\n" \
                      "\t\t\t\t}} else {{\n" \
                      "\t\t\t\t\tauto* ori_msg = reinterpret_cast<hiactor::actor_message_with_payload<\n" \
                      "\t\t\t\t\t\thiactor::serializable_queue>*>(a_msg);\n" \
                      "\t\t\t\t\t{return_tag}self->{method}({data_type}::load_from(ori_msg->data));\n" \
                      "\t\t\t\t}}\n" \
                      "\t\t\t}};\n" \
                      "\t\t\t{return_tag}apply_{method}(msg, this)" \
                .format(method=method.method_name,
                        class_name=act_info.name,
                        data_type=method.args[0].no_ref_type,
                        return_tag=return_specifier)
        else:
            calling = "\t\t\t{return_tag}{method}()".format(method=method.method_name, return_tag=return_specifier)

        method_processors += "\t\tcase {method_enum}: {{\n" \
                             "{stmt}" \
                             "\t\t}}\n".format(method_enum=method.enum_type_name(), stmt=calling + after_calling)
    method_processors += "\t\tdefault: {\n\t\t\treturn seastar::make_ready_future<hiactor::stop_reaction>(" \
                         "hiactor::stop_reaction::yes);\n\t\t}\n"

    do_work_def = "seastar::future<hiactor::stop_reaction> {class_name}::do_work(hiactor::actor_message* msg) {{\n" \
                  "{base_action}" \
                  "\tswitch (msg->hdr.behavior_tid) {{\n" \
                  "{processors}\t}}\n" \
                  "}}\n".format(class_name=act_info.name, base_action=base_action, processors=method_processors)
    return do_work_def
