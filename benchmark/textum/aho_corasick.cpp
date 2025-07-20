#include <textum/aho_corasick.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <iterator>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

template <typename T, typename V>
std::ostream & operator << (std::ostream & stream, const std::pair<T, V> & p)
{
    return stream << "{" << p.first << ", " << p.second << "}";
}

template <std::input_iterator I, std::sentinel_for<I> S>
std::ostream & print (std::ostream & stream, I begin, S end)
{
    if (begin != end)
    {
        stream << "[" << *begin;
        ++begin;
    }

    while (begin != end)
    {
        stream << ", " << *begin;
        ++begin;
    }

    return stream << "]";
}

template <std::ranges::input_range R>
std::ostream & print (std::ostream & stream, R && items)
{
    return print(stream, std::ranges::begin(items), std::ranges::end(items));
}

template <typename URNG>
std::string generate_random_word (URNG && generator, double mu, double sigma)
{
    // Утверждается, что длины слов в языке распределены логнормально с параметрами
    // 1.1 <= mu    <= 1.3
    // 0.6 <= sigma <= 0.8
    auto length_distribution = std::lognormal_distribution(mu, sigma);
    const auto length =
        static_cast<std::size_t>(std::max(1l, std::lround(length_distribution(generator))));
    auto word = std::string(length, 0);

    auto char_distribution = std::uniform_int_distribution<int>(0, 'z' - 'a');
    std::generate(word.begin(), word.end(),
        [& char_distribution, & generator] {return char_distribution(generator) + 'a';});

    return word;
}

template <typename URNG>
std::unordered_set<std::string>
    generate_random_words (URNG && generator, std::size_t count, double mu, double sigma)
{
    auto words = std::unordered_set<std::string>{};
    std::generate_n(std::inserter(words, words.end()), count,
        [& generator, mu, sigma]
        {
            return generate_random_word(generator, mu, sigma);
        });

    return words;
}

textum::aho_corasick<char, int> make_aho_corasick (std::unordered_set<std::string> words)
{
    auto values = std::vector<std::pair<std::string, int>>{};
    for (auto & word: words)
    {
        values.emplace_back(std::move(word), values.size() + 1);
    }
    print(std::clog, values) << std::endl;

    return textum::aho_corasick<char, int>(values);
}

struct command_line_args_t
{
    std::size_t words_count;
    double lognormal_distribution_mu;
    double lognormal_distribution_sigma;
    std::size_t attempts;
    std::size_t max_levenshtein_distance;
};

command_line_args_t parse_command_line (int argc, const char * argv[])
{
    try
    {
        if (argc == 1 + 5)
        {
            return
                command_line_args_t
                {
                    std::stoul(argv[1]),
                    std::stod(argv[2]),
                    std::stod(argv[3]),
                    std::stoul(argv[4]),
                    std::stoul(argv[5])
                };
        }
        else
        {
            throw std::runtime_error("Wrong command line arguments");
        }
    }
    catch (const std::exception & e)
    {
        std::cout << e.what() << std::endl;
        std::cout << "Usage: " << argv[0] << " count mu sigma attempts distance" << std::endl;

        exit(1);
    }
}

int main (int argc, const char * argv[])
{
    const auto [count, mu, sigma, attempts, distance] = parse_command_line(argc, argv);

    std::default_random_engine generator;

    auto words = generate_random_words(generator, count, mu, sigma);
    print(std::clog, words) << std::endl;

    const auto trie = make_aho_corasick(words);

    auto found = std::size_t{0};

    using namespace std::chrono;
    const auto start_time = high_resolution_clock::now();
    for (auto i = 0ul; i < attempts; ++i) {
        for (const auto & word: words)
        {
            auto results = std::vector<std::pair<int, int>>{};
            trie.find(textum::levenshtein(distance), word, std::back_inserter(results));
            if (!results.empty())
            {
                found += results.size();
            }
        }
    }
    const auto finish_time = high_resolution_clock::now();

    std::cout << "Total search time: " << duration_cast<duration<double>>(finish_time - start_time) << std::endl;
    std::cout << "Found: " << found << std::endl;
}
