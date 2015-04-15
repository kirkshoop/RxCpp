// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_SOURCES_RX_RANGE_HPP)
#define RXCPP_SOURCES_RX_RANGE_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace sources {

namespace detail {

template<class T, class Scheduler>
struct range : public source_base<T>
{
    typedef rxu::decay_t<Scheduler> scheduler_type;

    struct range_initial_type
    {
        range_initial_type(T f, T l, ptrdiff_t s, scheduler_type sc)
            : next(f)
            , last(l)
            , step(s)
            , scheduler(std::move(sc))
        {
        }
        mutable T next;
        T last;
        ptrdiff_t step;
        scheduler_type scheduler;
    };

    template<class Out>
    struct range_state_type
    {
        range_state_type(range_initial_type i, Out o)
            : state(i)
            , dest(std::move(o))
        {
        }
        range_initial_type state;
        Out dest;

        template<class I>
        auto operator()(rxsc::worker<I>& ) -> rxsc::action_result {

            if (!dest.is_subscribed()) {
                // terminate loop
                return nullptr;
            }

            // send next value
            dest.on_next(state.next);
            if (!dest.is_subscribed()) {
                // terminate loop
                return nullptr;
            }

            if (std::abs(state.last - state.next) < std::abs(state.step)) {
                if (state.last != state.next) {
                    dest.on_next(state.last);
                }
                dest.on_completed();
                // o is unsubscribed
                return nullptr;
            }
            state.next = static_cast<T>(state.step + state.next);

            // tail recurse this same action to continue loop
            return rxsc::action_result();
        }
    };

    range_initial_type initial;

    range(T f, T l, ptrdiff_t s, scheduler_type sc)
        : initial(f, l, s, std::move(sc))
    {
    }

    template<class Subscriber>
    void on_subscribe(Subscriber o) const {
        static_assert(is_subscriber<Subscriber>::value, "subscribe must be passed a subscriber");

        // creates a worker whose lifetime is the same as this subscription
        auto worker = initial.scheduler.create_worker(o.get_subscription());

        //copy initial state
        auto state = range_state_type<Subscriber>(initial, std::move(o));

        worker.schedule(state);
    }
};

}

template<class T>
auto range(T first = 0, T last = std::numeric_limits<T>::max(), ptrdiff_t step = 1)
    ->      observable<T,   detail::range<T, rxsc::scheduler<rxsc::immediate>>> {
    return  observable<T,   detail::range<T, rxsc::scheduler<rxsc::immediate>>>(
                            detail::range<T, rxsc::scheduler<rxsc::immediate>>(first, last, step, rxsc::make_immediate()));
}
template<class T, class Scheduler>
auto range(T first, T last, ptrdiff_t step, Scheduler sc)
    ->      observable<T,   detail::range<T, Scheduler>> {
    return  observable<T,   detail::range<T, Scheduler>>(
                            detail::range<T, Scheduler>(first, last, step, std::move(sc)));
}
template<class T, class Scheduler>
auto range(T first, T last, Scheduler sc)
    -> typename std::enable_if<is_scheduler<Scheduler>::value,
            observable<T,   detail::range<T, Scheduler>>>::type {
    return  observable<T,   detail::range<T, Scheduler>>(
                            detail::range<T, Scheduler>(first, last, 1, std::move(sc)));
}
template<class T, class Scheduler>
auto range(T first, Scheduler sc)
    -> typename std::enable_if<is_scheduler<Scheduler>::value,
            observable<T,   detail::range<T, Scheduler>>>::type {
    return  observable<T,   detail::range<T, Scheduler>>(
                            detail::range<T, Scheduler>(first, std::numeric_limits<T>::max(), 1, std::move(sc)));
}

}

}

#endif
