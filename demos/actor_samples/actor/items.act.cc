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

#include "actor/items.act.h"
#include "actor/item_store.h"
#include <seastar/core/print.hh>

namespace item {

seastar::future<hiactor::Integer> generic_item::buy(hiactor::Integer&& num) {
    if (num.val > _stock) {
        return seastar::make_exception_future<hiactor::Integer>(std::make_exception_ptr(std::runtime_error(
            seastar::format("stock is not enough, request: {}, remaining: {}", num.val, _stock))));
    } else {
        _stock -= num.val;
    }
    return seastar::make_ready_future<hiactor::Integer>(_stock);
}

namespace food {

food_item::food_item(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
    : generic_item(exec_ctx, addr) {
    // init the food item with its metadata
    add_meta("food", food_store::get().find(_item_id));
}

void food_item::print_info() {
    auto* info = get_meta_ptr<food_info>("food");
    fmt::print("[Shard: {}][Food: id({}), name({}), production_data({}), stock({})]\n",
               hiactor::local_shard_id(), _item_id, info->name, info->production_date, _stock);
}

} // namespace food

namespace medicine {

medicine_item::medicine_item(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
    : generic_item(exec_ctx, addr) {
    // init the medicine item with its metadata
    add_meta("medicine", medicine_store::get().find(_item_id));
}

void medicine_item::print_info() {
    auto* info = get_meta_ptr<medicine_info>("medicine");
    fmt::print("[Shard: {}][Medicine: id({}), name({}), urgency_level({}), stock({})]\n",
               hiactor::local_shard_id(), _item_id, info->name, info->urgency_level, _stock);
}

} // namespace medicine

} // namespace item
