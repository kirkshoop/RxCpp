// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_SCHEDULER_HPP)
#define RXCPP_RX_SCHEDULER_HPP

#include "rx-includes.hpp"

namespace rxcpp {

namespace schedulers {

namespace detail {

class scheduler_interface;
class worker_interface;

typedef std::shared_ptr<worker_interface> worker_interface_ptr;
typedef std::shared_ptr<const worker_interface> const_worker_interface_ptr;

typedef std::shared_ptr<scheduler_interface> scheduler_interface_ptr;
typedef std::shared_ptr<const scheduler_interface> const_scheduler_interface_ptr;

}

struct scheduler_base
{
    typedef std::chrono::steady_clock clock_type;
    typedef tag_scheduler scheduler_tag;
};

struct worker_base : public subscription_base
{
    typedef tag_worker worker_tag;
};

struct action_base
{
    typedef tag_action action_tag;
};

template<class T, class C = rxu::types_checked>
struct not_action {typedef rxu::types_checked type;};

template<class T>
struct not_action<T, typename rxu::types_checked_from<typename rxu::decay_t<T>::action_tag>::type>;

struct action_verb
{
    enum type {
        exit = 0,
        tail,
        repeat_when
    };
};

struct action_result {
    typedef scheduler_base::clock_type clock_type;
    action_result() : verb(action_verb::tail) {}
    action_result(std::nullptr_t) : verb(action_verb::exit) {}
    action_result(action_verb::type v) : verb(v) {}
    explicit action_result(clock_type::time_point when) : verb(action_verb::repeat_when), when(when) {}
    explicit action_result(clock_type::duration when) : verb(action_verb::repeat_when), when(clock_type::now() + when) {}
    action_verb::type verb;
    clock_type::time_point when;
};

template<class Inner = void>
class scheduler;

template<class Inner = void>
class worker;

namespace detail {

template<class F>
struct is_action_function
{
    struct not_void {};
    template<class CF>
    static auto check(int) -> decltype((*(CF*)nullptr)(*(worker<>*)nullptr));
    template<class CF>
    static not_void check(...);

    static const bool value = std::is_same<decltype(check<rxu::decay_t<F>>(0)), action_result>::value;
};

}

template<class F = void>
class action;

template<>
class action<void> : public action_base
{
public:
    typedef std::function<action_result(worker<>&)> function_type;
private:
    function_type function;
public:
    action()
    {
    }
    explicit action(function_type f)
        : function(std::move(f))
    {
    }
    template<class OF>
    action(const action<OF>& o)
        : function(o.function)
    {
    }
    template<class OF>
    action(action<OF>&& o)
        : function(std::move(o.function))
    {
    }

    explicit operator bool () const { return !!function; }
    action& operator=(std::nullptr_t) { function = nullptr; return *this; }

    /// call the function
    action_result operator()(worker<>& s) const {
        return function(s);
    }
    action_result operator()(worker<>& s) {
        return function(s);
    }
    action_result operator()(worker<>&& s) const {
        return function(s);
    }
    action_result operator()(worker<>&& s) {
        return function(s);
    }
};

template<class F>
class action : public action_base
{
public:
    typedef F function_type;
private:
    function_type function;

    template<class OF>
    friend class action;
public:
    action()
    {
    }
    explicit action(function_type f)
        : function(std::move(f))
    {
    }

    explicit operator bool () const { return true; }

    /// call the function
    template<class I>
    action_result operator()(worker<I>& s) const {
        return function(s);
    }
    template<class I>
    action_result operator()(worker<I>& s) {
        return function(s);
    }
    template<class I>
    action_result operator()(worker<I>&& s) const {
        return function(s);
    }
    template<class I>
    action_result operator()(worker<I>&& s) {
        return function(s);
    }
};

auto make_action() -> action<> {
    return action<>();
}

template<class F, class NotAction = typename not_action<F>::type>
auto make_action(F&& f) -> action<rxu::decay_t<F>> {
    static_assert(detail::is_action_function<F>::value, "action function must be action_result(worker)");
    return action<rxu::decay_t<F>>(std::forward<F>(f));
}
template<class F>
auto make_action(action<F> a) -> action<F> {
    return action<F>(std::move(a));
}

namespace detail {
class worker_interface
    : public std::enable_shared_from_this<worker_interface>
{
public:
    typedef worker_interface worker_interface_tag;

    typedef scheduler_base::clock_type clock_type;

    virtual ~worker_interface() {}

    virtual clock_type::time_point now() const = 0;

    virtual composite_subscription& get_subscription() const = 0;

    virtual void schedule(action<> act) const = 0;
    virtual void schedule(clock_type::time_point when, action<> act) const = 0;
};

template<class Inner>
class inner_worker : public worker_interface
{
public:
    template<class I>
    explicit inner_worker(I&& i) : inner(std::forward<I>(i)) {}

    virtual clock_type::time_point now() const {
        return inner.now();
    }

    virtual composite_subscription& get_subscription() const {
        return inner.get_subscription();
    }

    virtual void schedule(action<> act) const {
        inner.schedule(std::move(act));
    }
    virtual void schedule(clock_type::time_point when, action<> act) const {
        inner.schedule(when, std::move(act));
    }

private:
    Inner inner;
};

template<class Inner,
        class NotInterface = rxu::not_same<Inner, detail::worker_interface_ptr>,
        class NotConstInterface = rxu::not_same<Inner, detail::const_worker_interface_ptr>>
auto make_inner_worker(Inner&& i) -> detail::worker_interface_ptr {
    return std::make_shared<inner_worker<Inner>>(std::forward<Inner>(i));
}
auto make_inner_worker(detail::const_worker_interface_ptr i) -> detail::worker_interface_ptr {
    return std::const_pointer_cast<worker_interface>(i);
}

template<class Worker, class F>
void schedule_periodically(Worker& w, scheduler_base::clock_type::time_point initial, scheduler_base::clock_type::duration period, F f) {
    auto target = initial;
    auto periodic = make_action(
        [target, period, f](worker<>& self) mutable -> action_result {
            action_result r;
            while (r.verb != action_verb::exit) {
                if (r.verb == action_verb::repeat_when) {std::this_thread::sleep_until(r.when); }
                r = f(self);
            }
            // schedule next occurance (if the action took longer than 'period' target will be in the past)
            target += period;
            return action_result(target);
        });
    w.schedule(target, periodic);
}


}

template<class T, class C = rxu::types_checked>
struct not_worker {typedef rxu::types_checked type;};

template<class T>
struct not_worker<T, typename rxu::types_checked_from<typename rxu::decay_t<T>::worker_tag>::type>;

/// a worker ensures that all scheduled actions on the same instance are executed in-order with no overlap
/// a worker ensures that all scheduled actions are unsubscribed when it is unsubscribed
/// some inner implementations will impose additional constraints on the execution of items.

template<>
class worker<void> : public worker_base
{
    detail::worker_interface_ptr inner;
    friend bool operator==(const worker&, const worker&);
public:
    typedef scheduler_base::clock_type clock_type;
    typedef composite_subscription::weak_subscription weak_subscription;

    worker()
    {
    }
    template<class I, class NotWorker = typename not_worker<I>::type>
    explicit worker(I&& i)
        : inner(detail::make_inner_worker(std::forward<I>(i)))
    {
    }
    template<class OI>
    worker(const worker<OI>& o)
        : inner(detail::make_inner_worker(o.inner))
    {
    }
    template<class OI>
    worker(worker<OI>&& o)
        : inner(detail::make_inner_worker(std::move(o.inner)))
    {
    }

    explicit operator bool () const { return !!inner; }
    worker& operator=(std::nullptr_t) { inner = nullptr; return *this; }

    inline const composite_subscription& get_subscription() const {
        return inner->get_subscription();
    }
    inline composite_subscription& get_subscription() {
        return inner->get_subscription();
    }

    // composite_subscription
    //
    inline bool is_subscribed() const {
        return get_subscription().is_subscribed();
    }
    inline weak_subscription add(subscription s) const {
        return get_subscription().add(std::move(s));
    }
    inline void remove(weak_subscription w) const {
        return get_subscription().remove(std::move(w));
    }
    inline void clear() const {
        return get_subscription().clear();
    }
    inline void unsubscribe() const {
        return get_subscription().unsubscribe();
    }

    // worker_interface
    //
    /// return the current time for this worker
    inline clock_type::time_point now() const {
        return inner->now();
    }

    /// insert the supplied action to be run as soon as possible
    inline void schedule(action<> act) const {
        trace_activity().schedule_enter(*inner.get());
        inner->schedule(std::move(act));
        trace_activity().schedule_return(*inner.get());
    }

    /// insert the supplied action to be run at the time specified
    inline void schedule(clock_type::time_point when, action<> act) const {
        trace_activity().schedule_when_enter(*inner.get(), when);
        inner->schedule(when, std::move(act));
        trace_activity().schedule_when_return(*inner.get());
    }

    // helpers
    //

    /// insert the supplied action to be run at the time specified
    template<class Action>
    void schedule(Action act) const {
        schedule(make_action(act));
    }

    /// insert the supplied action to be run at the time specified
    template<class Action>
    void schedule(clock_type::time_point when, Action act) const {
        schedule(when, make_action(act));
    }

    /// insert the supplied action to be run at now() + the delay specified
    template<class Action>
    void schedule(clock_type::duration when, Action act) const {
        schedule(now() + when, make_action(act));
    }

    /// insert the supplied action to be run at the initial time specified and then again at initial + (N * period)
    /// this will continue until the worker or action is unsubscribed.
    template<class Action>
    void schedule_periodically(clock_type::time_point initial, clock_type::duration period, Action act) const {
        detail::schedule_periodically(*this, initial, period, make_action(act));
    }

    /// insert the supplied action to be run at now() + the initial delay specified and then again at now() + initial + (N * period)
    /// this will continue until the worker or action is unsubscribed.
    template<class Action>
    void schedule_periodically(clock_type::duration initial, clock_type::duration period, Action act) const {
        detail::schedule_periodically(*this, now() + initial, period, make_action(act));
    }
    /// insert the supplied action to be run repeatibly at now() + initial + (N * period)
    /// this will continue until the worker or action is unsubscribed.
    template<class Action>
    void schedule_periodically(clock_type::duration period, Action act) const {
        detail::schedule_periodically(*this, now() + period, period, make_action(act));
    }
};

template<class Inner>
class worker : public worker_base
{
    worker();
    Inner inner;
    friend bool operator==(const worker&, const worker&);
    friend class worker<void>;
public:
    typedef scheduler_base::clock_type clock_type;
    typedef composite_subscription::weak_subscription weak_subscription;

    template<class I,
        class NotWorker = typename not_worker<I>::type>
    worker(I&& i) : inner(std::forward<I>(i)) {}

    explicit operator bool () const { return true; }

    inline const composite_subscription& get_subscription() const {
        return inner.get_subscription();
    }
    inline composite_subscription& get_subscription() {
        return inner.get_subscription();
    }

    // composite_subscription
    //
    inline bool is_subscribed() const {
        return get_subscription().is_subscribed();
    }
    inline weak_subscription add(subscription s) const {
        return get_subscription().add(std::move(s));
    }
    inline void remove(weak_subscription w) const {
        return get_subscription().remove(std::move(w));
    }
    inline void clear() const {
        return get_subscription().clear();
    }
    inline void unsubscribe() const {
        return get_subscription().unsubscribe();
    }

    // worker_interface
    //
    /// return the current time for this worker
    inline clock_type::time_point now() const {
        return inner.now();
    }

    /// insert the supplied action to be run as soon as possible
    template<class F>
    inline void schedule(action<F> act) const {
        trace_activity().schedule_enter(inner);
        inner.schedule(std::move(act));
        trace_activity().schedule_return(inner);
    }

    /// insert the supplied action to be run at the time specified
    template<class F>
    inline void schedule(clock_type::time_point when, action<F> act) const {
        trace_activity().schedule_when_enter(inner, when);
        inner.schedule(when, std::move(act));
        trace_activity().schedule_when_return(inner);
    }

    // helpers
    //

    /// insert the supplied action to be run at the time specified
    template<class Action>
    void schedule(Action act) const {
        schedule(make_action(act));
    }

    /// insert the supplied action to be run at the time specified
    template<class Action>
    void schedule(clock_type::time_point when, Action act) const {
        schedule(when, make_action(act));
    }

    /// insert the supplied action to be run at now() + the delay specified
    template<class Action>
    void schedule(clock_type::duration when, Action act) const {
        schedule(now() + when, make_action(act));
    }

    /// insert the supplied action to be run at the initial time specified and then again at initial + (N * period)
    /// this will continue until the worker or action is unsubscribed.
    template<class Action>
    void schedule_periodically(clock_type::time_point initial, clock_type::duration period, Action act) const {
        detail::schedule_periodically(*this, initial, period, make_action(act));
    }

    /// insert the supplied action to be run at now() + the initial delay specified and then again at now() + initial + (N * period)
    /// this will continue until the worker or action is unsubscribed.
    template<class Action>
    void schedule_periodically(clock_type::duration initial, clock_type::duration period, Action act) const {
        detail::schedule_periodically(*this, now() + initial, period, make_action(act));
    }
    /// insert the supplied action to be run repeatibly at now() + initial + (N * period)
    /// this will continue until the worker or action is unsubscribed.
    template<class Action>
    void schedule_periodically(clock_type::duration period, Action act) const {
        detail::schedule_periodically(*this, now() + period, period, make_action(act));
    }
};

template<class Inner>
inline bool operator==(const worker<Inner>& lhs, const worker<Inner>& rhs) {
    return lhs.inner == rhs.inner;
}
template<class Inner>
inline bool operator!=(const worker<Inner>& lhs, const worker<Inner>& rhs) {
    return !(lhs == rhs);
}


auto make_worker() -> worker<> {
    return worker<>();
}

template<class I, class NotWorker = typename not_worker<I>::type>
auto make_worker(I&& i) -> worker<rxu::decay_t<I>> {
    return worker<rxu::decay_t<I>>(std::forward<I>(i));
}

template<class I>
auto make_worker(worker<I> i) -> worker<I> {
    return worker<I>(std::move(i));
}

namespace detail {

class scheduler_interface
    : public std::enable_shared_from_this<scheduler_interface>
{
    typedef scheduler_interface this_type;

public:
    typedef scheduler_base::clock_type clock_type;

    virtual ~scheduler_interface() {}

    virtual clock_type::time_point now() const = 0;

    virtual worker<> create_worker(composite_subscription cs) const = 0;
};

template<class Inner>
class inner_scheduler : public scheduler_interface
{
public:
    template<class I>
    explicit inner_scheduler(I&& i) : inner(std::forward<I>(i)) {
    }

    virtual clock_type::time_point now() const {
        return inner.now();
    }

    virtual worker<> create_worker(composite_subscription cs) const {
        return inner.create_worker(std::move(cs));
    }

private:
    Inner inner;
};

template<class Inner,
        class NotInterface = rxu::not_same<Inner, detail::scheduler_interface_ptr>,
        class NotConstInterface = rxu::not_same<Inner, detail::const_scheduler_interface_ptr>>
auto make_inner_scheduler(Inner&& i) -> detail::scheduler_interface_ptr {
    return std::make_shared<inner_scheduler<Inner>>(std::forward<Inner>(i));
}
auto make_inner_scheduler(detail::const_scheduler_interface_ptr i) -> detail::scheduler_interface_ptr {
    return std::const_pointer_cast<scheduler_interface>(i);
}

}

template<class T, class C = rxu::types_checked>
struct not_scheduler {typedef rxu::types_checked type;};

template<class T>
struct not_scheduler<T, typename rxu::types_checked_from<typename rxu::decay_t<T>::scheduler_tag>::type>;


/*!
    \brief allows functions to be called at specified times and possibly in other contexts.

    \ingroup group-core

*/
template<>
class scheduler<void> : public scheduler_base
{
    detail::scheduler_interface_ptr inner;
    friend bool operator==(const scheduler&, const scheduler&);
public:
    typedef scheduler_base::clock_type clock_type;
    typedef worker<> worker_type;

    scheduler()
    {
    }
    template<class I, class NotScheduler = typename not_scheduler<I>::type>
    explicit scheduler(I&& i)
        : inner(detail::make_inner_scheduler(std::forward<I>(i)))
    {
    }
    template<class OI>
    scheduler(const scheduler<OI>& o)
        : inner(detail::make_inner_scheduler(o.inner))
    {
    }
    template<class OI>
    scheduler(scheduler<OI>&& o)
        : inner(detail::make_inner_scheduler(std::move(o.inner)))
    {
    }

    explicit operator bool () const { return !!inner; }
    scheduler& operator=(std::nullptr_t) { inner = nullptr; return *this; }

    /// return the current time for this scheduler
    inline clock_type::time_point now() const {
        return inner->now();
    }
    /// create a worker with a lifetime.
    /// when the worker is unsubscribed all scheduled items will be unsubscribed.
    /// items scheduled to a worker will be run one at a time.
    /// scheduling order is preserved: when more than one item is scheduled for
    /// time T then at time T they will be run in the order that they were scheduled.
    inline worker<> create_worker(composite_subscription cs = composite_subscription()) const {
        return inner->create_worker(cs);
    }
};

/*!
    \brief allows functions to be called at specified times and possibly in other contexts.

    \ingroup group-core

*/
template<class Inner>
class scheduler : public scheduler_base
{
    Inner inner;
    friend bool operator==(const scheduler&, const scheduler&);
    friend class scheduler<>;
public:
    typedef scheduler_base::clock_type clock_type;
    typedef decltype(((Inner*)nullptr)->create_worker(composite_subscription())) worker_type;

    template<class I,
        class NotScheduler = typename not_scheduler<I>::type>
    scheduler(I&& i) : inner(std::forward<I>(i)) {}

    explicit operator bool () const { return true; }

    /// return the current time for this scheduler
    inline clock_type::time_point now() const {
        return inner.now();
    }
    /// create a worker with a lifetime.
    /// when the worker is unsubscribed all scheduled items will be unsubscribed.
    /// items scheduled to a worker will be run one at a time.
    /// scheduling order is preserved: when more than one item is scheduled for
    /// time T then at time T they will be run in the order that they were scheduled.
    inline auto create_worker(composite_subscription cs = composite_subscription()) const -> worker_type {
        return inner.create_worker(cs);
    }
};

auto make_scheduler() -> scheduler<> {
    return scheduler<>();
}

template<class I, class NotScheduler = typename not_scheduler<I>::type>
auto make_scheduler(I&& i) -> scheduler<I> {
    return scheduler<I>(std::forward<I>(i));
}

template<class I>
auto make_scheduler(scheduler<I> i) -> scheduler<I> {
    return scheduler<I>(std::move(i));
}

namespace detail {

template<class TimePoint>
struct time_action
{
    typedef TimePoint time_point_type;

    time_action(TimePoint when, action<> a)
        : when(when)
        , what(std::move(a))
    {
    }
    TimePoint when;
    action<> what;
};


// Sorts time_action items in priority order sorted
// on value of time_action.when. Items with equal
// values for when are sorted in fifo order.
template<class TimePoint>
class action_queue {
public:
    typedef time_action<TimePoint> item_type;
    typedef std::pair<item_type, int64_t> elem_type;
    typedef std::vector<elem_type> container_type;
    typedef const item_type& const_reference;

private:
    struct compare_elem
    {
        bool operator()(const elem_type& lhs, const elem_type& rhs) const {
            if (lhs.first.when == rhs.first.when) {
                return lhs.second > rhs.second;
            }
            else {
                return lhs.first.when > rhs.first.when;
            }
        }
    };

    typedef std::priority_queue<
        elem_type,
        container_type,
        compare_elem
    > queue_type;

    queue_type queue;

    int64_t ordinal;

public:
    const_reference top() const {
        return queue.top().first;
    }

    void pop() {
        queue.pop();
    }

    bool empty() const {
        return queue.empty();
    }

    void push(const item_type& value) {
        queue.push(elem_type(value, ordinal++));
    }

    void push(item_type&& value) {
        queue.push(elem_type(std::move(value), ordinal++));
    }
};

}

}
namespace rxsc=schedulers;

}

#include "schedulers/rx-action_queue.hpp"
//#include "schedulers/rx-currentthread.hpp"
#include "schedulers/rx-newthread.hpp"
//#include "schedulers/rx-eventloop.hpp"
#include "schedulers/rx-immediate.hpp"
//#include "schedulers/rx-virtualtime.hpp"
//#include "schedulers/rx-sameworker.hpp"

#endif
