//
// ported from @jhusain's learnrx website
// http://reactive-extensions.github.io/learnrx/
//

#include "iostream"

#include "json.h"
using json = nlohmann::json;

#include "../../src/rxcpp/rx.hpp"
namespace rx = rxcpp;
namespace rxu = rxcpp::util;

template<class T, class Lambda>
void run_test(const char* name, std::initializer_list<T> il, Lambda l) {
    auto required = rxu::to_vector(il);
    auto actual = l().
        reduce(
            std::vector<T>(),
            [](std::vector<T> s, json j){
                s.push_back(j);
                return std::move(s);
            },
            [](std::vector<T>& r){
                return r;
            }).
        as_blocking().
        first();
    if (required == actual) {
        std::cout << "PASSED - " << name << std::endl;
    } else {
        std::cout << "FAILED (required == actual) - " << name << std::endl
            << required << std::endl
            << "==" << std::endl
            << actual << std::endl;
    }

}

template<class Lambda>
void Exercise_5_Use_map_to_project_an_array_of_videos_into_an_array_of_id_title_pairs(Lambda l) {
    run_test(
        __FUNCTION__,
        {
            R"({ "id": 70111470, "title": "Die Hard" })"_json,
            R"({ "id": 654356453, "title": "Bad Boys" })"_json,
            R"({ "id": 65432445, "title": "The Chamber" })"_json,
            R"({ "id": 675465, "title": "Fracture" })"_json
        },
        l);
}

rx::observable<json> Exercise_5_Use_map_to_project_an_array_of_videos_into_an_array_of_id_title_pairs_ANSWER(json data) {
    return rx::observable<>::iterate(data).
        map([](json m){
            return json({
                {"id", m["id"]},
                {"title", m["title"]}
            });
        });
}

template<class Lambda>
void Exercise_8_Chain_filter_and_map_to_collect_the_ids_of_videos_that_have_a_rating_of_5_0(Lambda l) {
    run_test(
        __FUNCTION__,
        {
            json(654356453),
            json(675465)
        },
        l);
}

rx::observable<json> Exercise_8_Chain_filter_and_map_to_collect_the_ids_of_videos_that_have_a_rating_of_5_0_ANSWER(json data) {
    return rx::observable<>::iterate(data).
        filter([](json m) {
            return double(m["rating"]) == 5.0;
        }).
        map([](json m) {
            return m["id"];
        });
}


int main() {

    auto newReleases = R"(
        [
            {
              "id": 70111470,
              "title": "Die Hard",
              "boxart": "http://cdn-0.nflximg.com/images/2891/DieHard.jpg",
              "uri": "http://api.netflix.com/catalog/titles/movies/70111470",
              "rating": 4.0,
              "bookmark": []
            },
            {
              "id": 654356453,
              "title": "Bad Boys",
              "boxart": "http://cdn-0.nflximg.com/images/2891/BadBoys.jpg",
              "uri": "http://api.netflix.com/catalog/titles/movies/70111470",
              "rating": 5.0,
              "bookmark": [{ "id":432534, "time":65876586 }]
            },
            {
              "id": 65432445,
              "title": "The Chamber",
              "boxart": "http://cdn-0.nflximg.com/images/2891/TheChamber.jpg",
              "uri": "http://api.netflix.com/catalog/titles/movies/70111470",
              "rating": 4.0,
              "bookmark": []
            },
            {
              "id": 675465,
              "title": "Fracture",
              "boxart": "http://cdn-0.nflximg.com/images/2891/Fracture.jpg",
              "uri": "http://api.netflix.com/catalog/titles/movies/70111470",
              "rating": 5.0,
              "bookmark": [{ "id":432534, "time":65876586 }]
            }
        ]
    )"_json;

    Exercise_5_Use_map_to_project_an_array_of_videos_into_an_array_of_id_title_pairs(
        [&]() {
            //return rx::observable<>::iterate(newReleases);
            return Exercise_5_Use_map_to_project_an_array_of_videos_into_an_array_of_id_title_pairs_ANSWER(newReleases);
        }
    );

    Exercise_8_Chain_filter_and_map_to_collect_the_ids_of_videos_that_have_a_rating_of_5_0(
        [&]() {
            //return rx::observable<>::iterate(newReleases);
            return Exercise_8_Chain_filter_and_map_to_collect_the_ids_of_videos_that_have_a_rating_of_5_0_ANSWER(newReleases);
        }
    );

    return 0;
}
