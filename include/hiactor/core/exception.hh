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

#include <exception>

namespace hiactor {

class actor_method_exception : public std::exception {
    std::string _context;
public:
    explicit actor_method_exception(const char* ctx)
        : std::exception(), _context(ctx) {}

    actor_method_exception(const char* ctx_str, size_t len)
        : std::exception(), _context(ctx_str, len) {}

    ~actor_method_exception() override = default;

    const char* what() const noexcept override {
        return _context.c_str();
    }
};

class task_canceled_exception : public std::exception {
    const char* _context;
public:
    explicit task_canceled_exception(const char* ctx)
        : std::exception(), _context(ctx) {}

    ~task_canceled_exception() override = default;

    const char* what() const noexcept override {
        return _context;
    }
};

class gpu_exception : public std::exception {
    const char* _context;
public:
    explicit gpu_exception(const char* ctx)
        : std::exception(), _context(ctx) {}

    ~gpu_exception() override = default;

    const char* what() const noexcept override {
        return _context;
    }
};

} // namespace hiactor
