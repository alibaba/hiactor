/** Copyright 2022 Alibaba Group Holding Limited. All Rights Reserved.
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

#include <unordered_map>

/// This file simulates a simple patient department to manage patient infos in our actor samples.

struct outpatient_info {
    std::string name;
    int registration_id;
    outpatient_info(std::string name, int registration_id)
        : name(std::move(name)), registration_id(std::move(registration_id)) {}
};

class outpatient_dp {
    std::unordered_map<unsigned, outpatient_info> _id_to_info = {
        {0, {"o-a", 2}},
        {1, {"o-b", 1}},
        {2, {"o-c", 5}},
        {3, {"o-d", 3}},
        {4, {"o-e", 6}},
        {5, {"o-f", 4}}
    };
    outpatient_dp() = default;
public:
    static outpatient_dp& get() {
        static outpatient_dp instance;
        return instance;
    }
    const outpatient_info& find(unsigned outpatient_id) const {
        return _id_to_info.at(outpatient_id);
    }
};

struct emergency_info {
    std::string name;
    // We use emergency level to describe the emergency of the emergency.
    // The higher the level, the more urgent demand for this emergency
    // under normal circumstances.
    unsigned emergency_level;
    emergency_info(std::string name, unsigned emergency_level)
        : name(std::move(name)), emergency_level(emergency_level) {}
};

class emergency_dp {
    std::unordered_map<unsigned, emergency_info> _id_to_info = {
        {0, {"e-a", 2}},
        {1, {"e-b", 1}},
        {2, {"e-c", 5}},
        {3, {"e-d", 3}},
        {4, {"e-e", 2}},
        {5, {"e-f", 4}}
    };
    emergency_dp() = default;
public:
    static emergency_dp& get() {
        static emergency_dp instance;
        return instance;
    }
    const emergency_info& find(unsigned emergency_id) const {
        return _id_to_info.at(emergency_id);
    }
};
