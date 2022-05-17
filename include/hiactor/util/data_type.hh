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

#include <hiactor/core/actor_message.hh>
#include <hiactor/net/serializable_queue.hh>

namespace hiactor {

template <typename Type>
struct SerializablePrimitive {
    SerializablePrimitive() = default;
    explicit SerializablePrimitive(Type v) : val(v) {}
    SerializablePrimitive(SerializablePrimitive&& x) noexcept = default;

    void dump_to(serializable_queue& qu) {
        auto buf = seastar::temporary_buffer<char>(sizeof(val));
        memcpy(buf.get_write(), &val, sizeof(val));
        qu.push(std::move(buf));
    }

    static SerializablePrimitive load_from(serializable_queue& qu) {
        auto new_val = *reinterpret_cast<const Type*>(qu.pop().get());
        return SerializablePrimitive{new_val};
    }
    Type val;
};

template <>
struct SerializablePrimitive<void> {
    SerializablePrimitive() = default;
    SerializablePrimitive(SerializablePrimitive&& x) = default;

    void dump_to(serializable_queue&) {}

    static SerializablePrimitive load_from(serializable_queue&) {
        return SerializablePrimitive{};
    }
};

using Integer = SerializablePrimitive<int>;
using Boolean = SerializablePrimitive<bool>;
using Void = SerializablePrimitive<void>;

struct simple_string {
    char* str{};
    size_t len{};

    simple_string() : str(nullptr), len(0) {}
    explicit simple_string(const char* c_str) : str(new char[strlen(c_str)]), len(strlen(c_str)) {
        memcpy(str, c_str, len);
    }
    explicit simple_string(const std::string& std_str) : simple_string(std_str.data(), std_str.size()) {}
    simple_string(const char* data, size_t len) : str(new char[len]), len(len) {
        memcpy(str, data, len);
    }
    simple_string(simple_string&& x) noexcept: str(x.str), len(x.len) {
        x.str = nullptr;
        x.len = 0;
    }
    simple_string(const simple_string& x) : simple_string(x.str, x.len) {}
    simple_string& operator=(simple_string&& x) noexcept {
        if (this != &x) {
            str = x.str;
            len = x.len;
            x.str = nullptr;
            x.len = 0;
        }
        return *this;
    }
    ~simple_string() { delete[] str; }

    static simple_string from_exception(const std::exception_ptr& ep = std::current_exception()) {
        if (!ep) { throw std::bad_exception(); }
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            return simple_string{e.what()};
        } catch (const std::string& e) {
            return simple_string{e};
        } catch (const char* e) {
            return simple_string{e};
        } catch (...) {
            return simple_string{"unknown exception!"};
        }
    }

    void dump_to(serializable_queue& qu) {
        auto buf = seastar::temporary_buffer<char>(len);
        if (str && len > 0) {
            memcpy(buf.get_write(), &str, len);
        }
        qu.push(std::move(buf));
    }

    static simple_string load_from(serializable_queue& qu) {
        auto q = qu.pop();
        return simple_string{q.get(), q.size()};
    }
};

} // namespace hiactor
