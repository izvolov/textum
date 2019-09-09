#include <textum/aho_corasick.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE("Сконструированный по умолчанию поисковик пуст")
{
    textum::aho_corasick<char, int> a;
    CHECK(a.empty());
}

TEST_CASE("Размер пустого поисковика равен нулю (в множестве нет ни одной строки)")
{
    textum::aho_corasick<char, int> a;
    CHECK(a.size() == 0);
}

TEST_CASE("Поисковик, сконструированный из непустого диапазона, непуст")
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = textum::aho_corasick<char, int>(values);

    CHECK(not a.empty());
}

TEST_CASE("Размер поисковика равен размеру диапазона, переданного ему при конструировании")
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = textum::aho_corasick<char, int>(values);

    CHECK(a.size() == values.size());
}

TEST_CASE("Повторяющиеся строки не заносятся в поисковик")
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 1},
            {"qwerty", 2},
            {"qwe", 3},
            {"qwe", 4}
        };
    const auto a = textum::aho_corasick<char, int>(values);

    CHECK(a.size() == 2);
}

TEST_CASE("Строки, переданные поисковику при конструировании, находятся методом \"find\"")
{
    const auto values =
        std::vector<std::pair<std::string, short>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = textum::aho_corasick<char, short>(values);

    CHECK(a.find(std::string("qwerty")) != a.end());
    CHECK(a.find(std::string("asdfgh")) != a.end());
    CHECK(a.find(std::string("qwe")) != a.end());
    CHECK(a.find(std::string("rty")) != a.end());
}

TEST_CASE("Найденный итератор указывает на значение, соответствующее искомой строке")
{
    const auto values =
        std::vector<std::pair<std::string, short>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = textum::aho_corasick<char, short>(values);

    CHECK(*a.find(std::string("qwerty")) == 1);
    CHECK(*a.find(std::string("asdfgh")) == 2);
    CHECK(*a.find(std::string("qwe")) == 3);
    CHECK(*a.find(std::string("rty")) == 4);
}

TEST_CASE("Если при создании поисковика есть повторяющиеся строки, то к этой строке привязывается "
    "значение первой из встреченных повторяющихся строк")
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 11},
            {"qwerty", 22},
            {"qwe", 33},
            {"qwe", 44}
        };
    const auto a = textum::aho_corasick<char, int>(values);

    CHECK(*a.find(std::string("qwerty")) == 11);
    CHECK(*a.find(std::string("qwe")) == 33);
}

TEST_CASE("Поиск строк, которые не участвовали в конструировании, возвращает итератор на конец")
{
    const auto values =
        std::vector<std::pair<std::string, short>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = textum::aho_corasick<char, short>(values);

    CHECK(a.find(std::string("qwert")) == a.end());
    CHECK(a.find(std::string("aadfgh")) == a.end());
    CHECK(a.find(std::string("we")) == a.end());
    CHECK(a.find(std::string("y")) == a.end());
}

TEST_CASE("Поиск не зависит от порядка элементов в диапазоне при конструировании")
{
    {
        const auto values =
            std::vector<std::pair<std::string, short>>
            {
                {"qwerty", 1},
                {"qwe", 3},
                {"rty", 4}
            };
        const auto a = textum::aho_corasick<char, short>(values);

        CHECK(*a.find(std::string("qwerty")) == 1);
        CHECK(*a.find(std::string("qwe")) == 3);
        CHECK(*a.find(std::string("rty")) == 4);
    }
    {
        const auto values =
            std::vector<std::pair<std::string, short>>
            {
                {"qwe", 3},
                {"qwerty", 1},
                {"rty", 4}
            };
        const auto a = textum::aho_corasick<char, short>(values);

        CHECK(*a.find(std::string("qwerty")) == 1);
        CHECK(*a.find(std::string("qwe")) == 3);
        CHECK(*a.find(std::string("rty")) == 4);
    }
    {
        const auto values =
            std::vector<std::pair<std::string, short>>
            {
                {"rty", 4},
                {"qwe", 3},
                {"qwerty", 1}
            };
        const auto a = textum::aho_corasick<char, short>(values);

        CHECK(*a.find(std::string("qwerty")) == 1);
        CHECK(*a.find(std::string("qwe")) == 3);
        CHECK(*a.find(std::string("rty")) == 4);
    }
}

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

TEST_CASE("Поиск по редакционному расстоянию записывает в выходной итератор пары, состоящие из "
    "расстояния до совпадающей строки, а также значения, соответствующего этой строке")
{
    const auto values =
        std::vector<std::pair<std::string, unsigned long>>
        {
            {"abcdef", 1},
            {"abcabc", 2},
            {"bcdefg", 3},
        };
    const auto a = textum::aho_corasick<char, long>(values);

    std::vector<std::pair<unsigned long, std::string::difference_type>> matched_values;
    a.find_levenshtein(std::string("abcdef"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<unsigned long, std::string::difference_type>>
        {
            {1, 0}, // abcdef
            {2, 3}, // abcabc
            {3, 2}, // bcdefg
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE("Поиск по редакционному расстоянию находит даже те строки, в которых нет ни одного "
    "совпадения с исходной строкой")
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcd", 1},
            {"qwerty", 2},
        };
    const auto a = textum::aho_corasick<char, long>(values);

    std::vector<std::pair<long, std::string::difference_type>> matched_values;
    a.find_levenshtein(std::string("bcd"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, std::string::difference_type>>
        {
            {1, 1},
            {2, 6}
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE("Поиск по редакционному расстоянию может возвращать только те результаты, которые "
    "удалены от искомой строки не более чем на заданную величину")
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcd", 1},
            {"qwerty", 2},
        };
    const auto a = textum::aho_corasick<char, long>(values);

    std::vector<std::pair<long, std::string::difference_type>> matched_values;
    a.find_levenshtein(textum::levenshtein(1), std::string("bcd"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, std::string::difference_type>>
        {
            {1, 1}, // abcd
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE("Максимальное редакционное расстояние может быть задано числом с плавающей точкой")
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcd", 1},
            {"aaad", 2},
        };
    const auto a = textum::aho_corasick<char, long>(values);

    std::vector<std::pair<long, double>> matched_values;
    a.find_levenshtein(textum::levenshtein(2.5), std::string("bd"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, double>>
        {
            {1, 2.0}, // abcd
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE("Префиксный поиск находит все принимаемые автоматом последовательности, до которых можно "
    "дойти из вершины, в которую привёл префикс")
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcdef", 1},
            {"abcabc", 2},
            {"abc",    6},
            {"ab",     7},
            {"bcdefg", 3},
            {"abdefg", 4},
        };
    const auto a = textum::aho_corasick<char, long>(values);

    std::vector<long> found_values;
    a.find_prefix(std::string("abc"), std::back_inserter(found_values));
    std::sort(found_values.begin(), found_values.end());

    const auto expected_values =
        std::vector<long>
        {
            1, // abcdef
            2, // abcabc
            6, // abc
        };
    CHECK(found_values == expected_values);
}
