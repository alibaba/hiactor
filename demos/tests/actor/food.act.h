#pragma once

#include <hiactor/core/actor-template.hh>
#include <hiactor/util/data_type.hh>

namespace hc = hiactor;

namespace food {

class ANNOTATION(actor:impl) food : public hc::actor {
protected:
    unsigned _item_id;
    int _stock = 10; // init stock of 10.
public:
    food(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr)
        : hiactor::actor(exec_ctx, addr, true) {
        _item_id = actor_id();
    }
    ~food() override = default;

    virtual seastar::future<hiactor::Integer> ANNOTATION(actor:method) buy(hiactor::Integer&& num) = 0;

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

}