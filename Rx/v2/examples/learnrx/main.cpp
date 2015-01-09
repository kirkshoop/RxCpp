//
// ported from @jhusain's learnrx website
// http://reactive-extensions.github.io/learnrx/
//

#include "iostream"

#include "json.h"
using json = nlohmann::json;

#include "rxcpp/rx.hpp"
namespace rx = rxcpp;
namespace rxu = rxcpp::util;

#include "run.hpp"
#include "excercises.hpp"

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

    auto movieLists = R"(
        [
            {
                "name": "Instant Queue",
                "videos" : [
                    {
                        "id": 70111470,
                        "title": "Die Hard",
                        "boxarts": [
                            { "width": 150, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/DieHard150.jpg" },
                            { "width": 200, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/DieHard200.jpg" }
                        ],
                        "url": "http://api.netflix.com/catalog/titles/movies/70111470",
                        "rating": 4.0,
                        "bookmark": []
                    },
                    {
                        "id": 654356453,
                        "title": "Bad Boys",
                        "boxarts": [
                            { "width": 200, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/BadBoys200.jpg" },
                            { "width": 150, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/BadBoys150.jpg" }

                        ],
                        "url": "http://api.netflix.com/catalog/titles/movies/70111470",
                        "rating": 5.0,
                        "bookmark": [{ "id":432534, "time":65876586 }]
                    }
                ]
            },
            {
                "name": "New Releases",
                "videos": [
                    {
                        "id": 65432445,
                        "title": "The Chamber",
                        "boxarts": [
                            { "width": 150, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/TheChamber150.jpg" },
                            { "width": 200, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/TheChamber200.jpg" }
                        ],
                        "url": "http://api.netflix.com/catalog/titles/movies/70111470",
                        "rating": 4.0,
                        "bookmark": []
                    },
                    {
                        "id": 675465,
                        "title": "Fracture",
                        "boxarts": [
                            { "width": 200, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/Fracture200.jpg" },
                            { "width": 150, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/Fracture150.jpg" },
                            { "width": 300, "height":200, "url":"http://cdn-0.nflximg.com/images/2891/Fracture300.jpg" }
                        ],
                        "url": "http://api.netflix.com/catalog/titles/movies/70111470",
                        "rating": 5.0,
                        "bookmark": [{ "id":432534, "time":65876586 }]
                    }
                ]
            }
        ]
    )"_json;


    Exercise_11_Use_map_and_merge_to_project_and_flatten_the_movieLists_into_an_array_of_video_ids(
        [&]() {
            //return rx::observable<>::iterate(newReleases);
            return Exercise_11_Use_map_and_merge_to_project_and_flatten_the_movieLists_into_an_array_of_video_ids_ANSWER(movieLists);
        }
    );

    Exercise_12_Retrieve_id_title_and_a_150_200_box_art_url_for_every_video(
        [&]() {
            //return rx::observable<>::iterate(newReleases);
            return Exercise_12_Retrieve_id_title_and_a_150_200_box_art_url_for_every_video_ANSWER(movieLists);
        }
    );

    return 0;
}
