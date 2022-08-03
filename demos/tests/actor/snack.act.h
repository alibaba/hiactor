#pragma once

#include <hiactor/core/actor-template.hh>
#include <hiactor/util/data_type.hh>

#include "food.act.h"

namespace food::snack {

class ANNOTATION(actor:impl) snack : public food {
public:
    snack(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr): food(exec_ctx, addr) {
        set_max_concurrency(1);
    }
    ~snack() override = default;

    seastar::future<hiactor::Integer> ANNOTATION(actor:method) buy(hiactor::Integer&& num) override;
    virtual void ANNOTATION(actor:method) eat() {}

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

}