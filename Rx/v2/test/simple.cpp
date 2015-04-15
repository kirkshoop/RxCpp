#include "rxcpp/rx.hpp"
namespace rx=rxcpp;
namespace rxu=rxcpp::util;
namespace rxs=rxcpp::sources;
namespace rxo=rxcpp::operators;
namespace rxsc=rxcpp::schedulers;

//#include "rxcpp/rx-test.hpp"
#include "catch.hpp"

const int static_onnextcalls = 10000000;


SCENARIO("range", "[range][perf]"){
    const int& onnextcalls = static_onnextcalls;
    GIVEN("some ranges"){
        WHEN("generating ints"){
            using namespace std::chrono;
            typedef steady_clock clock;

            int n = 1;
            auto sectionCount = onnextcalls;
            auto start = clock::now();
            int c = 0;

            auto count = rx::make_subscriber<int>([&](int ){++c;});

            rxs::range(0, 9) |
                rxo::filter([](int){ return true; }) |
                rxo::map([&](int ){ return rxs::range(0, sectionCount - 1); }) |
                rxo::concat() |
                rxo::as_blocking() |
                rxo::subscribe<int>([&](int ){++c;});

            auto finish = clock::now();
            auto msElapsed = duration_cast<milliseconds>(finish.time_since_epoch()) -
                   duration_cast<milliseconds>(start.time_since_epoch());
            std::cout << "range : " << n << " subscribed, " << c << " emitted, " << msElapsed.count() << "ms elapsed " << c / (msElapsed.count() / 1000.0) << " ops/sec" << std::endl;
        }
    }
}


