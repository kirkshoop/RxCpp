/*
 * Getting timelines by Twitter Streaming API
 */
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <oauth.h>
#include <curl/curl.h>
#include <json.hpp>
#include <rxcpp/rx.hpp>
#include <sstream>
#include <fstream>
#include <regex>
#include <deque>
#include <map>

using namespace std;
using namespace std::chrono;
using namespace rxcpp;
using namespace rxcpp::rxo;
using namespace rxcpp::rxs;

using json=nlohmann::json;

#include "tweets.h"

#include "rxcurl.h"
using namespace rxcurl;

#include "imgui.h"
#include "imgui_impl_sdl_gl3.h"
#include <GL/glew.h>
#include <SDL.h>


#if 1

struct TimeRange
{
    using timestamp = milliseconds;

    timestamp begin;
    timestamp end;
};
bool operator<(const TimeRange& lhs, const TimeRange& rhs){
    return lhs.begin < rhs.begin && lhs.end < rhs.end;
}
struct TweetGroup
{
    vector<const shared_ptr<json>> tweets;
    std::map<string, int> words;
};
struct Model
{
    int total = 0;
    deque<TimeRange> groups;
    std::map<TimeRange, shared_ptr<TweetGroup>> groupedtweets;
    deque<int> tweetsperminute;
};
using Reducer = function<Model(Model&)>;

//http://xpo6.com/list-of-english-stop-words/
set<string> ignoredWords{"&amp", "&gt", "&lt", "&nbsp", "http", "https", "rt", "like", "just", "tomorrow", "new", "year", "month", "day", "today", "a", "about", "above", "above", "across", "after", "afterwards", "again", "against", "all", "almost", "alone", "along", "already", "also","although","always","am","among", "amongst", "amoungst", "amount",  "an", "and", "another", "any","anyhow","anyone","anything","anyway", "anywhere", "are", "around", "as",  "at", "back","be","became", "because","become","becomes", "becoming", "been", "before", "beforehand", "behind", "being", "below", "beside", "besides", "between", "beyond", "bill", "both", "bottom","but", "by", "call", "can", "cannot", "cant", "co", "con", "could", "couldnt", "cry", "de", "describe", "detail", "do", "done", "down", "due", "during", "each", "eg", "eight", "either", "eleven","else", "elsewhere", "empty", "enough", "etc", "even", "ever", "every", "everyone", "everything", "everywhere", "except", "few", "fifteen", "fify", "fill", "find", "fire", "first", "five", "for", "former", "formerly", "forty", "found", "four", "from", "front", "full", "further", "get", "give", "go", "had", "has", "hasnt", "have", "he", "hence", "her", "here", "hereafter", "hereby", "herein", "hereupon", "hers", "herself", "him", "himself", "his", "how", "however", "hundred", "ie", "if", "in", "inc", "indeed", "interest", "into", "is", "it", "its", "itself", "keep", "last", "latter", "latterly", "least", "less", "ltd", "made", "many", "may", "me", "meanwhile", "might", "mill", "mine", "more", "moreover", "most", "mostly", "move", "much", "must", "my", "myself", "name", "namely", "neither", "never", "nevertheless", "next", "nine", "no", "nobody", "none", "noone", "nor", "not", "nothing", "now", "nowhere", "of", "off", "often", "on", "once", "one", "only", "onto", "or", "other", "others", "otherwise", "our", "ours", "ourselves", "out", "over", "own","part", "per", "perhaps", "please", "put", "rather", "re", "same", "see", "seem", "seemed", "seeming", "seems", "serious", "several", "she", "should", "show", "side", "since", "sincere", "six", "sixty", "so", "some", "somehow", "someone", "something", "sometime", "sometimes", "somewhere", "still", "such", "system", "take", "ten", "than", "that", "the", "their", "them", "themselves", "then", "thence", "there", "thereafter", "thereby", "therefore", "therein", "thereupon", "these", "they", "thickv", "thin", "third", "this", "those", "though", "three", "through", "throughout", "thru", "thus", "to", "together", "too", "top", "toward", "towards", "twelve", "twenty", "two", "un", "under", "until", "up", "upon", "us", "very", "via", "was", "we", "well", "were", "what", "whatever", "when", "whence", "whenever", "where", "whereafter", "whereas", "whereby", "wherein", "whereupon", "wherever", "whether", "which", "while", "whither", "who", "whoever", "whole", "whom", "whose", "why", "will", "with", "within", "without", "would", "yet", "you", "your", "yours", "yourself", "yourselves", "the"};

#endif

inline float  Clamp(float v, float mn, float mx)                       { return (v < mn) ? mn : (v > mx) ? mx : v; }
inline ImVec2 Clamp(const ImVec2& f, const ImVec2& mn, ImVec2 mx)      { return ImVec2(Clamp(f.x,mn.x,mx.x), Clamp(f.y,mn.y,mx.y)); }

int main(int argc, const char *argv[])
{
    if (argc == 2) {
        std::ifstream infile(argv[1]);
        std::string line;
        while (std::getline(infile, line))
        {
            if (line[0] == '{') {
                line+="\r\n";
                assert(isEndOfTweet(line));
                sendchunk(line);
            }
        }
        return 0;
    }
    if (argc < 6) {
        printf("twitter CONS_KEY CONS_SECRET ATOK_KEY ATOK_SECRET [sample.json | filter.json?track=<topic>]\n");
        return -1;
    }
    // ==== Constants - URL
    string URL = "https://stream.twitter.com/1.1/statuses/";
    URL += argv[5];
    cerr << "url = " << URL.c_str() << endl;
    
    // ==== Twitter keys
    const char *CONS_KEY = argv[1];
    const char *CONS_SEC = argv[2];
    const char *ATOK_KEY = argv[3];
    const char *ATOK_SEC = argv[4];

    bool isFilter = URL.find("/statuses/filter") != string::npos;

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    SDL_Window *window = SDL_CreateWindow("ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext glcontext = SDL_GL_CreateContext(window);
    glewInit();

    // Setup ImGui binding
    ImGui_ImplSdlGL3_Init(window);

    // Load Fonts
    // (there is a default font, this is only if you want to change it. see extra_fonts/README.txt for more details)
    //ImGuiIO& io = ImGui::GetIO();
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
    //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    const ImVec4 clear_color = ImColor(114, 144, 154);
    const float fltmax = numeric_limits<float>::max();

    schedulers::run_loop rl;

    auto mainthread = observe_on_run_loop(rl);
    auto poolthread = observe_on_event_loop();

    composite_subscription lifetime;

    subjects::subject<int> framebus;
    auto frameout = framebus.get_subscriber();
    auto sendframe = [=]() {
        frameout.on_next(1);
    };
    auto frame$ = framebus.get_observable();

    auto t$ = tweet$ |
        on_error_resume_next([](std::exception_ptr ep){
            cerr << rxu::what(ep) << endl;
            return observable<>::empty<shared_ptr<json>>();
        }) |
        repeat(std::numeric_limits<int>::max()) |
        publish() |
        ref_count();

#if 1

    auto grouptpm = 
#if 0
    observable<>::empty<Reducer>();
#else
    t$ |
        group_by([](shared_ptr<json> tw) {
            auto& tweet = *tw;
            if (tweet["timestamp_ms"].is_null()) {
                return minutes(~0);
            } else {
                auto t = milliseconds(stoll(tweet["timestamp_ms"].get<string>()));
                auto m = duration_cast<minutes>(t);
                return m;
            }
        }) |
        rxo::map([=](grouped_observable<minutes, shared_ptr<json>> g){
            auto noop = Reducer([=](Model& m){return std::move(m);});
            auto group = g | 
                timeout(minutes(2)) | 
                observe_on(poolthread) |
                scan(noop, [=](Reducer&, shared_ptr<json> tw){
                    auto& tweet = *tw;
                    if (tweet["timestamp_ms"].is_null()) {
                        return noop;
                    }

                    auto words = split(tweet["text"], R"([\(\)\[\]\s,.-:;?'"]+)", Split::RemoveDelimiter); //"
                    for (auto& word: words) {
                        transform(word.begin(), word.end(), word.begin(), [=](char c){return tolower(c);});
                    }
                    words.erase(std::remove_if(words.begin(), words.end(), [=](const string& w){
                        return !(w.size() > 1 && ignoredWords.find(w) == ignoredWords.end() && URL.find(w) == string::npos);
                    }), words.end());

                    return Reducer([=](Model& m){
                        auto t = milliseconds(stoll(tweet["timestamp_ms"].get<string>()));
                        auto rangebegin = duration_cast<milliseconds>(g.get_key() - minutes(2));
                        auto rangeend = rangebegin+minutes(1);
                        auto searchend = rangeend+minutes(2);
                        auto offset = milliseconds(0);
                        for (;rangebegin+offset < searchend;offset += milliseconds(500)){
                            if (rangebegin+offset <= t && t < rangeend+offset) {
                                auto key = TimeRange{rangebegin+offset, rangeend+offset};
                                auto it = m.groupedtweets.find(key);
                                if (it == m.groupedtweets.end()) {
                                    //cout << "add (" << key.begin.count() << " - " << key.end.count() << ")" << endl;
                                    // add group
                                    m.groups.push_back(key);
                                    sort(m.groups.begin(), m.groups.end());
                                    it = m.groupedtweets.insert(make_pair(key, make_shared<TweetGroup>())).first;
                                }
                                it->second->tweets.push_back(tw);
                                for (auto& word: words) {
                                    ++it->second->words[word];
                                }
                            }
                        }
#if 1
                        while(!m.groups.empty() && m.groups.front().begin + minutes(4) < m.groups.back().end) {
                            // remove group
                            m.groupedtweets.erase(m.groups.front());
                            m.groups.pop_front();
                        }
#endif
                        return std::move(m);
                    });
                }) |
                on_error_resume_next([](std::exception_ptr ep){
                    cerr << rxu::what(ep) << endl;
                    return observable<>::empty<Reducer>();
                });
            return group;
        }) |
        merge() |
        as_dynamic();
#endif

    auto windowtpm = t$ |
        window_with_time(milliseconds(60000), milliseconds(500), poolthread) |
        rxo::map([](observable<shared_ptr<json>> source){
            auto tweetsperminute = source | count() | rxo::map([](int count){
                return Reducer([=](Model& m){
                    m.tweetsperminute.push_back(count);
                    while(m.tweetsperminute.size() > (2 * 60 * 2)) {
                        m.tweetsperminute.pop_front();
                    }
                    return std::move(m);
                });
            }) |
            on_error_resume_next([](std::exception_ptr ep){
                cerr << rxu::what(ep) << endl;
                return observable<>::empty<Reducer>();
            });
            return tweetsperminute;
        }) |
        merge() |
        as_dynamic();

    auto total = t$ |
        rxo::map([](const shared_ptr<json>&){
            return Reducer([=](Model& m){
                ++m.total;
                return std::move(m);
            });
        }) |
        on_error_resume_next([](std::exception_ptr ep){
            cerr << rxu::what(ep) << endl;
            return observable<>::empty<Reducer>();
        }) |
        as_dynamic();

    auto reducer$ = from(windowtpm, grouptpm, total) |
        merge(poolthread);

    auto model$ = reducer$ |
        scan(Model{}, [](Model& m, Reducer& f){
            return f(m);
        }) | 
        start_with(Model{}) |
        debounce(poolthread, milliseconds(100));

    // render models
    frame$ |
        with_latest_from(mainthread, [=](int, const Model& m){
            {
                ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
                ImGui::Begin("Twitter Window");

                ImGui::TextWrapped("url: %s", URL.c_str());
                ImGui::Text("Live Analysis");

                ImGui::Text("Total Tweets: %d", m.total);

                // by window
                if (ImGui::CollapsingHeader("Tweets Per Minute (windowed)"))
                {
                    static vector<float> tpm;
                    tpm.clear();
                    transform(m.tweetsperminute.begin(), m.tweetsperminute.end(), back_inserter(tpm), [](int count){return static_cast<float>(count);});
                    ImGui::PlotLines("", &tpm[0], tpm.size(), 0, nullptr, fltmax, fltmax, ImVec2(0,100));
                }

                // by group
                if (ImGui::CollapsingHeader("Tweets Per Minute (grouped)"))
                {
                    static vector<float> tpm;
                    tpm.clear();
#if 0
                    transform(m.groups.begin(), m.groups.end(), back_inserter(tpm), [&](const TimeRange& group){
                        auto& tg = m.groupedtweets.at(group);
                        return static_cast<float>(tg.tweets.size());
                    });
#else
                    static vector<pair<milliseconds, float>> groups;
                    groups.clear();
                    transform(m.groupedtweets.begin(), m.groupedtweets.end(), back_inserter(groups), [&](const pair<TimeRange, shared_ptr<TweetGroup>>& group){
                        return make_pair(group.first.begin, static_cast<float>(group.second->tweets.size()));
                    });
                    sort(groups.begin(), groups.end(), [](const pair<milliseconds, float>& l, const pair<milliseconds, float>& r){
                        return l.first < r.first;
                    });
                    transform(groups.begin(), groups.end(), back_inserter(tpm), [&](const pair<milliseconds, float>& group){
                        return group.second;
                    });
#endif
                    ImVec2 plotposition = ImGui::GetCursorScreenPos();
                    ImVec2 plotextent(ImGui::GetContentRegionAvailWidth(),100);
                    ImGui::PlotLines("", &tpm[0], tpm.size(), 0, nullptr, fltmax, fltmax, plotextent);
                    if (ImGui::IsItemHovered()) {
                        const float t = Clamp((ImGui::GetMousePos().x - plotposition.x) / plotextent.x, 0.0f, 0.9999f);
                        const int idx = (int)(t * (tpm.size() - 1));
                        if (idx >= 0 && idx < int(tpm.size())) {
                            auto& window = m.groups.at(idx);
                            auto& group = m.groupedtweets.at(window);

                            ImGui::Columns(2);
                            ImGui::Text("Start: %lld", window.begin.count());
                            ImGui::Text("Tweets: %ld", group->tweets.size());
                            ImGui::Text("Words: %ld", group->words.size());

                            ImGui::NextColumn();
                            ImGui::Text("Top 5 words:");
#if 1
                            static vector<pair<string, int>> words;
                            words.clear();
                            copy(group->words.begin(), group->words.end(), back_inserter(words));
                            sort(words.begin(), words.end(), [](const pair<string, int>& l, const pair<string, int>& r){
                                return l.second > r.second;
                            });
                            words.resize(5);
                            for (auto& w : words) {
                                ImGui::Text("%d - %s", w.second, w.first.c_str());
                            }
#endif
                        }
                    }
                }

                ImGui::End();
            }
            return m;
        }, model$) |
        subscribe<Model>([](const Model&){});

#if 0
    model$ |
        subscribe<Model>(lifetime, [=](const Model& ){
#if 0
            for(auto count : m.tweetsperminute){
                cout << count << ", ";
            }
            cout << endl;
#endif

#if 0
            if (!m.groups.empty()) {
                auto end = m.groups.end();
                auto cursor = m.groups.begin();
#if 0
                auto viewDuration = seconds(10);
                auto windowDuration = minutes(2) + viewDuration;

                auto beginTime = cursor->begin + max(milliseconds(0), (m.groups.back().end - m.groups.front().begin) - windowDuration);
                auto endTime = beginTime + viewDuration;

                cursor = find_if(cursor, end, [=](const TimeRange& k){
                    return beginTime < k.begin;
                });
                end = find_if(cursor, end, [=](const TimeRange& k){
                    return endTime < k.begin;
                });
#endif

                for (;cursor != end; ++cursor) {
                    auto& tg = m.groupedtweets.at(*cursor);
                    cout << tg.tweets.size() << ", ";
                }
                cout << endl;
            }
#endif

        });
#endif

#endif

#if 1
    t$ |
        subscribe<shared_ptr<json>>([](const shared_ptr<json>& tw){
            auto& tweet = *tw;
            if (!tweet["delete"].is_null()) {
                cout << "delete - id=" << tweet["delete"]["status"]["id_str"] << endl;
            }
            else if (!tweet["user"].is_null() && !tweet["text"].is_null()) {
                cout << tweet["user"]["name"] << " (" << tweet["user"]["screen_name"] << ") - " << tweet["text"] << endl;
            }
            else {
                cout << tweet << endl;
            }
        });
#endif

    string method = isFilter ? "POST" : "GET";

    char* signedurl;
    signedurl = oauth_sign_url2(
        URL.c_str(), NULL, OA_HMAC, method.c_str(),
        CONS_KEY, CONS_SEC, ATOK_KEY, ATOK_SEC
    );
    string url{signedurl};
    free(signedurl);

    auto factory = create_rxcurl();

    factory.create(http_request{url, method})
        .map([](http_response r){
            return r.body.chunks;
        })
        .merge()
        .subscribe(chunkbus.get_subscriber());

    // main loop
    while(lifetime.is_subscribed()) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSdlGL3_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                lifetime.unsubscribe();
            }
        }

        ImGui_ImplSdlGL3_NewFrame(window);

        sendframe();

        while (!rl.empty() && rl.peek().when < rl.now()) {
            rl.dispatch();
        }

        // 1. Show a simple window
        // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
        {
            ImGui::Text("Hello, world!");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }

        // Rendering
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplSdlGL3_Shutdown();
    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}