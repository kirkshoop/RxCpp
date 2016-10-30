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

using namespace std;
using namespace std::chrono;
using namespace rxcpp;
using namespace rxcpp::rxo;
using namespace rxcpp::rxs;

using json=nlohmann::json;

subjects::subject<string> chunkbus;
auto chunkout = chunkbus.get_subscriber();
auto sendchunk = [] (const string& chunk) {
    chunkout.on_next(chunk);
};
auto chunk$ = chunkbus.get_observable();

//
// recover lines of text from chunk stream
//

auto isEndOfTweet = [](const string& s){
    if (s.size() < 2) return false;
    auto it0 = s.begin() + (s.size() - 2);
    auto it1 = s.begin() + (s.size() - 1);
    return *it0 == '\r' && *it1 == '\n';
};

auto split = [](string s, string d){
    regex delim(d);
    cregex_token_iterator cursor(&s[0], &s[0] + s.size(), delim, {-1, 0});
    cregex_token_iterator end;
    vector<string> splits(cursor, end);
    return splits;
};

// create strings split on \r
auto string$ = chunk$ |
    concat_map([](const string& s){
        auto splits = split(s, "\r\n");
        return iterate(move(splits));
    }) |
    filter([](const string& s){
        return !s.empty();
    }) |
    publish() |
    ref_count();

#if 0
int group = 0;
auto linewindow$ = string$ |
    group_by([](const string& s) {
        return isEndOfTweet(s) ? group++ : group;
    });

// reduce the strings for a line into one string
auto line$ = linewindow$ |
    flat_map([](const grouped_observable<int, string>& w$){
        return w$ | start_with<string>("") | sum();
    });
#else
// filter to last string in each line
auto close$ = string$ |
    filter(isEndOfTweet) |
    rxo::map([](const string&){return 0;});

// group strings by line
auto linewindow$ = string$ |
    window_toggle(close$ | start_with(0), [](int){return close$;});

// reduce the strings for a line into one string
auto line$ = linewindow$ |
    flat_map([](const observable<string>& w) {
        return w | start_with<string>("") | sum();
    });
#endif

auto tweet$ = line$ |
    filter([](const string& s){
        return s.size() > 2;
    })| 
    rxo::map([](const string& line){
        return json::parse(line);
    });

size_t fncCallback(char* ptr, size_t size, size_t nmemb, string* stream) {
    int iRealSize = size * nmemb;
//    stream->append(ptr, iRealSize);
//    string str = *stream;
//    cout << (str.c_str() + (str.size() - iRealSize));
    stream->assign(ptr, iRealSize);
    sendchunk(*stream);
    return iRealSize;
}

class Proc
{
    string cUrl;
    const char* cConsKey;
    const char* cConsSec;
    const char* cAtokKey;
    const char* cAtokSec;
    CURL        *curl;
    char*       cSignedUrl;
    string      chunk;
public:
    Proc(string, const char*, const char*, const char*, const char*);
    void execProc();
};

// Constructor
Proc::Proc(
    string cUrl,
    const char* cConsKey, const char* cConsSec,
    const char* cAtokKey, const char* cAtokSec)
{
    this->cUrl     = cUrl;
    this->cConsKey = cConsKey;
    this->cConsSec = cConsSec;
    this->cAtokKey = cAtokKey;
    this->cAtokSec = cAtokSec;
}

void Proc::execProc()
{
    // ==== cURL Initialization
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        cout << "[ERROR] curl_easy_init" << endl;
        curl_global_cleanup();
        return;
    }

    bool isFilter = cUrl.find("/statuses/filter") != string::npos;
    if (isFilter) {
    }

    // ==== cURL Setting
    // - URL, POST parameters, OAuth signing method, HTTP method, OAuth keys
    cSignedUrl = oauth_sign_url2(
        cUrl.c_str(), NULL, OA_HMAC, isFilter ? "POST" : "GET",
        cConsKey, cConsSec, cAtokKey, cAtokSec
    );
    // - URL
    curl_easy_setopt(curl, CURLOPT_URL, cSignedUrl);
    if (isFilter) {
        // - POST data
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        // - specify the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    }
    // - User agent name
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "rxcpp stream analisys");
    // - HTTP STATUS >=400 ---> ERROR
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    // - Callback function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fncCallback);
    // - Write data
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    // ==== Execute
    int iStatus = curl_easy_perform(curl);
    if (!iStatus)
        cout << "[ERROR] curl_easy_perform: STATUS=" << iStatus << endl;

    // ==== cURL Cleanup
    curl_easy_cleanup(curl);
    curl_global_cleanup();
}

int main(int argc, const char *argv[])
{
    auto t$ = tweet$ |
        on_error_resume_next([](std::exception_ptr ep){
            cerr << rxu::what(ep) << endl;
            return observable<>::empty<json>();
        }) |
        repeat(std::numeric_limits<int>::max()) |
        publish() |
        ref_count();

    struct Model
    {
        deque<int> tweetsperminute;
    };
    using Reducer = function<Model(Model&)>;

    t$ |
        rxo::map([](json){return 1;}) |
        window_with_time(milliseconds(60000), milliseconds(500), rxcpp::observe_on_new_thread()) |
        rxo::map([](observable<int> source){
//            cout << "+window" << endl;
            auto tweetsperminute = source | count() | rxo::map([](int count){
//                cout << "-window" << endl;
                return Reducer([=](Model& m){
//                    cout << count << endl;
                    m.tweetsperminute.push_back(count);
                    while(m.tweetsperminute.size() > 20) m.tweetsperminute.pop_front();
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
        scan(Model{}, [](Model& m, Reducer& f){
//            cout << "scan" << endl;
            return f(m);
        }) |
        subscribe<Model>([](const Model& m){
            for(auto count : m.tweetsperminute){
                cout << count << ", ";
            }
            cout << endl;
        });

#if 0
    t$ |
        subscribe<json>([](json tweet){
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

    // ==== Instantiation
    Proc objProc(URL, CONS_KEY, CONS_SEC, ATOK_KEY, ATOK_SEC);

    // ==== Main proccess
    objProc.execProc();

    return 0;
}