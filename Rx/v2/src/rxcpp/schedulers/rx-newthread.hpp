// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_SCHEDULER_NEW_THREAD_HPP)
#define RXCPP_RX_SCHEDULER_NEW_THREAD_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace schedulers {

typedef std::function<std::thread(std::function<void()>)> thread_factory;

struct new_thread
{
    typedef scheduler_base::clock_type clock_type;
private:
    typedef new_thread this_type;

    struct new_worker
    {
        typedef scheduler_base::clock_type clock_type;
    private:
        typedef new_worker this_type;

        typedef detail::current_thread_action_queue queue_type;

        struct new_worker_state : public std::enable_shared_from_this<new_worker_state>
        {
            typedef detail::action_queue<
                typename clock_type::time_point> queue_item_time;

            typedef queue_item_time::item_type item_type;

            ~new_worker_state()
            {
                std::unique_lock<std::mutex> guard(lock);
                if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) {
                    lifetime.unsubscribe();
                    guard.unlock();
                    worker.join();
                }
                else {
                    lifetime.unsubscribe();
                    worker.detach();
                }
            }

            explicit new_worker_state(composite_subscription cs)
                : lifetime(cs)
            {
            }

            composite_subscription lifetime;
            mutable std::mutex lock;
            mutable std::condition_variable wake;
            mutable queue_item_time queue;
            std::thread worker;
        };

        std::shared_ptr<new_worker_state> state;

    public:
        explicit new_worker(std::shared_ptr<new_worker_state> ws)
            : state(ws)
        {
        }

        new_worker(composite_subscription cs, thread_factory& tf)
            : state(std::make_shared<new_worker_state>(cs))
        {
            auto keepAlive = state;

            state->lifetime.add([keepAlive](){
                keepAlive->wake.notify_one();
            });

            state->worker = tf([keepAlive](){

                auto w = make_worker(new_worker(keepAlive));

                // take ownership
                queue_type::ensure(w);
                // release ownership
                RXCPP_UNWIND_AUTO([]{
                    queue_type::destroy();
                });

                for(;;) {
                    std::unique_lock<std::mutex> guard(keepAlive->lock);
                    if (keepAlive->queue.empty()) {
                        keepAlive->wake.wait(guard, [keepAlive](){
                            return !keepAlive->lifetime.is_subscribed() || !keepAlive->queue.empty();
                        });
                    }
                    if (!keepAlive->lifetime.is_subscribed()) {
                        break;
                    }
                    auto& peek = keepAlive->queue.top();
                    if (!peek.what) {
                        keepAlive->queue.pop();
                        continue;
                    }
                    if (clock_type::now() < peek.when) {
                        keepAlive->wake.wait_until(guard, peek.when);
                        continue;
                    }
                    auto value = peek;
                    keepAlive->queue.pop();

                    guard.unlock();

                    action_result r;
                    while (keepAlive->lifetime.is_subscribed() && r.verb != action_verb::exit) {
                        r = value.what(w);
                        if (r.verb == action_verb::repeat_when) {
                            value.when = r.when;
                            guard.lock();
                            keepAlive->queue.push(value);
                            break;
                        }
                    }
                }
            });
        }

        clock_type::time_point now() const {
            return clock_type::now();
        }

        composite_subscription& get_subscription() const {
            return state->lifetime;
        }

        void schedule(action<> act) const {
            schedule(now(), act);
        }

        void schedule(clock_type::time_point when, action<> act) const {
            if (act) {
                std::unique_lock<std::mutex> guard(state->lock);
                state->queue.push(new_worker_state::item_type(when, act));
                state->wake.notify_one();
            }
        }
    };

    mutable thread_factory factory;

public:
    new_thread()
        : factory([](std::function<void()> start){
            return std::thread(std::move(start));
        })
    {
    }
    explicit new_thread(thread_factory tf)
        : factory(tf)
    {
    }

    clock_type::time_point now() const {
        return clock_type::now();
    }

    auto create_worker(composite_subscription cs) const -> worker<> {
        return make_worker(new_worker(std::move(cs), factory));
    }
};

inline auto make_new_thread() -> scheduler<new_thread> {
    return scheduler<new_thread>(new_thread());
}
inline auto make_new_thread(thread_factory tf) -> scheduler<new_thread> {
    return scheduler<new_thread>(new_thread(tf));
}

}

}

#endif
