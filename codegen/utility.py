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


def write_defs_with_namespace(fp, ns_list, def_stmts):
    if len(ns_list) > 0:
        fp.write("\n")
        ns_starting = "\n".join(
            ["namespace {ns} {{".format(ns=nsn) for nsn in ns_list])
        fp.write(ns_starting)
        fp.write("\n\n")
    fp.write(def_stmts)
    if len(ns_list) > 0:
        ns_ending = "\n".join(
            ["}} // namespace {ns}".format(ns=nsn) for nsn in ns_list])
        fp.write(ns_ending)
        fp.write("\n")


def get_name_with_namespace(name, ns_list, delim):
    if len(ns_list) > 0:
        return delim.join(ns_list) + delim + name
    return name


def remove_class_prefix(name):
    if name.startswith("class "):
        return name[6:]
    elif name.startswith("struct "):
        return name[7:]
    return name
