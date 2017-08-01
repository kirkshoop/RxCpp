// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_SCHEDULER_NEW_THREAD_HPP)
#define RXCPP_RX_SCHEDULER_NEW_THREAD_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace schedulers {

typedef std::function<std::thread(std::function<void()>)> thread_factory;

struct new_thread : public scheduler_interface
{
private:
    typedef new_thread this_type;
    new_thread(const this_type&);

    struct new_worker : public worker_interface
    {
    private:
        typedef new_worker this_type;

        typedef detail::action_queue queue_type;

        new_worker(const this_type&);

        typedef detail::schedulable_queue<
            typename clock_type::time_point> queue_item_time;

        typedef queue_item_time::item_type item_type;

        composite_subscription lifetime;
        mutable std::mutex lock;
        mutable std::condition_variable wake;
        mutable queue_item_time q;
        std::thread worker;
        recursion r;

    public:
        virtual ~new_worker()
        {
            std::cerr << "new worker down!" << std::endl;
            lifetime.unsubscribe();
        }

        new_worker(composite_subscription cs)
            : lifetime(cs)
        {
        }

        void start(thread_factory& tf) {

            lifetime.add([this](){
                std::cerr << ">>unsub" << std::endl;
                std::unique_lock<std::mutex> guard(this->lock);
                auto expired = std::move(this->q);
                std::cerr << "-locked-" << std::endl;
                if (!this->q.empty()) std::terminate();
                this->wake.notify_one();
                std::cerr << "-notified-" << std::endl;

                if (this->worker.joinable() && this->worker.get_id() != std::this_thread::get_id()) {
                    guard.unlock();
                    this->worker.join();
                std::cerr << "-joined-" << std::endl;
                }
                else {
                    this->worker.detach();
                std::cerr << "-detached-" << std::endl;
                std::cerr << "unsub<<" << std::endl;
                }
            });

            //std::weak_ptr<new_worker> weak = std::shared_ptr<new_worker>(this->shared_from_this(), this);
            auto keepAlive = std::shared_ptr<new_worker>(this->shared_from_this(), this);

            worker = tf([keepAlive](){

                RXCPP_UNWIND_AUTO([]{
                    std::cerr << "thread down!" << std::endl;
                });
                std::cerr << "thread up!" << std::endl;

//                auto keepAlive = weak;
//                if (!keepAlive) { return; }

                // take ownership
                queue_type::ensure(keepAlive);
                // release ownership
                RXCPP_UNWIND_AUTO([]{
                    queue_type::destroy();
                });

                for(;;) {
                    std::unique_lock<std::mutex> guard(keepAlive->lock);
                    if (keepAlive->q.empty()) {
                        keepAlive->wake.wait(guard, [keepAlive](){
                            return !keepAlive->lifetime.is_subscribed() || !keepAlive->q.empty();
                        });
                    }
                    if (!keepAlive->lifetime.is_subscribed()) {
                        break;
                    }
                    auto& peek = keepAlive->q.top();
                    if (!peek.what.is_subscribed()) {
                        keepAlive->q.pop();
                        continue;
                    }
                    if (clock_type::now() < peek.when) {
                        keepAlive->wake.wait_until(guard, peek.when);
                        continue;
                    }
                    auto what = peek.what;
                    keepAlive->q.pop();
                    keepAlive->r.reset(keepAlive->q.empty());
                    guard.unlock();
                    what(keepAlive->r.get_recurse());
                }
            });
        }

        virtual clock_type::time_point now() const {
            return clock_type::now();
        }

        virtual void schedule(const schedulable& scbl) const {
            schedule(now(), scbl);
        }

        virtual void schedule(clock_type::time_point when, const schedulable& scbl) const {
            if (scbl.is_subscribed()) {
                std::unique_lock<std::mutex> guard(lock);
                q.push(item_type(when, scbl));
                r.reset(false);
            }
            wake.notify_one();
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
    virtual ~new_thread()
    {
    }

    virtual clock_type::time_point now() const {
        return clock_type::now();
    }

    virtual worker create_worker(composite_subscription cs) const {
        auto w = std::make_shared<new_worker>(cs);
        w->start(factory);
        return worker(cs, w);
    }
};

inline scheduler make_new_thread() {
    static scheduler instance = make_scheduler<new_thread>();
    return instance;
}
inline scheduler make_new_thread(thread_factory tf) {
    return make_scheduler<new_thread>(tf);
}

}

}

#endif
