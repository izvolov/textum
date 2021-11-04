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
    return to_lower(l)(std::move(s));
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
// constexpr auto utf32_to_utf8 = utf32_to_utf8_t{};

auto split (const std::string & s, const std::locale & locale)
{
    auto u32s = to_lower(locale)(utf8_to_utf32(s));
    std::replace(u32s.begin(), u32s.end(), U'«', U' ');
    std::replace(u32s.begin(), u32s.end(), U'»', U' ');

    std::wstringstream stream(u32s);
    std::vector<std::wstring> v;
    for (std::wstring u32w; stream >> u32w; /*пусто*/)
    {
        // std::cout << w << std::endl;
        // auto u32_word = to_lower(locale)(utf8_to_utf32(std::move(w)));
        // std::cout << U'«' << std::endl;
        // std::cout << "---" << std::endl;
        // auto y = utf8_to_utf32(u8"«");
        // for (auto x: y)
        // {
        //     std::cout << x << std::endl;
        // }
        // std::cout << "---" << std::endl;
        // for (auto x: u32_word)
        // {
        //     std::cout << x << std::endl;
        // }
        // std::replace_if(u32_word.begin(), u32_word.end(), [] (auto x) {return x == wchar_t{U'«'};}, wchar_t{U' '});
        // std::replace_if(u32_word.begin(), u32_word.end(), [] (auto x) {return x == wchar_t{U'»'};}, wchar_t{U' '});
        v.push_back(u32w);
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

auto
    get_best_matched_strings
    (
        const std::wstring & u32w,
        const textum::aho_corasick<wchar_t, std::int32_t> & a,
        const std::unordered_map<std::int32_t, std::wstring> & id_to_word,
        const std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> & word_id_to_string_ids
    )
{
    auto weighted_words = get_weighted_matched_words(u32w, a, id_to_word);

    std::unordered_map<std::int32_t, std::pair<double, bool>> matched_strings;
    for (const auto & [word_id, distance_weight, position_weight, exact_match]: weighted_words)
    {
        const auto & string_ids = word_id_to_string_ids.at(word_id);
        for (const auto & string_id: string_ids)
        {
            if (auto matched_string = matched_strings.find(string_id); matched_string != matched_strings.end())
            {
                auto new_weight = distance_weight * position_weight;
                if (matched_string->second.first < new_weight)
                {
                    matched_string->second = std::pair{new_weight, exact_match};
                }
            }
            else
            {
                matched_strings[string_id] = std::pair{distance_weight * position_weight, exact_match};
            }
        }
    }

    return matched_strings;
}

auto
    get_candidates
    (
        const std::vector<std::wstring> & u32words,
        const textum::aho_corasick<wchar_t, std::int32_t> & a,
        const std::unordered_map<std::int32_t, std::wstring> & id_to_word,
        const std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> & word_id_to_string_ids
    )
{
    std::unordered_map<std::int32_t, std::pair<double, int>> candidate_weights;
    for (const auto & u32w: u32words)
    {
        auto candidates = get_best_matched_strings(u32w, a, id_to_word, word_id_to_string_ids);

        for (const auto & [id, value]: candidates)
        {
            auto [weight, exact_match] = value;
            candidate_weights[id].first += weight;
            candidate_weights[id].second += (exact_match ? 1 : 0);
        }
    }

    return candidate_weights;
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

    std::cout << "Введите запрос:" << std::endl;
    for (std::string s; std::getline(std::cin, s); /*пусто*/)
    {
        const auto query_start_time = chr::steady_clock::now();

        auto u32words = split(s, locale);
        auto candidates = get_candidates(u32words, a, id_to_word, word_id_to_string_ids);

        auto flat_candidates =
            std::vector<std::pair<std::int32_t, std::pair<double, int>>>(candidates.begin(), candidates.end());
        std::sort(flat_candidates.begin(), flat_candidates.end(),
            [] (const auto & l, const auto & r)
            {
                return l.second > r.second;
            });
        flat_candidates.resize(std::min(flat_candidates.size(), max_candidates));

        const auto query_time =
            chr::duration_cast<chr::duration<double>>(chr::steady_clock::now() - query_start_time);

        for (const auto & [id, value]: flat_candidates)
        {
            auto [weight, exact_matches] = value;
            std::cout << "\t" << weight << '\t' << exact_matches << '\t' << id_to_string.at(id) << std::endl;
        }
        std::cout << "\t----\n\tДокументов найдено: " << candidates.size() << " шт" << std::endl;
        std::cout << "\t----\n\tВремя обработки: " << query_time.count() << " с" << std::endl;
    }
}
