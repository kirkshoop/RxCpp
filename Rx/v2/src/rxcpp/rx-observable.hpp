// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_OBSERVABLE_HPP)
#define RXCPP_RX_OBSERVABLE_HPP

#include "rx-includes.hpp"

namespace rxcpp {

namespace detail {

template<class Source, class F>
struct is_operator_factory_for
{
    struct not_void {};
    template<class CS, class CF>
    static auto check(int) -> decltype((*(CF*)nullptr)(*(CS*)nullptr));
    template<class CS, class CF>
    static not_void check(...);

    typedef rxu::decay_t<Source> source_type;
    typedef rxu::decay_t<F> function_type;

    typedef decltype(check<source_type, function_type>(0)) detail_result;
    static const bool value = !std::is_same<detail_result, not_void>::value && is_observable<source_type>::value;
};

template<class Subscriber, class T>
struct has_on_subscribe_for
{
    struct not_void {};
    template<class CS, class CT>
    static auto check(int) -> decltype((*(CT*)nullptr).on_subscribe(*(CS*)nullptr));
    template<class CS, class CT>
    static not_void check(...);

    typedef decltype(check<rxu::decay_t<Subscriber>, T>(0)) detail_result;
    static const bool value = std::is_same<detail_result, void>::value;
};

}

/*!
    \defgroup group-observable Observables

    \brief These are the set of observable classes in rxcpp.
*/



template<class T>
class dynamic_observable
    : public rxs::source_base<T>
{
    struct state_type
        : public std::enable_shared_from_this<state_type>
    {
        typedef std::function<void(subscriber<T>)> onsubscribe_type;

        onsubscribe_type on_subscribe;
    };
    std::shared_ptr<state_type> state;

    template<class U>
    friend bool operator==(const dynamic_observable<U>&, const dynamic_observable<U>&);

    template<class SO>
    void construct(SO&& source, rxs::tag_source&&) {
        rxu::decay_t<SO> so = std::forward<SO>(source);
        state->on_subscribe = [so](subscriber<T> o) mutable {
            so.on_subscribe(std::move(o));
        };
    }

    struct tag_function {};
    template<class F>
    void construct(F&& f, tag_function&&) {
        state->on_subscribe = std::forward<F>(f);
    }

public:

    typedef tag_dynamic_observable dynamic_observable_tag;

    dynamic_observable()
    {
    }

    template<class SOF>
    explicit dynamic_observable(SOF&& sof, typename std::enable_if<!is_dynamic_observable<SOF>::value, void**>::type = 0)
        : state(std::make_shared<state_type>())
    {
        construct(std::forward<SOF>(sof),
                  typename std::conditional<rxs::is_source<SOF>::value || rxo::is_operator<SOF>::value, rxs::tag_source, tag_function>::type());
    }

    void on_subscribe(subscriber<T> o) const {
        state->on_subscribe(std::move(o));
    }

    template<class Subscriber>
    typename std::enable_if<is_subscriber<Subscriber>::value, void>::type
    on_subscribe(Subscriber o) const {
        state->on_subscribe(o.as_dynamic());
    }
};

template<class T>
inline bool operator==(const dynamic_observable<T>& lhs, const dynamic_observable<T>& rhs) {
    return lhs.state == rhs.state;
}
template<class T>
inline bool operator!=(const dynamic_observable<T>& lhs, const dynamic_observable<T>& rhs) {
    return !(lhs == rhs);
}

template<class T, class Source>
observable<T> make_observable_dynamic(Source&& s) {
    return observable<T>(dynamic_observable<T>(std::forward<Source>(s)));
}


/*!
    \brief a source of values whose methods block until all values have been emitted. subscribe or use one of the operator methods that reduce the values emitted to a single value.

    \ingroup group-observable

*/
template<class T, class Observable>
class blocking_observable
{
    template<class Obsvbl, class... ArgN>
    static auto blocking_subscribe(const Obsvbl& source, bool do_rethrow, ArgN&&... an)
        -> void {
        std::mutex lock;
        std::condition_variable wake;
        std::exception_ptr error;

        struct tracking
        {
            ~tracking()
            {
                if (!disposed || !wakened) abort();
            }
            tracking()
            {
                disposed = false;
                wakened = false;
                false_wakes = 0;
                true_wakes = 0;
            }
            std::atomic_bool disposed;
            std::atomic_bool wakened;
            std::atomic_int false_wakes;
            std::atomic_int true_wakes;
        };
        auto track = std::make_shared<tracking>();

        auto dest = make_subscriber<T>(std::forward<ArgN>(an)...);

        // keep any error to rethrow at the end.
        auto scbr = make_subscriber<T>(
            dest,
            [&](T t){dest.on_next(t);},
            [&](std::exception_ptr e){
                if (do_rethrow) {
                    error = e;
                } else {
                    dest.on_error(e);
                }
            },
            [&](){dest.on_completed();}
            );

        auto cs = scbr.get_subscription();
        cs.add(
            [&, track](){
                // OSX geting invalid x86 op if notify_one is after the disposed = true
                // presumably because the condition_variable may already have been awakened
                // and is now sitting in a while loop on disposed
                wake.notify_one();
                track->disposed = true;
            });

        std::unique_lock<std::mutex> guard(lock);
        source.subscribe(std::move(scbr));

        wake.wait(guard,
            [&, track](){
                // this is really not good.
                // false wakeups were never followed by true wakeups so..

                // anyways this gets triggered before disposed is set now so wait.
                while (!track->disposed) {
                    ++track->false_wakes;
                }
                ++track->true_wakes;
                return true;
            });
        track->wakened = true;
        if (!track->disposed || !track->wakened) abort();

        if (error) {std::rethrow_exception(error);}
    }

public:
    typedef rxu::decay_t<Observable> observable_type;
    observable_type source;
    ~blocking_observable()
    {
    }
    blocking_observable(observable_type s) : source(std::move(s)) {}

    ///
    /// `subscribe` will cause this observable to emit values to the provided subscriber.
    ///
    /// \return void
    ///
    /// \param an... - the arguments are passed to make_subscriber().
    ///
    /// callers must provide enough arguments to make a subscriber.
    /// overrides are supported. thus
    ///   `subscribe(thesubscriber, composite_subscription())`
    /// will take `thesubscriber.get_observer()` and the provided
    /// subscription and subscribe to the new subscriber.
    /// the `on_next`, `on_error`, `on_completed` methods can be supplied instead of an observer
    /// if a subscription or subscriber is not provided then a new subscription will be created.
    ///
    template<class... ArgN>
    auto subscribe(ArgN&&... an) const
        -> void {
        return blocking_subscribe(source, false, std::forward<ArgN>(an)...);
    }

    ///
    /// `subscribe_with_rethrow` will cause this observable to emit values to the provided subscriber.
    ///
    /// \note  If the source observable calls on_error, the raised exception is rethrown by this method.
    ///
    /// \note  If the source observable calls on_error, the `on_error` method on the subscriber will not be called.
    ///
    /// \return void
    ///
    /// \param an... - the arguments are passed to make_subscriber().
    ///
    /// callers must provide enough arguments to make a subscriber.
    /// overrides are supported. thus
    ///   `subscribe(thesubscriber, composite_subscription())`
    /// will take `thesubscriber.get_observer()` and the provided
    /// subscription and subscribe to the new subscriber.
    /// the `on_next`, `on_error`, `on_completed` methods can be supplied instead of an observer
    /// if a subscription or subscriber is not provided then a new subscription will be created.
    ///
    template<class... ArgN>
    auto subscribe_with_rethrow(ArgN&&... an) const
        -> void {
        return blocking_subscribe(source, true, std::forward<ArgN>(an)...);
    }

    /*! Return the first item emitted by this blocking_observable, or throw an std::runtime_error exception if it emits no items.

        \return  The first item emitted by this blocking_observable.

        \note  If the source observable calls on_error, the raised exception is rethrown by this method.

        \sample
        When the source observable emits at least one item:
        \snippet blocking_observable.cpp blocking first sample
        \snippet output.txt blocking first sample

        When the source observable is empty:
        \snippet blocking_observable.cpp blocking first empty sample
        \snippet output.txt blocking first empty sample
    */
    T first() {
        rxu::maybe<T> result;
        composite_subscription cs;
        subscribe_with_rethrow(
            cs,
            [&](T v){result.reset(v); cs.unsubscribe();});
        if (result.empty())
            throw rxcpp::empty_error("first() requires a stream with at least one value");
        return result.get();
    }

    /*! Return the last item emitted by this blocking_observable, or throw an std::runtime_error exception if it emits no items.

        \return  The last item emitted by this blocking_observable.

        \note  If the source observable calls on_error, the raised exception is rethrown by this method.

        \sample
        When the source observable emits at least one item:
        \snippet blocking_observable.cpp blocking last sample
        \snippet output.txt blocking last sample

        When the source observable is empty:
        \snippet blocking_observable.cpp blocking last empty sample
        \snippet output.txt blocking last empty sample
    */
    T last() const {
        rxu::maybe<T> result;
        subscribe_with_rethrow(
            [&](T v){result.reset(v);});
        if (result.empty())
            throw rxcpp::empty_error("last() requires a stream with at least one value");
        return result.get();
    }

    /*! Return the total number of items emitted by this blocking_observable.

        \return  The total number of items emitted by this blocking_observable.

        \sample
        \snippet blocking_observable.cpp blocking count sample
        \snippet output.txt blocking count sample

        When the source observable calls on_error:
        \snippet blocking_observable.cpp blocking count error sample
        \snippet output.txt blocking count error sample
    */
    int count() const {
        int result = 0;
        source.count().as_blocking().subscribe_with_rethrow(
            [&](int v){result = v;});
        return result;
    }

    /*! Return the sum of all items emitted by this blocking_observable, or throw an std::runtime_error exception if it emits no items.

        \return  The sum of all items emitted by this blocking_observable.

        \sample
        When the source observable emits at least one item:
        \snippet blocking_observable.cpp blocking sum sample
        \snippet output.txt blocking sum sample

        When the source observable is empty:
        \snippet blocking_observable.cpp blocking sum empty sample
        \snippet output.txt blocking sum empty sample

        When the source observable calls on_error:
        \snippet blocking_observable.cpp blocking sum error sample
        \snippet output.txt blocking sum error sample
    */
    T sum() const {
        return source.sum().as_blocking().last();
    }

    /*! Return the average value of all items emitted by this blocking_observable, or throw an std::runtime_error exception if it emits no items.

        \return  The average value of all items emitted by this blocking_observable.

        \sample
        When the source observable emits at least one item:
        \snippet blocking_observable.cpp blocking average sample
        \snippet output.txt blocking average sample

        When the source observable is empty:
        \snippet blocking_observable.cpp blocking average empty sample
        \snippet output.txt blocking average empty sample

        When the source observable calls on_error:
        \snippet blocking_observable.cpp blocking average error sample
        \snippet output.txt blocking average error sample
    */
    double average() const {
        return source.average().as_blocking().last();
    }
};

template<>
class observable<void, void>;

template<class T, class SourceOperator, class Observable>
class observable_operators;

/*!
    \class rxcpp::observable_root

    \ingroup group-observable group-core

    \brief observable uses this to store the operator and implement the core subscribe and lift methods.

*/
template<class T, class SourceOperator, class Observable>
class observable_root
    : public observable_base<T>
{
    typedef observable_root<T, SourceOperator, Observable> this_type;

    typedef Observable observable_type;

    const observable_type& cast() const {
        return *static_cast<observable_type const * const>(this);
    }

public:
    typedef rxu::decay_t<SourceOperator> source_operator_type;
    mutable source_operator_type source_operator;

protected:

    template<class Subscriber>
    auto detail_subscribe(Subscriber o) const
        -> composite_subscription {

        typedef rxu::decay_t<Subscriber> subscriber_type;

        static_assert(is_subscriber<subscriber_type>::value, "subscribe must be passed a subscriber");
        static_assert(std::is_same<typename source_operator_type::value_type, T>::value && std::is_convertible<T*, typename subscriber_type::value_type*>::value, "the value types in the sequence must match or be convertible");
        static_assert(detail::has_on_subscribe_for<subscriber_type, source_operator_type>::value, "inner must have on_subscribe method that accepts this subscriber ");

        trace_activity().subscribe_enter(*this, o);

        if (!o.is_subscribed()) {
            trace_activity().subscribe_return(*this);
            return o.get_subscription();
        }

        auto safe_subscribe = [&]() {
            try {
                source_operator.on_subscribe(o);
            }
            catch(...) {
                if (!o.is_subscribed()) {
                    throw;
                }
                o.on_error(std::current_exception());
                o.unsubscribe();
            }
        };

        // make sure to let current_thread take ownership of the thread as early as possible.
        if (rxsc::current_thread::is_schedule_required()) {
            const auto& sc = rxsc::make_current_thread();
            sc.create_worker(o.get_subscription()).schedule(
                [&](const rxsc::schedulable&) {
                    safe_subscribe();
                });
        } else {
            // current_thread already owns this thread.
            safe_subscribe();
        }

        trace_activity().subscribe_return(*this);
        return o.get_subscription();
    }

public:
    typedef T value_type;

    static_assert(rxo::is_operator<source_operator_type>::value || rxs::is_source<source_operator_type>::value, "observable must wrap an operator or source");

    ~observable_root()
    {
    }

    observable_root()
    {
    }

    explicit observable_root(const source_operator_type& o)
        : source_operator(o)
    {
    }
    explicit observable_root(source_operator_type&& o)
        : source_operator(std::move(o))
    {
    }

    /// implicit conversion between observables of the same value_type
    template<class SO>
    observable_root(const observable<T, SO>& o)
        : source_operator(o.source_operator)
    {}
    /// implicit conversion between observables of the same value_type
    template<class SO>
    observable_root(observable<T, SO>&& o)
        : source_operator(std::move(o.source_operator))
    {}

#if 0
    template<class I>
    void on_subscribe(observer<T, I> o) const {
        source_operator.on_subscribe(o);
    }
#endif

    /*! Return a new observable that contains the blocking methods for this observable.

        \return  An observable that contains the blocking methods for this observable.

        \sample
        \snippet from.cpp threaded from sample
        \snippet output.txt threaded from sample
    */
    blocking_observable<T, this_type> as_blocking() const {
        return blocking_observable<T, this_type>(cast());
    }

    /// \cond SHOW_SERVICE_MEMBERS

    ///
    /// takes any function that will take this observable and produce a result value.
    /// this is intended to allow externally defined operators, that use subscribe,
    /// to be connected into the expression.
    ///
    template<class OperatorFactory>
    auto op(OperatorFactory&& of) const
        -> decltype(of(*(const observable_type*)nullptr)) {
        return      of(cast());
        static_assert(detail::is_operator_factory_for<observable_type, OperatorFactory>::value, "Function passed for op() must have the signature Result(SourceObservable)");
    }

    ///
    /// takes any function that will take a subscriber for this observable and produce a subscriber.
    /// this is intended to allow externally defined operators, that use make_subscriber, to be connected
    /// into the expression.
    ///
    template<class ResultType, class Operator>
    auto lift(Operator&& op) const
        ->      observable<rxu::value_type_t<rxo::detail::lift_operator<ResultType, source_operator_type, Operator>>, rxo::detail::lift_operator<ResultType, source_operator_type, Operator>> {
        return  observable<rxu::value_type_t<rxo::detail::lift_operator<ResultType, source_operator_type, Operator>>, rxo::detail::lift_operator<ResultType, source_operator_type, Operator>>(
                                                                                                                      rxo::detail::lift_operator<ResultType, source_operator_type, Operator>(source_operator, std::forward<Operator>(op)));
        static_assert(detail::is_lift_function_for<T, subscriber<ResultType>, Operator>::value, "Function passed for lift() must have the signature subscriber<...>(subscriber<T, ...>)");
    }

    ///
    /// takes any function that will take a subscriber for this observable and produce a subscriber.
    /// this is intended to allow externally defined operators, that use make_subscriber, to be connected
    /// into the expression.
    ///
    template<class ResultType, class Operator>
    auto lift_if(Operator&& op) const
        -> typename std::enable_if<detail::is_lift_function_for<T, subscriber<ResultType>, Operator>::value,
            observable<rxu::value_type_t<rxo::detail::lift_operator<ResultType, source_operator_type, Operator>>, rxo::detail::lift_operator<ResultType, source_operator_type, Operator>>>::type {
        return  observable<rxu::value_type_t<rxo::detail::lift_operator<ResultType, source_operator_type, Operator>>, rxo::detail::lift_operator<ResultType, source_operator_type, Operator>>(
                                                                                                                      rxo::detail::lift_operator<ResultType, source_operator_type, Operator>(source_operator, std::forward<Operator>(op)));
    }
    ///
    /// takes any function that will take a subscriber for this observable and produce a subscriber.
    /// this is intended to allow externally defined operators, that use make_subscriber, to be connected
    /// into the expression.
    ///
    template<class ResultType, class Operator>
    auto lift_if(Operator&&) const
        -> typename std::enable_if<!detail::is_lift_function_for<T, subscriber<ResultType>, Operator>::value,
            decltype(rxs::from<ResultType>())>::type {
        return       rxs::from<ResultType>();
    }
    /// \endcond

    /*! Subscribe will cause this observable to emit values to the provided subscriber.

        \tparam ArgN  types of the subscriber parameters

        \param an  the parameters for making a subscriber

        \return  A subscription with which the observer can stop receiving items before the observable has finished sending them.

        The arguments of subscribe are forwarded to rxcpp::make_subscriber function. Some possible alternatives are:

        - Pass an already composed rxcpp::subscriber:
        \snippet subscribe.cpp subscribe by subscriber
        \snippet output.txt subscribe by subscriber

        - Pass an rxcpp::observer. This allows subscribing the same subscriber to several observables:
        \snippet subscribe.cpp subscribe by observer
        \snippet output.txt subscribe by observer

        - Pass an `on_next` handler:
        \snippet subscribe.cpp subscribe by on_next
        \snippet output.txt subscribe by on_next

        - Pass `on_next` and `on_error` handlers:
        \snippet subscribe.cpp subscribe by on_next and on_error
        \snippet output.txt subscribe by on_next and on_error

        - Pass `on_next` and `on_completed` handlers:
        \snippet subscribe.cpp subscribe by on_next and on_completed
        \snippet output.txt subscribe by on_next and on_completed

        - Pass `on_next`, `on_error`, and `on_completed` handlers:
        \snippet subscribe.cpp subscribe by on_next, on_error, and on_completed
        \snippet output.txt subscribe by on_next, on_error, and on_completed
        .

        All the alternatives above also support passing rxcpp::composite_subscription instance. For example:
        \snippet subscribe.cpp subscribe by subscription, on_next, and on_completed
        \snippet output.txt subscribe by subscription, on_next, and on_completed

        If neither subscription nor subscriber are provided, then a new subscription is created and returned as a result:
        \snippet subscribe.cpp subscribe unsubscribe
        \snippet output.txt subscribe unsubscribe

        For more details, see rxcpp::make_subscriber function description.
    */
    template<class... ArgN>
    auto subscribe(ArgN&&... an) const
        -> composite_subscription {
        return detail_subscribe(make_subscriber<T>(std::forward<ArgN>(an)...));
    }
};

/*!
    \class rxcpp::observable

    \ingroup group-observable group-core

    \brief a source of values. subscribe or use one of the operator methods that return a new observable, which uses this observable as a source.

    \par Some code
    This sample will observable::subscribe() to values from a observable<void, void>::range().

    \sample
    \snippet range.cpp range sample
    \snippet output.txt range sample

*/
template<class T, class SourceOperator>
class observable
    : public observable_root<T, SourceOperator, observable<T, SourceOperator>>
    , public observable_operators<T, SourceOperator, observable<T, SourceOperator>>
{
private:
    static_assert(std::is_same<T, typename SourceOperator::value_type>::value, "SourceOperator::value_type must be the same as T in observable<T, SourceOperator>");

    typedef observable<T, SourceOperator> this_type;

    template<class U, class SO>
    friend class observable;

    template<class U, class SO>
    friend bool operator==(const observable<U, SO>&, const observable<U, SO>&);

public:
    typedef T value_type;
    typedef rxu::decay_t<SourceOperator> source_operator_type;

    static_assert(rxo::is_operator<source_operator_type>::value || rxs::is_source<source_operator_type>::value, "observable must wrap an operator or source");

    ~observable()
    {
    }

    observable()
    {
    }

    explicit observable(const source_operator_type& o)
        : observable_root<T, SourceOperator, this_type>(o)
    {
    }
    explicit observable(source_operator_type&& o)
        : observable_root<T, SourceOperator, this_type>(std::move(o))
    {
    }

    /// implicit conversion between observables of the same value_type
    template<class SO>
    observable(const observable<T, SO>& o)
        : observable_root<T, SourceOperator, this_type>(o)
    {}
    /// implicit conversion between observables of the same value_type
    template<class SO>
    observable(observable<T, SO>&& o)
        : observable_root<T, SourceOperator, this_type>(std::move(o))
    {}


    /*! Return a new observable that performs type-forgetting conversion of this observable.

        \return  The source observable converted to observable<T>.

        \note This operator could be useful to workaround lambda deduction bug on msvc 2013.

        \sample
        \snippet as_dynamic.cpp as_dynamic sample
        \snippet output.txt as_dynamic sample
    */
    observable<T> as_dynamic() const {
        return *this;
    }
};

template<class T, class SourceOperator>
inline bool operator==(const observable<T, SourceOperator>& lhs, const observable<T, SourceOperator>& rhs) {
    return lhs.source_operator == rhs.source_operator;
}
template<class T, class SourceOperator>
inline bool operator!=(const observable<T, SourceOperator>& lhs, const observable<T, SourceOperator>& rhs) {
    return !(lhs == rhs);
}

namespace detail {

template<bool Selector, class Default, class SO>
struct resolve_observable;

template<class Default, class SO>
struct resolve_observable<true, Default, SO>
{
    typedef typename SO::type type;
    typedef typename type::value_type value_type;
    static const bool value = true;
    typedef observable<value_type, type> observable_type;
    template<class... AN>
    static observable_type make(const Default&, AN&&... an) {
        return observable_type(type(std::forward<AN>(an)...));
    }
};
template<class Default, class SO>
struct resolve_observable<false, Default, SO>
{
    static const bool value = false;
    typedef Default observable_type;
    template<class... AN>
    static observable_type make(const observable_type& that, const AN&...) {
        return that;
    }
};
template<class SO>
struct resolve_observable<true, void, SO>
{
    typedef typename SO::type type;
    typedef typename type::value_type value_type;
    static const bool value = true;
    typedef observable<value_type, type> observable_type;
    template<class... AN>
    static observable_type make(AN&&... an) {
        return observable_type(type(std::forward<AN>(an)...));
    }
};
template<class SO>
struct resolve_observable<false, void, SO>
{
    static const bool value = false;
    typedef void observable_type;
    template<class... AN>
    static observable_type make(const AN&...) {
    }
};

}

template<class Selector, class Default, template<class... TN> class SO, class... AN>
struct defer_observable
    : public detail::resolve_observable<Selector::value, Default, rxu::defer_type<SO, AN...>>
{
};

}

//
// support range() >> filter() >> subscribe() syntax
// '>>' is spelled 'stream'
//
template<class T, class SourceOperator, class OperatorFactory>
auto operator >> (const rxcpp::observable<T, SourceOperator>& source, OperatorFactory&& of)
    -> decltype(source.op(std::forward<OperatorFactory>(of))) {
    return      source.op(std::forward<OperatorFactory>(of));
}

//
// support range() | filter() | subscribe() syntax
// '|' is spelled 'pipe'
//
template<class T, class SourceOperator, class OperatorFactory>
auto operator | (const rxcpp::observable<T, SourceOperator>& source, OperatorFactory&& of)
    -> decltype(source.op(std::forward<OperatorFactory>(of))) {
    return      source.op(std::forward<OperatorFactory>(of));
}

#endif
