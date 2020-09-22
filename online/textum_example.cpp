#include <textum/aho_corasick.hpp>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

int main ()
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcd", 1},
            {"qwerty", 2},
        };
    const auto a = textum::aho_corasick<char, long>(values);

    std::vector<std::pair<long, std::string::difference_type>> matched_values;
    a.find(textum::levenshtein(1), std::string("bcd"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, std::string::difference_type>>
        {
            {1, 1}, // abcd
        };
    assert(matched_values == expected_values);
}
