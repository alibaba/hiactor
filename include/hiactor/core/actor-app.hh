/** Copyright 2021 Alibaba Group Holding Limited. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <hiactor/core/root_actor_group.hh>
#include <seastar/core/app-template.hh>

namespace hiactor {

class actor_smp {
    static std::vector<root_actor_group*> _root_actor_groups;
public:
    static seastar::future<> configure(const boost::program_options::variables_map& vm);
    static void cleanup_cpu();
private:
    static void start_all_queues();
};

class actor_app {
    seastar::app_template _app;
public:
    explicit actor_app(seastar::app_template::config cfg = seastar::app_template::config())
        : _app(std::move(cfg)) {}
    int run(int ac, char** av, std::function<void()>&& func);
    seastar::alien::instance& alien() { return _app.alien(); }
private:
    void add_actor_options();
    seastar::future<> start_network();
    static void set_network_config(uint32_t this_mid, uint32_t conn_count);
};

} // namespace hiactor
