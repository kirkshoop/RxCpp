#pragma once

template<class T, class Lambda>
void run_test(const char* name, std::initializer_list<T> il, Lambda l) {
    std::cout << name << std::endl;
    auto required = rxu::to_vector(il);
    auto actual = l().
        reduce(
            std::vector<T>(),
            [](std::vector<T> s, T n){
                s.push_back(n);
                return std::move(s);
            },
            [](std::vector<T>& r){
                return r;
            }).
        as_blocking().
        first();
    if (required == actual) {
        for (auto& r : actual) {
            std::cout << r << std::endl;
        }
        std::cout << "PASSED" << std::endl;
    } else {
        for (auto& r : required) {
            std::cout << r << std::endl;
        }
        std::cout << "!=" << std::endl;
        for (auto& r : actual) {
            std::cout << r << std::endl;
        }
        std::cout << "FAILED (required != actual)" << std::endl;
    }

}
