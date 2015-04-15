// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_SCHEDULER_IMMEDIATE_HPP)
#define RXCPP_RX_SCHEDULER_IMMEDIATE_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace schedulers {

struct immediate
{
    typedef scheduler_base::clock_type clock_type;
private:
    struct immediate_worker
    {
        typedef scheduler_base::clock_type clock_type;
    private:
        mutable composite_subscription cs;
    public:
        explicit immediate_worker(composite_subscription cs)
            : cs(std::move(cs))
        {
        }

        clock_type::time_point now() const {
            return clock_type::now();
        }

        composite_subscription& get_subscription() const {
            return cs;
        }

        template<class F>
        void schedule(action<F> act) const {
            action_result r;
            auto w = make_worker(*this);
            while (cs.is_subscribed() && r.verb != action_verb::exit) {
                r = act(w);
                if (r.verb == action_verb::repeat_when) {
                    std::this_thread::sleep_until(r.when);
                }
            }
        }

        template<class F>
        void schedule(clock_type::time_point when, action<F> act) const {
            action_result r(when);
            auto w = make_worker(*this);
            while (cs.is_subscribed() && r.verb != action_verb::exit) {
                if (r.verb == action_verb::repeat_when) {
                    std::this_thread::sleep_until(r.when);
                }
                r = act(w);
            }
        }
    };

public:

    clock_type::time_point now() const {
        return clock_type::now();
    }

    auto create_worker(composite_subscription cs) const -> worker<immediate_worker> {
        return make_worker(immediate_worker(std::move(cs)));
    }
};

inline auto make_immediate() -> scheduler<immediate> {
    return scheduler<immediate>(immediate());
}

}

}

#endif
