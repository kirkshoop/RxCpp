// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_OPERATORS_RX_CONCAT_HPP)
#define RXCPP_OPERATORS_RX_CONCAT_HPP

#include "../rx-includes.hpp"

namespace rxcpp {

namespace operators {

namespace detail {

template<class F>
struct one_and_done
{
    F f;
    template<class W>
    auto operator()(W&) -> rxsc::action_result {
        f();
        return nullptr;
    }
};
template<class F>
auto make_one_and_done(F&& f) -> one_and_done<F> {
    one_and_done<F> r = {std::forward<F>(f)};
    return r;
}

template<class T, class Worker, class OnNext, class OnError, class OnCompleted>
struct scheduled_subscriber_state : public std::enable_shared_from_this<scheduled_subscriber_state<T, Worker, OnNext, OnError, OnCompleted>>
{
    ~scheduled_subscriber_state(){
        if (!fromtoken.expired()) {
            from.remove(fromtoken);
        }
        if (!totoken.expired()) {
            to.remove(totoken);
        }
    }
    scheduled_subscriber_state(Worker w, composite_subscription f, composite_subscription t, OnNext n, OnError e, OnCompleted c)
        : worker(w)
        , from(f)
        , to(t)
        , onnext(n)
        , onerror(e)
        , oncompleted(c)
    {
    }

    Worker worker;
    composite_subscription from;
    composite_subscription to;
    OnNext onnext;
    OnError onerror;
    OnCompleted oncompleted;
    typename composite_subscription::weak_subscription fromtoken;
    typename composite_subscription::weak_subscription totoken;
};

template<class T, class Worker, class OnNext, class OnError, class OnCompleted>
struct scheduled_subscriber
{

    std::shared_ptr<scheduled_subscriber_state<T, Worker, OnNext, OnError, OnCompleted>> state;

    explicit scheduled_subscriber(std::shared_ptr<scheduled_subscriber_state<T, Worker, OnNext, OnError, OnCompleted>> s) : state(s) {}

    void on_next(T v) const {
        auto localState = state;
        localState->worker.schedule(make_one_and_done([=](){
            localState->onnext(v);
        }));
    }
    void on_error(std::exception_ptr e) const {
        auto localState = state;
        localState->worker.schedule(make_one_and_done([=](){
            localState->onerror(e);
        }));
    }
    void on_completed() const {
        auto localState = state;
        localState->worker.schedule(make_one_and_done([=](){
            localState->oncompleted();
        }));
    }
};

template<class T, class Worker, class OnNext, class OnError, class OnCompleted, class Finally>
auto make_scheduled_subscriber(Worker w, composite_subscription f, composite_subscription t, OnNext n, OnError e, OnCompleted c, Finally fin)
    -> decltype(make_subscriber<T>(composite_subscription(), make_observer<T>(*(scheduled_subscriber<T, Worker, OnNext, OnError, OnCompleted>*)nullptr))) {

    auto scrbr = std::make_shared<scheduled_subscriber_state<T, Worker, OnNext, OnError, OnCompleted>>(w, f, t, n, e, c);

    auto disposer = make_one_and_done([=](){
        if (fin()) {
            scrbr->from.unsubscribe();
            scrbr->to.unsubscribe();
            scrbr->worker.unsubscribe();
        }
    });

    scrbr->totoken = scrbr->to.add([=](){
        scrbr->worker.schedule(disposer);
    });

    scrbr->fromtoken = scrbr->from.add([=](){
        scrbr->worker.schedule(disposer);
    });

    return make_subscriber<T>(scrbr->from, make_observer<T>(scheduled_subscriber<T, Worker, OnNext, OnError, OnCompleted>(scrbr)));
}

template<class T, class Observable, class Scheduler>
struct concat
    : public operator_base<rxu::value_type_t<rxu::decay_t<T>>>
{
    typedef concat<T, Observable, Scheduler> this_type;

    typedef rxu::decay_t<T> source_value_type;
    typedef rxu::decay_t<Observable> source_type;
    typedef rxu::decay_t<Scheduler> scheduler_type;

    typedef typename scheduler_type::worker_type worker_type;
    typedef typename source_type::source_operator_type source_operator_type;
    typedef source_value_type collection_type;
    typedef typename collection_type::value_type value_type;

    struct values
    {
        values(source_operator_type o, scheduler_type sc)
            : source_operator(std::move(o))
            , scheduler(std::move(sc))
        {
        }
        source_operator_type source_operator;
        scheduler_type scheduler;
    };
    values initial;

    concat(const source_type& o, scheduler_type sc)
        : initial(o.source_operator, std::move(sc))
    {
    }

    template<class Subscriber>
    void on_subscribe(Subscriber scbr) const {
        static_assert(is_subscriber<Subscriber>::value, "subscribe must be passed a subscriber");

        typedef Subscriber output_type;

        struct concat_state_type
            : public std::enable_shared_from_this<concat_state_type>
            , public values
        {
            concat_state_type(values i, worker_type w, output_type oarg)
                : values(i)
                , source(i.source_operator)
                , sourceLifetime(composite_subscription::empty())
                , collectionLifetime(composite_subscription::empty())
                , worker(std::move(w))
                , out(std::move(oarg))
                , pending(0)
            {
            }

            void subscribe_to(collection_type st)
            {
                auto state = this->shared_from_this();

                state->collectionLifetime = composite_subscription();

                ++state->pending;

                // this subscribe does not share the out subscription
                // so that when it is unsubscribed the out will continue
                st.subscribe(make_scheduled_subscriber<value_type>(
                    state->worker,
                    state->collectionLifetime,
                    state->out.get_subscription(),
                // on_next
                    [state](value_type ct) {
                        state->out.on_next(ct);
                    },
                // on_error
                    [state](std::exception_ptr e) {
                        state->out.on_error(e);
                    },
                //on_completed
                    [state](){
                        if (!state->selectedCollections.empty()) {
                            auto value = state->selectedCollections.front();
                            state->selectedCollections.pop_front();
                            state->collectionLifetime.unsubscribe();
                            state->subscribe_to(value);
                        } else if (!state->sourceLifetime.is_subscribed()) {
                            state->out.on_completed();
                        }
                    },
                // finally
                    [state](){
                        return --state->pending == 0;
                    }
                ));
            }
            observable<source_value_type, source_operator_type> source;
            composite_subscription sourceLifetime;
            composite_subscription collectionLifetime;
            std::deque<collection_type> selectedCollections;
            worker_type worker;
            output_type out;
            int pending;
        };

        auto worker = initial.scheduler.create_worker();

        // take a copy of the values for each subscription
        auto state = std::make_shared<concat_state_type>(initial, std::move(worker), std::move(scbr));

        state->sourceLifetime = composite_subscription();

        ++state->pending;

        // this subscribe does not share the observer subscription
        // so that when it is unsubscribed the observer can be called
        // until the inner subscriptions have finished
        state->source.subscribe(make_scheduled_subscriber<T>(
            state->worker,
            state->sourceLifetime,
            state->out.get_subscription(),
        // on_next
            [state](collection_type st) {
                if (state->collectionLifetime.is_subscribed()) {
                    state->selectedCollections.push_back(st);
                } else if (state->selectedCollections.empty()) {
                    state->subscribe_to(st);
                }
            },
        // on_error
            [state](std::exception_ptr e) {
                state->out.on_error(e);
            },
        // on_completed
            [state]() {
                if (!state->collectionLifetime.is_subscribed() && state->selectedCollections.empty()) {
                    state->out.on_completed();
                }
            },
        // finally
            [state](){
                return --state->pending == 0;
            }
        ));
    }
};

template<class Scheduler>
class concat_factory
{
    typedef rxu::decay_t<Scheduler> scheduler_type;

    scheduler_type scheduler;
public:
    concat_factory(scheduler_type sc)
        : scheduler(std::move(sc))
    {
    }

    template<class Observable>
    auto operator()(Observable source)
        ->      observable<rxu::value_type_t<concat<rxu::value_type_t<Observable>, Observable, Scheduler>>,  concat<rxu::value_type_t<Observable>, Observable, Scheduler>> {
        return  observable<rxu::value_type_t<concat<rxu::value_type_t<Observable>, Observable, Scheduler>>,  concat<rxu::value_type_t<Observable>, Observable, Scheduler>>(
                                                                                                             concat<rxu::value_type_t<Observable>, Observable, Scheduler>(std::move(source), scheduler));
    }
};

}

template<class Scheduler>
auto concat(Scheduler&& sc)
    ->      detail::concat_factory<Scheduler> {
    return  detail::concat_factory<Scheduler>(std::forward<Scheduler>(sc));
}

auto concat()
    ->      decltype(concat(rxsc::make_immediate())) {
    return  concat(rxsc::make_immediate());
}

}

}

#endif
