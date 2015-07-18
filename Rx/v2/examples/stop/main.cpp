
#include "rxcpp/lite/rx.hpp"
// create alias' to simplify code
// these are owned by the user so that
// conflicts can be managed by the user.
namespace rx=rxcpp;
namespace rxs=rxcpp::sources;
namespace rxo=rxcpp::operators;
namespace rxsub=rxcpp::subjects;
namespace rxu=rxcpp::util;

// At this time, RxCpp will fail to compile if the contents
// of the std namespace are merged into the global namespace
// DO NOT USE: 'using namespace std;'

int main()
{
    // works
    {
        auto published_observable =
            rxs::range(1) |
            rxo::filter([](long i)
            {
                std::cout << i << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                return true;
            }) |
            rxo::subscribe_on(rx::observe_on_new_thread()) |
            rxo::publish();

        auto subscription = published_observable.connect();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        subscription.unsubscribe();
        std::cout << "unsubscribed" << std::endl << std::endl;
    }

    // idiomatic (prefer operators)
    {
        auto published_observable =
            rxs::interval(std::chrono::milliseconds(300)) |
            rxo::subscribe_on(rx::observe_on_new_thread()) |
            rxo::publish();

        published_observable |
            rxo::ref_count() |
            rxo::take_until(rxs::timer(std::chrono::seconds(1))) |
            rxo::finally([](){
                std::cout << "unsubscribed" << std::endl << std::endl;
            }) |
            rxo::subscribe<long>([](long i){
                std::cout << i << std::endl;
            });
    }

    return 0;
}
