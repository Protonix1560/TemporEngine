#include <iostream>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <locale>
#include <utility>
#include <vector>
#include <concepts>

#include <mat/interval_union.hpp>


bool iequals(std::string_view a, std::string_view b, const std::locale& loc = std::locale{}) {
    if (a.size() != b.size()) return false;
    const auto& ct = std::use_facet<std::ctype<char>>(loc);
    for (std::size_t i = 0; i < a.size(); ++i) if (ct.tolower(a[i]) != ct.tolower(b[i])) return false;
    return true;
}

template <typename B>
std::string print_set(const mat::interval<B>& inter) {
    std::ostringstream buf;
    if (inter.first_included()) buf << "[";
    else buf << "(";
    buf << inter.set_begin() << "; " << inter.set_end();
    if (inter.last_included()) buf << "]";
    else buf << ")";
    return buf.str();
}

template <typename B>
std::string print_set(const mat::interval_union<B>& un) {
    std::ostringstream buf;
    for (const auto& el : un) {
        if (&el != &un.front()) buf << " U ";
        buf << print_set(el);
    }
    return buf.str();
}

template <typename T>
std::string print_set(const mat::view_interval<const T>& inter) {
    std::ostringstream buf;
    buf << inter.set_begin() << "{";
    for (const auto& el : inter) {
        buf << el;
        if (&el != &inter.back()) buf << "; ";
    }
    buf << "}";
    return buf.str();
}
template <typename T, typename C> requires std::same_as<T, typename C::value_type>
std::string print_set(const mat::data_interval_union<T, C>& un) {
    std::ostringstream buf;
    for (const auto& el : un) {
        buf << print_set(el.cview());
        if (&el != &un.back()) buf << " U ";
    }
    return buf.str();
}


int test(std::string_view a, std::string_view b) {
    if (iequals(a, b)) return 0;
    std::cout << "\e[100m " << a << " \e[31m!=\e[39m " << b << " \e[0m\n";
    return 1;
}

#define TEST(a, b) do { \
    auto result = test(a, b); \
    if (result) return result; \
} while (0)

#define TESTE(...) do { \
    auto result = (__VA_ARGS__); \
    if (!result) { \
        std::cout << "\e[100m " << #__VA_ARGS__ << " \e[31m== false\e[39m \e[0m\n"; \
        return result; \
    } \
} while (0)

int main() {

    // interval
    {
        size_t test_num = 1;
        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::interval<size_t> a{0, 3};
            mat::interval<size_t> b{1, 5};
            TEST(print_set(a | b), "[0; 5)");
            TEST(print_set(a | b), print_set(b | a));
            TEST(print_set(a & b), "[1; 3)");
            TEST(print_set(a & b), print_set(b & a));
            TEST(print_set(mat::interval_union<size_t>(a ^ b)), "[0; 1) U [3; 5)");
            TEST(print_set(mat::interval_union<size_t>(a ^ b)), print_set(mat::interval_union<size_t>(b ^ a)));
            TEST(print_set(mat::interval_union<size_t>(a - b)), "[0; 1)");
            TEST(print_set(mat::interval_union<size_t>(b-  a)), "[3; 5)");
        }
        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::interval<size_t> a{0, 3};
            mat::interval<size_t> b{3, 5, false};
            TEST(print_set(a | b), "[0; 5)");
            TESTE((a & b).equivalent(mat::interval<size_t>::empty_set()));
            TEST(print_set(mat::interval_union<size_t>(a - b)), "[0; 3)");
            TEST(print_set(mat::interval_union<size_t>(a ^ b)), "[0; 3) U (3; 5)");
        }
        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            TEST(print_set(mat::interval<size_t>{0, 5} | mat::interval<size_t>::empty_set()), "[0; 5)");
            TESTE((mat::interval<size_t>{0, 5} & mat::interval<size_t>::empty_set()).equivalent(mat::interval<size_t>::empty_set()));
            TEST(print_set(mat::interval_union<size_t>(mat::interval<size_t>{0, 5} ^ mat::interval<size_t>::empty_set())), "[0; 5)");
            TEST(print_set(mat::interval_union<size_t>(mat::interval<size_t>{0, 5} - mat::interval<size_t>::empty_set())), "[0; 5)");
        }
        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            TESTE(mat::interval<size_t>{0, 5}.equivalent(mat::interval<size_t>{0, 5}));
            TESTE(!mat::interval<size_t>{0, 5}.equivalent(mat::interval<size_t>{0, 5, false}));
        }
        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            TESTE(mat::interval<size_t>{5, 5, false, false}.equivalent(mat::interval<size_t>::empty_set()));
            TESTE(!mat::interval<size_t>{0, 5}.equivalent(mat::interval<size_t>::empty_set()));
        }

        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::interval_union<size_t> un{};
            un |= mat::interval<size_t>{10, 20};
            un |= mat::interval<size_t>{21, 30};
            TEST(print_set(un), "[10; 20) U [21; 30)");
        }
        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::interval<size_t> a{0, 1};
            mat::interval<size_t> b{1, 2, false};
            mat::interval_union<size_t> un{};
            un |= a;
            un |= b;
            TEST(print_set(un), "[0; 1) U (1; 2)");
        }
        {
            std::cout << "interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::interval_union<size_t> un;
            un |= {1, 5};
            un |= {7, 8};
            un |= {10, 12};
            un -= {4, 11};
            TEST(print_set(un), "[1; 4) U [11; 12)");
        }
    }

    // data interval
    {
        size_t test_num = 1;
        {
            std::cout << "data interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::data_interval<int> a{{0, 1, 2, 3}, 5};
            mat::data_interval<int> b{{0, -1, -2, -3}, 7};
            a.merge_with(b);
            TEST(print_set<int>(a), "5{0; 1; 0; -1; -2; -3}");
        }
        {
            std::cout << "data interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::data_interval<int> a{{0, 1, 2, 3}, 5};
            mat::data_interval<int> b{{0, -1, -2, -3}, 7};
            a.merge_with(std::move(b), mat::comb_keep<int>);
            TEST(print_set<int>(a), "5{0; 1; 2; 3; -2; -3}");
        }
        {
            std::cout << "data interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::data_interval<int> a{{0, 1, 2, 3}, 0};
            mat::data_interval<int> b{{0, -1, -2, -3}, 4};
            a.merge_with(std::move(b));
            TEST(print_set<int>(a), "0{0; 1; 2; 3; 0; -1; -2; -3}");
        }
        {
            std::cout << "data interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::data_interval<int> a{{0, 1, 2, 3}, 0};
            mat::data_interval<int> b{{0, -1, -2, -3}, 4};
            b.merge_with(std::move(a));
            TEST(print_set<int>(b), "0{0; 1; 2; 3; 0; -1; -2; -3}");
        }
        {
            std::cout << "data interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::data_interval<int> a{{0, 1, 2, 3}, 0};
            mat::data_interval<int> b{{0, -1, -2, -3}, 10};
            std::vector<int> v(14, 99);
            mat::data_interval_union<int> un;
            un |= a;
            std::cout << print_set<int>(un) << "\n";
            un |= b;
            std::cout << print_set<int>(un) << "\n";
            un.copy_subset({0, 14}, v.begin());
            TEST(print_set(mat::view_interval<const int>{v.begin(), v.end()}), "0{0; 1; 2; 3; 99; 99; 99; 99; 99; 99; 0; -1; -2; -3}");
        }
        {
            std::cout << "data interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::data_interval<int> a{{0, 1, 2, 3}, 6};
            mat::data_interval<int> b{{0, -1, -2, -3}, 10};
            std::vector<int> v(14, 99);
            mat::data_interval_union<int> un;
            un |= a;
            un |= b;
            un.copy_subset({0, 14}, v.begin());
            TEST(print_set(mat::view_interval<const int>{v.begin(), v.end()}), "0{99; 99; 99; 99; 99; 99; 0; 1; 2; 3; 0; -1; -2; -3}");
        }
        {
            std::cout << "data interval test " << test_num << "\n"; std::cout.flush(); test_num++;
            mat::data_interval<int> a{{0, 1, 2, 3}, 6};
            mat::data_interval<int> b{{0, -1, -2, -3}, 10};
            std::vector<int> v(14, 99);
            mat::data_interval_union<int> un;
            un |= a;
            un |= b;
            mat::interval_union<size_t> sel;
            sel |= {4, 5};
            sel |= {6, 8};
            sel |= {12, 15};
            un.copy_subset(sel, v.begin() + static_cast<decltype(v)::difference_type>(sel.set_begin()));
            TEST(print_set(mat::view_interval<const int>{v.begin(), v.end()}), "0{99; 99; 99; 99; 99; 99; 0; 1; 99; 99; 99; 99; -2; -3}");
        }
    }

    return 0;
}
