#pragma once

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
