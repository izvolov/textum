#include <textum/aho_corasick.hpp>
#include <textum/trie.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE_TEMPLATE("Сконструированный по умолчанию поисковик пуст",
    searcher, textum::trie<char, int>, textum::aho_corasick<char, int>)
{
    searcher a;
    CHECK(a.empty());
}

TEST_CASE_TEMPLATE("Размер пустого поисковика равен нулю (в множестве нет ни одной строки)",
    searcher, textum::trie<char, int>, textum::aho_corasick<char, int>)
{
    searcher a;
    CHECK(a.size() == 0);
}

TEST_CASE_TEMPLATE("Поисковик, сконструированный из непустого диапазона, непуст",
    searcher, textum::trie<char, int>, textum::aho_corasick<char, int>)
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = searcher(values);

    CHECK(not a.empty());
}

TEST_CASE_TEMPLATE("Размер поисковика равен размеру диапазона, переданного ему при конструировании",
    searcher, textum::trie<char, int>, textum::aho_corasick<char, int>)
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = searcher(values);

    CHECK(a.size() == values.size());
}

TEST_CASE_TEMPLATE("Повторяющиеся строки не заносятся в поисковик",
    searcher, textum::trie<char, int>, textum::aho_corasick<char, int>)
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 1},
            {"qwerty", 2},
            {"qwe", 3},
            {"qwe", 4}
        };
    const auto a = searcher(values);

    CHECK(a.size() == 2);
}

TEST_CASE_TEMPLATE("Строки, переданные поисковику при конструировании, находятся методом \"find\"",
    searcher, textum::trie<char, short>, textum::aho_corasick<char, short>)
{
    const auto values =
        std::vector<std::pair<std::string, short>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = searcher(values);

    CHECK(a.find(std::string("qwerty")) != a.end());
    CHECK(a.find(std::string("asdfgh")) != a.end());
    CHECK(a.find(std::string("qwe")) != a.end());
    CHECK(a.find(std::string("rty")) != a.end());
}

TEST_CASE_TEMPLATE("Найденный итератор указывает на значение, соответствующее искомой строке",
    searcher, textum::trie<char, short>, textum::aho_corasick<char, short>)
{
    const auto values =
        std::vector<std::pair<std::string, short>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = searcher(values);

    CHECK(*a.find(std::string("qwerty")) == 1);
    CHECK(*a.find(std::string("asdfgh")) == 2);
    CHECK(*a.find(std::string("qwe")) == 3);
    CHECK(*a.find(std::string("rty")) == 4);
}

TEST_CASE_TEMPLATE("Если при создании поисковика есть повторяющиеся строки, то к этой строке привязывается "
    "значение первой из встреченных повторяющихся строк",
    searcher, textum::trie<char, int>, textum::aho_corasick<char, int>)
{
    const auto values =
        std::vector<std::pair<std::string, int>>
        {
            {"qwerty", 11},
            {"qwerty", 22},
            {"qwe", 33},
            {"qwe", 44}
        };
    const auto a = searcher(values);

    CHECK(*a.find(std::string("qwerty")) == 11);
    CHECK(*a.find(std::string("qwe")) == 33);
}

TEST_CASE_TEMPLATE("Поиск строк, которые не участвовали в конструировании, возвращает итератор на конец",
    searcher, textum::trie<char, short>, textum::aho_corasick<char, short>)
{
    const auto values =
        std::vector<std::pair<std::string, short>>
        {
            {"qwerty", 1},
            {"asdfgh", 2},
            {"qwe", 3},
            {"rty", 4}
        };
    const auto a = searcher(values);

    CHECK(a.find(std::string("qwert")) == a.end());
    CHECK(a.find(std::string("aadfgh")) == a.end());
    CHECK(a.find(std::string("we")) == a.end());
    CHECK(a.find(std::string("y")) == a.end());
}

TEST_CASE_TEMPLATE("Поиск не зависит от порядка элементов в диапазоне при конструировании",
    searcher, textum::trie<char, short>, textum::aho_corasick<char, short>)
{
    {
        const auto values =
            std::vector<std::pair<std::string, short>>
            {
                {"qwerty", 1},
                {"qwe", 3},
                {"rty", 4}
            };
        const auto a = searcher(values);

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
        const auto a = searcher(values);

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
        const auto a = searcher(values);

        CHECK(*a.find(std::string("qwerty")) == 1);
        CHECK(*a.find(std::string("qwe")) == 3);
        CHECK(*a.find(std::string("rty")) == 4);
    }
}

TEST_CASE_TEMPLATE("Поиск по редакционному расстоянию записывает в выходной итератор пары, состоящие из "
    "расстояния до совпадающей строки, а также значения, соответствующего этой строке",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
{
    const auto values =
        std::vector<std::pair<std::string, unsigned long>>
        {
            {"abcdef", 1},
            {"abcabc", 2},
            {"bcdefg", 3},
        };
    const auto a = searcher(values);

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

TEST_CASE_TEMPLATE("Поиск по редакционному расстоянию находит даже те строки, в которых нет ни одного "
    "совпадения с исходной строкой",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcd", 1},
            {"qwerty", 2},
        };
    const auto a = searcher(values);

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

TEST_CASE_TEMPLATE("Поиск по редакционному расстоянию может возвращать только те результаты, которые "
    "удалены от искомой строки не более чем на заданную величину",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcd", 1},
            {"qwerty", 2},
        };
    const auto a = searcher(values);

    std::vector<std::pair<long, std::string::difference_type>> matched_values;
    a.find(textum::levenshtein(1), std::string("bcd"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, std::string::difference_type>>
        {
            {1, 1}, // abcd
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE_TEMPLATE("Максимальное редакционное расстояние может быть задано числом с плавающей точкой",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"abcd", 1},
            {"aaad", 2},
        };
    const auto a = searcher(values);

    std::vector<std::pair<long, double>> matched_values;
    a.find(textum::levenshtein(2.5), std::string("bd"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, double>>
        {
            {1, 2.0}, // abcd
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE_TEMPLATE("Если цена замены нулевая, то, сколько бы ни стоили вставка и удаление, расстояние "
    "между строками одинаковой длины будет равно нулю",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"asdfg", 1},
            {"zxcvb", 2},
            {"qwerty", 3},
        };
    const auto a = searcher(values);

    std::vector<std::pair<long, double>> matched_values;
    auto search_parameters =
        textum::levenshtein
        (
            1,
            [] (auto) {return 100500;}, // Вставка и удаление очень дорогие.
            [] (auto, auto) {return 0;} // Замена ничего не стоит.
        );
    a.find(search_parameters, std::string("qwert"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, double>>
        {
            {1, 0}, // asdfg
            {2, 0}, // zxcvb
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE_TEMPLATE("Если замена слишком дорога, то минимальное расстояние достигается за счёт "
    "удалений и вставок",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            {"asdfg", 1},
        };
    const auto a = searcher(values);

    std::vector<std::pair<long, double>> matched_values;
    auto search_parameters =
        textum::levenshtein
        (
            100500,
            [] (auto) {return 10;}, // Вставка и удаление дешёвые.
            [] (auto x, auto y) {return x == y ? 0 : 100500;} // Замена очень дорогая.
        );
    a.find(search_parameters, std::string("123456"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, double>>
        {
            {1, 10 * 5 + 10 * 6}, // Пять удалений, шесть вставок.
        };
    CHECK(matched_values == expected_values);
}

TEST_CASE_TEMPLATE("Префиксный поиск находит все принимаемые автоматом последовательности, до которых можно "
    "дойти из вершины, в которую привёл префикс",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
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
    const auto a = searcher(values);

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

TEST_CASE_TEMPLATE("Префиксный поиск возвращает входной итератор, если в автомате нет последовательностей, "
    "начинающихся с искомого префикса",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
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
    const auto a = textum::trie<char, long>(values);

    std::vector<long> found_values(values.size());
    const auto result = a.find_prefix(std::string("qwer"), found_values.begin());

    CHECK(result == found_values.begin());
}

TEST_CASE_TEMPLATE("Нечёткий префиксный поиск находит все принимаемые автоматом последовательности, до "
    "которых можно дойти из вершины, в которую привёла последовательность, отличающаяся от "
    "префикса не более чем на заданное редакционное расстояние",
    searcher, textum::trie<char, long>, textum::aho_corasick<char, long>)
{
    const auto values =
        std::vector<std::pair<std::string, long>>
        {
            // Префикс совпадает со всей строкой
            {"abc",     11},
            {"abcdef",  21},
            {"abcabc",  22},
            {"abcdeh",  23},
            {"abcdefg", 24},

            // Префикс нечётко совпадает со строкой (вставки и удаления)
            {"ab",     31},
            {"abdef",  32},
            {"ac",     41},
            {"acfgh",  42},
            {"bc",     51},
            {"bczxcg", 52},

            // Префикс нечётко совпадает со строкой (замены)
            {"Xbc",    61},
            {"Xbcdef", 62},
            {"aXc",    71},
            {"aXcdef", 72},
            {"abX",    81},
            {"abXdef", 82},

            // Префикс не совпадает со строкой
            {"aXY",    91},
            {"aXYdef", 92},
            {"XbY",    101},
            {"XbYdef", 102},
            {"XYc",    111},
            {"XYcdef", 112},
            {"qwerty", 121},
        };
    const auto a = textum::trie<char, long>(values);

    std::vector<std::pair<long, int>> matched_values;
    a.find_prefix(textum::levenshtein(1), std::string("abc"), std::back_inserter(matched_values));
    std::sort(matched_values.begin(), matched_values.end());

    const auto expected_values =
        std::vector<std::pair<long, int>>
        {
            {11, 0},
            {21, 0}, {22, 0}, {23, 0}, {24, 0},
            {31, 1}, {32, 1},
            {41, 1}, {42, 1},
            {51, 1}, {52, 1},
            {61, 1}, {62, 1},
            {71, 1}, {72, 1},
            {81, 1}, {82, 1},
        };
    CHECK(matched_values == expected_values);
}
