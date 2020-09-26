#include <textum/aho_corasick.hpp>

#include <chrono>
#include <codecvt>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>

struct to_lower_t
{
    template <typename Char>
    auto operator () (std::basic_string<Char> s) const
    {
        for (auto & c: s)
        {
            c = std::tolower(c, locale);
        }
        return s;
    }

    template <typename Char>
    auto operator () (Char c) const
    {
        return std::tolower(c, locale);
    }

    std::reference_wrapper<const std::locale> locale;
};

inline auto to_lower (const std::locale & l)
{
    return to_lower_t{l};
}

template <typename Char>
auto to_lower (std::basic_string<Char> s, const std::locale & l)
{
    return to_lower(l)(s);
}

using utf32_converter_type = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>;

struct utf8_to_utf32_t
{
    std::wstring operator () (const std::string & s) const
    {
        return utf32_converter_type{}.from_bytes(s);
    }
};
constexpr auto utf8_to_utf32 = utf8_to_utf32_t{};

struct utf32_to_utf8_t
{
    std::string operator () (const std::wstring & s) const
    {
        return utf32_converter_type{}.to_bytes(s);
    }
};
constexpr auto utf32_to_utf8 = utf32_to_utf8_t{};

auto split (const std::string & s, const std::locale & locale)
{
    std::stringstream ss(s);
    std::vector<std::wstring> v;
    for (std::string w; ss >> w; /*пусто*/)
    {
        v.push_back(to_lower(locale)(utf8_to_utf32(std::move(w))));
    }

    return v;
}

auto index (std::istream & stream, const std::locale & locale)
{
    std::unordered_map<std::int32_t, std::string> id_to_string;
    std::unordered_map<std::string, std::int32_t> string_to_id;
    std::unordered_map<std::wstring, std::int32_t> marked;
    std::unordered_map<std::int32_t, std::wstring> id_to_word;
    std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> word_id_to_string_ids;

    for (std::string string; std::getline(stream, string); /*пусто*/)
    {
        auto [i, inserted] = string_to_id.emplace(string, string_to_id.size());
        if (inserted)
        {
            id_to_string.emplace(i->second, string);
            std::replace(string.begin(), string.end(), ';', ' ');
            std::replace(string.begin(), string.end(), '|', ' ');

            auto u32words = split(string, locale);
            for (const auto & u32w: u32words)
            {
                auto [m, inserted] = marked.emplace(u32w, marked.size());
                id_to_word[m->second] = m->first;
                word_id_to_string_ids[m->second].emplace(i->second);
            }
        }
    }

    return
        std::tuple
        (
            textum::aho_corasick<wchar_t, std::int32_t>(std::move(marked)),
            std::move(id_to_word),
            std::move(word_id_to_string_ids),
            std::move(id_to_string)
        );
}

auto match_words_raw (const std::wstring & u32w, const textum::aho_corasick<wchar_t, std::int32_t> & a)
{
    std::vector<std::pair<std::int32_t, double>> matched_word_ids;

    namespace chr = std::chrono;
    const auto ac_start_time = chr::steady_clock::now();
    a.find_prefix(textum::levenshtein(1.0), u32w, std::back_inserter(matched_word_ids));
    const auto ac_time =
        chr::duration_cast<chr::duration<double>>(chr::steady_clock::now() - ac_start_time);
    std::cout << "Автомат пройден за: " << ac_time.count() << " с" << std::endl;

    const auto equal_by_first =
        [] (const auto & l, const auto & r)
        {
            return std::get<0>(l) == std::get<0>(r);
        };

    std::sort(matched_word_ids.begin(), matched_word_ids.end());
    matched_word_ids.erase
    (
        std::unique(matched_word_ids.begin(), matched_word_ids.end(), equal_by_first),
        matched_word_ids.end()
    );

    return matched_word_ids;
}

auto
    weigh_matched_words
    (
        const std::wstring & u32w,
        const std::vector<std::pair<std::int32_t, double>> & matched_words,
        const std::unordered_map<std::int32_t, std::wstring> & id_to_word
    )
{
    std::vector<std::tuple<std::int32_t, double, double, bool>> weighted_words;
    weighted_words.reserve(matched_words.size());

    for (const auto & [word_id, distance]: matched_words)
    {
        const auto & word = id_to_word.at(word_id);

        const auto query_word_size = static_cast<double>(u32w.size());
        const auto distance_weight = (query_word_size - static_cast<double>(distance)) / query_word_size;

        const auto mismatch = std::mismatch(u32w.begin(), u32w.end(), word.begin(), word.end());
        const auto position = mismatch.first - u32w.begin();
        auto position_weight = static_cast<double>(position + 1) / (query_word_size + 1);

        const auto exact_match = u32w == word;
        weighted_words.emplace_back(word_id, distance_weight, position_weight, exact_match);
    }

    return weighted_words;
}

auto
    get_weighted_matched_words
    (
        const std::wstring & u32w,
        const textum::aho_corasick<wchar_t, std::int32_t> & a,
        const std::unordered_map<std::int32_t, std::wstring> & id_to_word
    )
{
    auto matched_words = match_words_raw(u32w, a);
    return weigh_matched_words(u32w, matched_words, id_to_word);
}

int main (int /*argc*/, const char * argv[])
{
    std::cout << std::setprecision(2);
    namespace chr = std::chrono;
    const auto locale = std::locale("ru_RU.UTF-8");

    const auto file_name = argv[1];
    std::ifstream file(file_name);

    const auto max_candidates = std::stoul(argv[2]);

    const auto index_start_time = chr::steady_clock::now();
    auto [a, id_to_word, word_id_to_string_ids, id_to_string] = index(file, locale);
    const auto index_time =
        chr::duration_cast<chr::duration<double>>(chr::steady_clock::now() - index_start_time);
    std::cout << "Индекс построен за: " << index_time.count() << " с" << std::endl;

    std::cout << "Введите слово:" << std::endl;
    for (std::string w; std::cin >> w; /*пусто*/)
    {
        const auto query_start_time = chr::steady_clock::now();

        const auto u32word = to_lower(locale)(utf8_to_utf32(std::move(w)));
        auto matched_words = get_weighted_matched_words(u32word, a, id_to_word);
        const auto match_count = matched_words.size();

        std::sort(matched_words.begin(), matched_words.end(),
            [] (auto l, auto r)
            {
                std::get<0>(l) = std::get<0>(r) = 0;
                return l > r;
            });
        matched_words.resize(std::min(matched_words.size(), max_candidates));

        const auto query_time =
            chr::duration_cast<chr::duration<double>>(chr::steady_clock::now() - query_start_time);

        for (const auto & [id, distance_weight, position_weight, exact_match]: matched_words)
        {
            std::cout << "\t" << distance_weight << '\t' << position_weight << '\t' << exact_match << '\t' << utf32_to_utf8(id_to_word.at(id)) << std::endl;
        }
        std::cout << "\t----\n\tСлов найдено: " << match_count << " шт" << std::endl;
        std::cout << "\t----\n\tВремя обработки: " << query_time.count() << " с" << std::endl;
    }
}
