#pragma once


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
            json(654356453), json(675465)
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

template<class Lambda>
void Exercise_11_Use_map_and_merge_to_project_and_flatten_the_movieLists_into_an_array_of_video_ids(Lambda l) {
    run_test(
        __FUNCTION__,
        {
            json(70111470), json(654356453), json(65432445), json(675465),
        },
        l);
}

rx::observable<json> Exercise_11_Use_map_and_merge_to_project_and_flatten_the_movieLists_into_an_array_of_video_ids_ANSWER(json data) {
    return rx::observable<>::iterate(data).
        map([](json item) {
            return rx::observable<>::iterate(item["videos"]).
                map([](json m) {
                    return m["id"];
                });
        }).
        merge();
}

template<class Lambda>
void Exercise_12_Retrieve_id_title_and_a_150_200_box_art_url_for_every_video(Lambda l) {
    run_test(
        __FUNCTION__,
        {
            R"({ "id": 70111470, "title": "Die Hard", "url": "http://cdn-0.nflximg.com/images/2891/DieHard150.jpg" })"_json,
            R"({ "id": 65432445, "title": "The Chamber", "url": "http://cdn-0.nflximg.com/images/2891/TheChamber150.jpg" })"_json,
            R"({ "id": 654356453, "title": "Bad Boys", "url": "http://cdn-0.nflximg.com/images/2891/BadBoys150.jpg" })"_json,
            R"({ "id": 675465, "title": "Fracture", "url": "http://cdn-0.nflximg.com/images/2891/Fracture150.jpg" })"_json
        },
        l);
}

rx::observable<json> Exercise_12_Retrieve_id_title_and_a_150_200_box_art_url_for_every_video_ANSWER(json data) {
    return rx::observable<>::iterate(data).
        map([](json item) {
            return rx::observable<>::iterate(item["videos"]).
                map([](json m) {
                    auto id = m["id"];
                    auto title = m["title"];
                    return rx::observable<>::iterate(m["boxarts"]).
                        filter([](json b){
                            return int(b["width"]) == 150;
                        }).
                        map([id, title](json b) {
                            return json({
                                {"id", id},
                                {"title", title},
                                {"url", b["url"]}
                            });
                        });
                }).
                merge();
        }).
        merge();
}
