#pragma once

#include <hiactor/core/actor-template.hh>
#include <hiactor/util/data_type.hh>

#include "snack.act.h"

namespace food {
namespace kfc {

namespace snack_ns = snack;
using snack_t = snack_ns::snack;

class ANNOTATION(actor:impl) kfc : public snack_t {
public:
    kfc(hiactor::actor_base* exec_ctx, const hiactor::byte_t* addr): snack(exec_ctx, addr) {
        set_max_concurrency(100);
    }
    ~kfc() override = default;

    virtual seastar::future<hiactor::Integer> ANNOTATION(actor:method) send(hiactor::Integer&& num) {
        return seastar::make_ready_future<hiactor::Integer>(_stock);
    }
    void ANNOTATION(actor:method) test() {}
    void ANNOTATION(actor:method) eat() final {}

    /// Declare `do_work` func here, no need to implement.
    ACTOR_DO_WORK()
};

}
}