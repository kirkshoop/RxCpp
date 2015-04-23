#include "rxcpp/rx.hpp"
namespace rx=rxcpp;
namespace rxu=rxcpp::util;
namespace rxs=rxcpp::sources;
namespace rxo=rxcpp::operators;
namespace rxsc=rxcpp::schedulers;

//#include "rxcpp/rx-test.hpp"
#include "catch.hpp"

const int static_onnextcalls = 1000000;
const int static_take        =  900000;


SCENARIO("range for", "[for][perf]"){
    const int& onnextcalls = static_onnextcalls;
    GIVEN("nested for loops"){
        WHEN("generating ints"){
            using namespace std::chrono;
            typedef steady_clock clock;

            std::cout << "main    thread " << std::this_thread::get_id() << std::endl;

            int n = 1;
            auto sectionCount = onnextcalls;
            auto start = clock::now();
            int c = 0;

            rx::composite_subscription cs;
            cs.add([](){
                std::cout << "dispose thread " << std::this_thread::get_id() << std::endl;
            });

            for (int i = 0; i <= 9 && cs.is_subscribed(); ++i) {
                for (int y = 0; y <= (sectionCount / 10) - 1 && cs.is_subscribed(); ++y){
                    if (c == static_take) {
                        std::cout << "output  thread " << std::this_thread::get_id() << std::endl;
                        cs.unsubscribe();
                        continue;
                    }
                    ++c;
                }
            }

            auto finish = clock::now();
            auto msElapsed = duration_cast<milliseconds>(finish.time_since_epoch()) -
                   duration_cast<milliseconds>(start.time_since_epoch());
            std::cout << "range for : " << n << " subscribed, " << c << " emitted, " << msElapsed.count() << "ms elapsed " << c / (msElapsed.count() / 1000.0) << " ops/sec" << std::endl;
        }
    }
}

SCENARIO("range immediate", "[range][immediate][perf]"){
    const int& onnextcalls = static_onnextcalls;
    GIVEN("some ranges"){
        WHEN("generating ints"){
            using namespace std::chrono;
            typedef steady_clock clock;

            std::cout << "main    thread " << std::this_thread::get_id() << std::endl;

            int n = 1;
            auto sectionCount = onnextcalls;
            auto start = clock::now();
            int c = 0;

            rx::composite_subscription cs;
            cs.add([](){
                std::cout << "dispose thread " << std::this_thread::get_id() << std::endl;
            });

            rxs::range(0, 9) |
                rxo::map([&](int ){ return rxs::range(0, (sectionCount / 10) - 1); }) |
                rxo::concat() |
                rxo::take(static_take) |
                rxo::as_blocking() |
                rxo::subscribe<int>(
                cs,
                [&](int ){++c;},
                [](std::exception_ptr e){
                try {std::rethrow_exception(e);} catch(const std::exception& ex) {std::cout << ex.what() << std::endl;}
            },
                [](){
                std::cout << "output  thread " << std::this_thread::get_id() << std::endl;
            }
            );

            auto finish = clock::now();
            auto msElapsed = duration_cast<milliseconds>(finish.time_since_epoch()) -
                duration_cast<milliseconds>(start.time_since_epoch());
            std::cout << "range : " << n << " subscribed, " << c << " emitted, " << msElapsed.count() << "ms elapsed " << c / (msElapsed.count() / 1000.0) << " ops/sec" << std::endl;
        }
    }
}

SCENARIO("range new_thread", "[range][new_thread][perf]"){
    const int& onnextcalls = static_onnextcalls;
    GIVEN("some ranges"){
        WHEN("generating ints"){
            using namespace std::chrono;
            typedef steady_clock clock;

            std::cout << "main    thread " << std::this_thread::get_id() << std::endl;

            int n = 1;
            auto sectionCount = onnextcalls;
            auto start = clock::now();
            int c = 0;

            rx::composite_subscription cs;
            cs.add([](){
                std::cout << "dispose thread " << std::this_thread::get_id() << std::endl;
            });

            auto sc = rxsc::make_new_thread();

            rxs::range(0, 9, 1, sc) |
                rxo::map([&](int ){ return rxs::range(0, (sectionCount / 10) - 1, 1, sc) | rxo::finally([](){std::cout << "nested  thread " << std::this_thread::get_id() << std::endl;}); }) |
                rxo::concat(sc) |
                rxo::take(static_take) |
                rxo::as_blocking() |
                rxo::subscribe<int>(
                    cs,
                    [&](int ){++c;},
                    [](std::exception_ptr e){
                        try {std::rethrow_exception(e);} catch(const std::exception& ex) {std::cout << ex.what() << std::endl;}
                    },
                    [](){
                        std::cout << "output  thread " << std::this_thread::get_id() << std::endl;
                    }
                );

            auto finish = clock::now();
            auto msElapsed = duration_cast<milliseconds>(finish.time_since_epoch()) -
                   duration_cast<milliseconds>(start.time_since_epoch());
            std::cout << "range : " << n << " subscribed, " << c << " emitted, " << msElapsed.count() << "ms elapsed " << c / (msElapsed.count() / 1000.0) << " ops/sec" << std::endl;
        }
    }
}

static const int static_tripletCount = 1;

SCENARIO("concat pythagorian ranges", "[range][concat][pythagorian][perf]"){
    const int& tripletCount = static_tripletCount;
    GIVEN("some ranges"){
        WHEN("generating pythagorian triplets"){
            using namespace std::chrono;
            typedef steady_clock clock;

            auto test = [&](rxsc::scheduler<> sc) {

                std::atomic<int> c = 0;
                int ct = 0;
                int n = 1;
                auto start = clock::now();

                rx::composite_subscription cs;
                cs.add([](){
                    std::cout << "dispose thread " << std::this_thread::get_id() << std::endl;
                });

                auto triples =
                    rxs::range(1, sc) |
                        rxo::map(
                            [&c, sc](int z){
                                return rxs::range(1, z, 1, sc) |
                                    rxo::map(
                                        [&c, sc, z](int x){
                                            return rxs::range(x, z, 1, sc) |
                                                rxo::filter([&c, z, x](int y){++c; return x*x + y*y == z*z;}) |
                                                rxo::map([z, x](int y){
                                                    std::cout << "y       thread " << std::this_thread::get_id() << " triplet " << x << ", " << y << ", " << z << std::endl;
                                                    return std::make_tuple(x, y, z);
                                                }) |
                                                // forget type to workaround lambda deduction bug on msvc 2013
                                                rxo::as_dynamic();
                                        }) |
                                    rxo::concat(sc) |
                                    // forget type to workaround lambda deduction bug on msvc 2013
                                    rxo::as_dynamic();
                            }) |
                        rxo::concat(sc);

                triples |
                    rxo::take(tripletCount) |
                    rxo::as_blocking() |
                    rxo::subscribe<std::tuple<int, int, int>>(
                        cs,
                        rxu::apply_to([&ct](int /*x*/,int /*y*/,int /*z*/){++ct;}),
                        [](std::exception_ptr e){
                            try {std::rethrow_exception(e);} catch(const std::exception& ex) {std::cout << ex.what() << std::endl;}
                        },
                        [](){
                            std::cout << "output  thread " << std::this_thread::get_id() << std::endl;
                        });

                auto finish = clock::now();
                auto msElapsed = duration_cast<milliseconds>(finish.time_since_epoch()) -
                       duration_cast<milliseconds>(start.time_since_epoch());
                std::cout << "concat pythagorian range : " << n << " subscribed, " << c << " filtered to, " << ct << " triplets, " << msElapsed.count() << "ms elapsed " << c / (msElapsed.count() / 1000.0) << " ops/sec" << std::endl;
            };

            THEN("new thread completes") {
                std::cout << "main    thread " << std::this_thread::get_id() << std::endl;

                auto sc = rxsc::make_new_thread();

                test(sc);
            }

            THEN("immediate completes") {
                std::cout << "main    thread " << std::this_thread::get_id() << std::endl;

                auto sc = rxsc::make_immediate();

                test(sc);
            }
        }
    }
}

