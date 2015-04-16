// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_SCHEDULER_ACTION_QUEUE_HPP)
#define RXCPP_RX_SCHEDULER_ACTION_QUEUE_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace schedulers {

namespace detail {

struct current_thread_action_queue
{
    typedef current_thread_action_queue this_type;

    typedef scheduler_base::clock_type clock;
    typedef time_action<clock::time_point> item_type;

private:
    typedef action_queue<item_type::time_point_type> queue_item_time;

public:
    struct current_thread_queue_type {
        worker<> w;
        queue_item_time queue;
    };

private:
    static current_thread_queue_type*& current_thread_queue() {
        static RXCPP_THREAD_LOCAL current_thread_queue_type* queue;
        return queue;
    }

public:

    static bool owned() {
        return !!current_thread_queue();
    }
    static const worker<>& get_worker_interface() {
        return current_thread_queue()->w;
    }
    static bool empty() {
        if (!current_thread_queue()) {
            abort();
        }
        return current_thread_queue()->queue.empty();
    }
    static queue_item_time::const_reference top() {
        if (!current_thread_queue()) {
            abort();
        }
        return current_thread_queue()->queue.top();
    }
    static void pop() {
        auto state = current_thread_queue();
        if (!state) {
            abort();
        }
        state->queue.pop();
    }
    static void push(item_type item) {
        auto state = current_thread_queue();
        if (!state) {
            abort();
        }
        if (!item.what) {
            return;
        }
        state->queue.push(std::move(item));
    }
    static worker<> ensure(worker<> w) {
        if (!!current_thread_queue()) {
            abort();
        }
        // create and publish new queue
        current_thread_queue() = new current_thread_queue_type();
        current_thread_queue()->w = w;
        return w;
    }
    static std::unique_ptr<current_thread_queue_type> create(worker<> w) {
        std::unique_ptr<current_thread_queue_type> result(new current_thread_queue_type());
        result->w = std::move(w);
        return result;
    }
    static void set(current_thread_queue_type* queue) {
        if (!!current_thread_queue()) {
            abort();
        }
        // publish new queue
        current_thread_queue() = queue;
    }
    static void destroy(current_thread_queue_type* queue) {
        delete queue;
    }
    static void destroy() {
        if (!current_thread_queue()) {
            abort();
        }
        destroy(current_thread_queue());
        current_thread_queue() = nullptr;
    }
};


}

}

}

#endif
