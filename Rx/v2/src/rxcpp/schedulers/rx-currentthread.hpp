// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_SCHEDULER_CURRENT_THREAD_HPP)
#define RXCPP_RX_SCHEDULER_CURRENT_THREAD_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace schedulers {

struct current_thread : public scheduler_interface
{
private:
    typedef current_thread this_type;
    current_thread(const this_type&);

    typedef detail::action_queue queue;

    struct derecurser : public worker_interface
    {
    private:
        typedef current_thread this_type;
        derecurser(const this_type&);
    public:
        derecurser()
        {
        }
        virtual ~derecurser()
        {
        }

        virtual clock_type::time_point now() const {
            return clock_type::now();
        }

        virtual void schedule(const schedulable& scbl) const {
            queue::push(queue::item_type(now(), scbl));
        }

        virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
            queue::push(queue::item_type(when, scbl));
        }
    };

    struct current_worker : public worker_interface
    {
    private:
        typedef current_thread this_type;
        current_worker(const this_type&);
    public:
        current_worker()
        {
        }
        virtual ~current_worker()
        {
        }

        virtual clock_type::time_point now() const {
            return clock_type::now();
        }

        virtual void schedule(const schedulable& scbl) const {
            schedule(now(), scbl);
        }

        virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
            if (!scbl.is_subscribed()) {
                return;
            }

            {
                // check ownership
                if (queue::owned()) {
                    // already has an owner - delegate
                    queue::get_worker_interface()->schedule(when, scbl);
                    return;
                }

                // take ownership
                queue::ensure(std::make_shared<derecurser>());
            }
            // release ownership
            RXCPP_UNWIND_AUTO([]{
                queue::destroy();
            });

            const auto& recursor = queue::get_recursion().get_recurse();
            std::this_thread::sleep_until(when);
            if (scbl.is_subscribed()) {
                scbl(recursor);
            }
            if (queue::empty()) {
                return;
            }

            // loop until queue is empty
            for (
                auto next = queue::top().when;
                (std::this_thread::sleep_until(next), true);
                next = queue::top().when
            ) {
                auto what = queue::top().what;

                queue::pop();

                if (what.is_subscribed()) {
                    what(recursor);
                }

                if (queue::empty()) {
                    break;
                }
            }
        }
    };

    std::shared_ptr<current_worker> wi;

public:
    current_thread()
        : wi(std::make_shared<current_worker>())
    {
    }
    virtual ~current_thread()
    {
    }

    static bool is_schedule_required() { return !queue::owned(); }

    inline bool is_tail_recursion_allowed() const {
        return queue::empty();
    }

    virtual clock_type::time_point now() const {
        return clock_type::now();
    }

    virtual worker create_worker(composite_subscription cs) const {
        return worker(std::move(cs), wi);
    }
};

inline const scheduler& make_current_thread() {
    static scheduler instance = make_scheduler<current_thread>();
    return instance;
}

}

}

#endif
