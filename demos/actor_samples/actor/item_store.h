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

#include <unordered_map>

/// This file simulates a simple item store to manage item infos in our actor samples.

struct food_info {
    std::string name;
    std::string production_date;
    food_info(std::string name, std::string production_date)
        : name(std::move(name)), production_date(std::move(production_date)) {}
};

class food_store {
    std::unordered_map<unsigned, food_info> _id_to_info = {
        {0, {"Apple", "2021.04.03"}},
        {1, {"Cake", "2021.07.23"}},
        {2, {"Ham", "2019.09.17"}},
        {3, {"Beer", "2020.12.11"}},
        {4, {"Steak", "2021.10.18"}},
        {5, {"Juice", "2021.06.05"}}
    };
    food_store() = default;
public:
    static food_store& get() {
        static food_store instance;
        return instance;
    }
    const food_info& find(unsigned food_id) const {
        return _id_to_info.at(food_id);
    }
};

struct medicine_info {
    std::string name;
    // We use urgency level to describe the urgency of the medicine.
    // The higher the level, the more urgent demand for this medicine
    // under normal circumstances.
    unsigned urgency_level;
    medicine_info(std::string name, unsigned urgency_level)
        : name(std::move(name)), urgency_level(urgency_level) {}
};

class medicine_store {
    std::unordered_map<unsigned, medicine_info> _id_to_info = {
        {0, {"Amoxicillin", 2}},
        {1, {"Vitamin C", 1}},
        {2, {"Heart Reliever", 5}},
        {3, {"Band Aid", 3}},
        {4, {"Disinfectant", 2}},
        {5, {"Antihistamine", 4}}
    };
    medicine_store() = default;
public:
    static medicine_store& get() {
        static medicine_store instance;
        return instance;
    }
    const medicine_info& find(unsigned medicine_id) const {
        return _id_to_info.at(medicine_id);
    }
};
