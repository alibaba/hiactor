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

import subprocess
import sys
import os
import shutil
import argparse
import clang.cindex
from multiprocessing import cpu_count
from multiprocessing.pool import ThreadPool
from stat import S_IREAD, S_IRGRP, S_IROTH
from generator import *
from traverse import *
from typedef import *
from utility import *

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))


def process_actor_groups(process_args):
    p = ThreadPool(min(cpu_count(), len(process_args)))
    p.map(process_one_actor_group, process_args)
    p.close()
    p.join()


def process_one_actor_group(arg):
    print('Generating for %s ...' % arg.h_file)
    h_filepath = os.path.join(arg.actor_source_dir, arg.h_file)
    cc_filepath = os.path.join(arg.actor_source_dir, arg.autogen_cc_file)
    index = clang.cindex.Index.create()
    unit = index.parse(h_filepath, ['-nostdinc++', '-x', 'c++', '-std=gnu++14'] + arg.include_list)

    actg_list = []
    ns_list = []
    traverse_actor_group_types(unit.cursor, h_filepath, actg_list, ns_list)

    with open(cc_filepath, 'w') as fp:
        fp.write(get_license())
        fp.write('#include "%s"\n' % arg.h_file)
        fp.write('#include <hiactor/core/actor_factory.hh>\n')

        for actg_info in actg_list:
            fp.write(generate_actg_type_getter_func(actg_info))

        fp.write("\nnamespace auto_registration {\n\n")
        for actg_info in actg_list:
            class_name = get_name_with_namespace(actg_info.name, actg_info.ns_list, "::")
            var_name = get_name_with_namespace(actg_info.name, actg_info.ns_list, "_")
            fp.write(generate_auto_registration(class_name, var_name, actg_info.type_id))
        fp.write("\n} // namespace auto_registration\n")
    os.chmod(cc_filepath, S_IREAD | S_IRGRP | S_IROTH)


def process_actors(process_args):
    p = ThreadPool(min(cpu_count(), len(process_args)))
    p.map(process_one_actor, process_args)
    p.close()
    p.join()


def process_one_actor(arg):
    print('Generating for %s ...' % arg.h_file)
    h_filepath = os.path.join(arg.actor_source_dir, arg.h_file)
    ref_h_filepath = os.path.join(arg.actor_source_dir, arg.ref_h_file)
    cc_filepath = os.path.join(arg.actor_source_dir, arg.autogen_cc_file)
    index = clang.cindex.Index.create()
    unit = index.parse(h_filepath, ['-nostdinc++', '-x', 'c++', '-std=gnu++14'] + arg.include_list)

    act_list = []
    ns_list = []
    traverse_actor_types(unit.cursor, h_filepath, act_list, ns_list)

    # generate actor reference header
    with open(ref_h_filepath, 'w') as fp:
        fp.write(get_license())
        fp.write('#include "%s"\n' % arg.h_file)
        ref_includes = set()
        for act in act_list:
            if act.base_info.ref_include_path != "":
                ref_includes.add(act.base_info.ref_include_path)
        for ref_include in ref_includes:
            fp.write('#include "%s"\n' % ref_include)
        for act in act_list:
            ref_class_def = generate_actor_ref_class_def(act)
            write_defs_with_namespace(fp, act.ns_list, ref_class_def)
    os.chmod(ref_h_filepath, S_IREAD | S_IRGRP | S_IROTH)

    # generate actor reference method impls
    with open(cc_filepath, 'w') as fp:
        fp.write(get_license())
        fp.write('#include "%s"\n' % arg.ref_h_file)
        fp.write('#include <hiactor/core/actor_client.hh>\n')
        fp.write('#include <hiactor/core/actor_factory.hh>\n')
        for act in act_list:
            actor_method_enum = generate_actor_method_enum(act)
            ref_methods_def = generate_actor_ref_method_defs(act)
            do_work_def = generate_actor_do_work(act)
            write_defs_with_namespace(fp, act.ns_list, actor_method_enum + ref_methods_def + do_work_def)
        fp.write("\nnamespace auto_registration {\n\n")
        for act in act_list:
            class_name = get_name_with_namespace(act.name, act.ns_list, "::")
            var_name = get_name_with_namespace(act.name, act.ns_list, "_")
            fp.write(generate_auto_registration(class_name, var_name, act.type_id))
        fp.write("\n} // namespace auto_registration\n")
    os.chmod(cc_filepath, S_IREAD | S_IRGRP | S_IROTH)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Actor codegen tool.')
    parser.add_argument('--actor-source-dir', action="store", dest="actor_source_dir",
                        help="The CXX actor source root dir of your project.")
    parser.add_argument('--hiactor-include', action="store", dest="hiactor_include",
                        help="Path of hiactor include/ folder to search for headers")
    parser.add_argument('--system-include', action="store", dest="sys_include", default="",
                        help="Semicolon-separated list of paths of other system folder to search for headers.")
    parser.add_argument('--user-include', action="store", dest="user_include", default="",
                        help="Semicolon-separated list of paths of user-defined folder from your project to search "
                             "for headers.")
    args = parser.parse_args()

    includes = ['-isystem', args.hiactor_include]
    if len(args.sys_include) > 0:
        for path in args.sys_include.split(';'):
            includes.append('-isystem')
            includes.append(path)
    if len(args.user_include) > 0:
        for path in args.user_include.split(';'):
            includes.append('-I%s' % path)

    status, output = subprocess.getstatusoutput("echo | g++ -xc++ -E -Wp,-v -")
    start_idx = output.rfind("search starts here:")
    end_idx = output.rfind("End of search list")
    paths = output[start_idx + 19: end_idx].replace("\n", " ").split(" ")
    for gxx_inc in paths:
        if gxx_inc != '' and gxx_inc != '\n':
            includes.append('-I%s' % gxx_inc)

    autogen_dir = os.path.join(args.actor_source_dir, "generated")
    if os.path.exists(autogen_dir):
        shutil.rmtree(autogen_dir)
    os.mkdir(autogen_dir)

    actg_process_args = []
    act_process_args = []

    for a_dir, _, _ in os.walk(args.actor_source_dir):
        for f in os.listdir(a_dir):
            if f.endswith(".actg.h"):
                actg_name = os.path.basename(f).split(".")[0]
                actg_h = os.path.relpath(os.path.join(a_dir, f), args.actor_source_dir)
                actg_autogen_cc = os.path.join("generated", actg_name + ".actg.autogen.cc")
                actg_process_args.append(ActorGroupProcessArgument(
                    args.actor_source_dir, actg_h, actg_autogen_cc, includes))
            if f.endswith(".act.h"):
                act_name = os.path.basename(f).split(".")[0]
                act_h = os.path.relpath(os.path.join(a_dir, f), args.actor_source_dir)
                act_ref_h = os.path.join("generated", act_name + "_ref.act.autogen.h")
                act_autogen_cc = os.path.join("generated", act_name + ".act.autogen.cc")
                act_process_args.append(ActorProcessArgument(
                    args.actor_source_dir, act_h, act_ref_h, act_autogen_cc, includes))

    def get_key(item):
        return item.h_file
    actg_process_args = sorted(actg_process_args, key=get_key)
    act_process_args = sorted(act_process_args, key=get_key)

    if len(actg_process_args) > 0:
        process_actor_groups(actg_process_args)
    if len(act_process_args) > 0:
        process_actors(act_process_args)
