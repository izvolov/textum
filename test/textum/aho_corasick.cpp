#include <textum/aho_corasick.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE("Поиск совпадений возвращает все подстроки искомой строки, которые совпадают с "
    "каким-либо из образцов, которые были переданы поиковику при конструировании")
{
    const auto values =
        std::vector<std::pair<std::string, unsigned>>
        {
            {"aaaa", 1},
            {"aa", 3},
            {"a", 4},
            {"ab", 5},
            {"aba", 6},
            {"caa", 7}
        };
    const auto a = textum::aho_corasick<char, unsigned>(values);

    std::vector<unsigned> matched_values;
    a.match(std::string("aaaaabc"), std::back_inserter(matched_values));

    const auto expected_values =
        std::vector<unsigned>
        {
            1, // aaaa
            1, // aaaa
            3, // aa
            3, // aa
            3, // aa
            3, // aa
            4, // a
            4, // a
            4, // a
            4, // a
            4, // a
            5  // ab
        };
    std::sort(matched_values.begin(), matched_values.end());
    CHECK(matched_values == expected_values);
}
