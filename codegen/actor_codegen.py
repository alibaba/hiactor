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

def check_diagnostics_and_print(unit):
    if unit.diagnostics:
        parse_success = False
        for diag in unit.diagnostics:
            if diag.severity > clang.cindex.Diagnostic.Warning:
                print(f"Error: {diag.spelling}")
                print(f"  File: {diag.location.file.name}")
                print(f"  Line: {diag.location.line}")
                print(f"  Column: {diag.location.column}")
                print(f"  Severity: {diag.severity}")
    else:
        parse_success = True
    return parse_success

def process_one_actor_group(arg: ActorGroupProcessArgument):
    include_path = os.path.relpath(arg.filepath, arg.source_dir)
    print('Generating for %s ...' % include_path)

    index = clang.cindex.Index.create()
    unit = index.parse(arg.filepath, ['-nostdinc++', '-x', 'c++', '-std=gnu++17'] + arg.include_list)

    if not check_diagnostics_and_print(unit):
        raise Exception("Failed to parse %s" % arg.filepath)

    actg_list = []
    ns_list = []
    traverse_actor_group_types(unit.cursor, arg.filepath, actg_list, ns_list)

    cur_autogen_dir = os.path.join(arg.source_dir, "generated", arg.rel_dir)
    if not os.path.exists(cur_autogen_dir):
        os.mkdir(cur_autogen_dir)
    autogen_cc_filepath = os.path.join(cur_autogen_dir, arg.autogen_cc_file)
    with open(autogen_cc_filepath, 'w') as fp:
        fp.write(get_license())
        fp.write('#include "%s"\n' % include_path)
        fp.write('#include <hiactor/core/actor_factory.hh>\n')

        for actg_info in actg_list:
            fp.write(generate_actg_type_getter_func(actg_info))

        fp.write("\nnamespace auto_registration {\n\n")
        for actg_info in actg_list:
            class_name = get_name_with_namespace(actg_info.name, actg_info.ns_list, "::")
            var_name = get_name_with_namespace(actg_info.name, actg_info.ns_list, "_")
            fp.write(generate_auto_registration(class_name, var_name, actg_info.type_id))
        fp.write("\n} // namespace auto_registration\n")
    os.chmod(autogen_cc_filepath, S_IREAD | S_IRGRP | S_IROTH)


def process_actors(process_args):
    p = ThreadPool(min(cpu_count(), len(process_args)))
    p.map(process_one_actor, process_args)
    p.close()
    p.join()


def process_one_actor(arg: ActorProcessArgument):
    include_path = os.path.relpath(arg.filepath, arg.source_dir)
    print('Generating for %s ...' % include_path)

    index = clang.cindex.Index.create()
    unit = index.parse(arg.filepath, ['-nostdinc++', '-x', 'c++', '-std=gnu++17'] + arg.include_list)

    if not check_diagnostics_and_print(unit):
        raise Exception("Failed to parse %s" % arg.filepath)

    act_list = []
    ns_list = []
    traverse_actor_types(unit.cursor, arg.filepath, act_list, ns_list)

    cur_autogen_dir = os.path.join(arg.source_dir, "generated", arg.rel_dir)
    if not os.path.exists(cur_autogen_dir):
        os.mkdir(cur_autogen_dir)

    # generate actor reference header
    autogen_ref_h_filepath = os.path.join(cur_autogen_dir, arg.autogen_ref_h_file)
    with open(autogen_ref_h_filepath, 'w') as fp:
        fp.write(get_license())
        fp.write('#pragma once\n\n')
        fp.write('#include "%s"\n' % include_path)
        ref_includes = set()
        for act in act_list:
            if act.base_info.location_file != "":
                base_name = os.path.basename(act.base_info.location_file).split(".")[0]
                base_rel_dir = os.path.dirname(os.path.relpath(act.base_info.location_file, arg.source_dir))
                ref_includes.add(os.path.join("generated", base_rel_dir, base_name + "_ref.act.autogen.h"))
        for ref_include in ref_includes:
            fp.write('#include "%s"\n' % ref_include)
        for act in act_list:
            ref_class_def = generate_actor_ref_class_def(act)
            write_defs_with_namespace(fp, act.ns_list, ref_class_def)
    os.chmod(autogen_ref_h_filepath, S_IREAD | S_IRGRP | S_IROTH)

    # generate actor reference method impls
    autogen_cc_filepath = os.path.join(cur_autogen_dir, arg.autogen_cc_file)
    with open(autogen_cc_filepath, 'w') as fp:
        fp.write(get_license())
        fp.write('#include "%s"\n' % os.path.join("generated", arg.rel_dir, arg.autogen_ref_h_file))
        fp.write('#include <hiactor/core/actor_client.hh>\n')
        fp.write('#include <hiactor/core/actor_factory.hh>\n')
        fp.write('#include <hiactor/util/data_type.hh>\n')
        for act in act_list:
            actor_method_enum = generate_actor_method_enum(act)
            ref_methods_def = generate_actor_ref_method_defs(act)
            do_work_def = generate_actor_do_work(act)
            write_defs_with_namespace(fp, act.ns_list, actor_method_enum + ref_methods_def + do_work_def)
        fp.write("\nnamespace auto_registration {\n\n")
        for act in act_list:
            if not act.is_abstract():
                class_name = get_name_with_namespace(act.name, act.ns_list, "::")
                var_name = get_name_with_namespace(act.name, act.ns_list, "_")
                fp.write(generate_auto_registration(class_name, var_name, act.type_id))
        fp.write("\n} // namespace auto_registration\n")
    os.chmod(autogen_cc_filepath, S_IREAD | S_IRGRP | S_IROTH)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Actor codegen tool.')
    parser.add_argument('--source-dir', action="store", dest="source_dir",
                        help="The source dir of your actor files.")
    parser.add_argument('--include-paths', action="store", dest="include_paths", default="",
                        help="Comma-separated list of paths of folders to search for headers.")
    args = parser.parse_args()

    includes = []
    if len(args.include_paths) > 0:
        for path in args.include_paths.split(','):
            includes.append('-I%s' % path)

    status, output = subprocess.getstatusoutput("echo | g++ -xc++ -E -Wp,-v -")
    start_idx = output.rfind("search starts here:")
    end_idx = output.rfind("End of search list")
    paths = output[start_idx + 19: end_idx].replace("\n", " ").split(" ")
    for gxx_inc in paths:
        if gxx_inc != '' and gxx_inc != '\n':
            includes.append('-I%s' % gxx_inc)

    autogen_dir = os.path.join(args.source_dir, "generated")
    if os.path.exists(autogen_dir):
        shutil.rmtree(autogen_dir)
    os.mkdir(autogen_dir)

    actg_process_args = []
    act_process_args = []

    for a_dir, _, _ in os.walk(args.source_dir):
        for f in os.listdir(a_dir):
            if f.endswith(".actg.h"):
                actg_name = os.path.basename(f).split(".")[0]
                actg_autogen_cc_file = actg_name + ".actg.autogen.cc"

                actg_filepath = os.path.join(a_dir, f)
                actg_rel_dir = os.path.dirname(os.path.relpath(actg_filepath, args.source_dir))

                actg_process_args.append(ActorGroupProcessArgument(
                    actg_filepath, args.source_dir, actg_rel_dir, actg_autogen_cc_file, includes))
            if f.endswith(".act.h"):
                act_name = os.path.basename(f).split(".")[0]
                act_autogen_ref_h_file = act_name + "_ref.act.autogen.h"
                act_autogen_cc_file = act_name + ".act.autogen.cc"

                act_filepath = os.path.join(a_dir, f)
                act_rel_dir = os.path.dirname(os.path.relpath(act_filepath, args.source_dir))

                act_process_args.append(ActorProcessArgument(
                    act_filepath, args.source_dir, act_rel_dir, act_autogen_ref_h_file, act_autogen_cc_file, includes))

    if len(actg_process_args) > 0:
        process_actor_groups(actg_process_args)
    if len(act_process_args) > 0:
        process_actors(act_process_args)
