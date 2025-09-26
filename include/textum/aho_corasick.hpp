#pragma once

#include <textum/trie.hpp>
#include <textum/type_traits/iterator_value.hpp>

#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace textum
{
    template <typename Integral>
    constexpr auto not_set = std::numeric_limits<Integral>::max();

    /*!
        \brief
            Атрибуты состояния автомата Ахо-Корасик

        \details
            Добавление экземпляра этой структуры к состоянию простого автомата позволяет
            организовать автомат Ахо-Корасик.
            Содержит суффиксную ссылку и принимаемую суффиксную ссылку.
    */
    template <typename Integral>
    struct aho_corasick_state_attribute_t: trie_state_attribute_t
    {
        /*!
            Суффиксная ссылка всегда есть. При этом она может соответствовать "пустому" суффиксу,
            и в этом случае это будет ссылка на корень автомата.
        */
        Integral suffix_link = not_set<Integral>;
        /*!
            Это ссылка на наибольший принимаемый суффикс из существующих в автомате. Может быть
            пустой, если такой суффикс отсутствует.
        */
        Integral accept_suffix_link = not_set<Integral>;
    };

    /*!
        \brief
            Автомат Ахо-Корасик и соответствующие поисковые механизмы

        \details
            Вдобавок к бору предоставляет возможность искать все вхождения слов из словаря во
            входную строку (см, `match`).
    */
    template <typename Symbol, typename Mapped>
    class aho_corasick:
        public basic_trie<Symbol, Mapped, std::uint32_t, aho_corasick_state_attribute_t<std::uint32_t>>
    {
    public:
        using trie_type =
            basic_trie<Symbol, Mapped, std::uint32_t, aho_corasick_state_attribute_t<std::uint32_t>>;
        //! Тип символов, из которых состоят хранимые последовательности.
        using symbol_type = typename trie_type::symbol_type;
        //! Тип меток, соответствующих хранимым последовательностям символов.
        using mapped_type = typename trie_type::mapped_type;

    private:
        using trie_type::m_trie_automaton;
        using trie_type::m_attributes;
        using trie_type::begin;
        using trie_type::m_value_indices;
        using trie_type::m_values;

        using state_index_type = std::uint32_t;
        using state_attribute_type = aho_corasick_state_attribute_t<std::uint32_t>;

    public:
        /*!
            \brief
                Создание пустого автомата Ахо-Корасик

            \details
                Создаёт пустой автомат, в котором ничего нельзя найти.
        */
        aho_corasick ():
            trie_type()
        {
        }

        /*!
            \brief
                Создание автомата Ахо-Корасик

            \details
                Асимптотика:
                -   Время: `O(суммы длин входных последовательностей)`;
                -   Память (занимаемая итоговым объектом): `O(суммы длин входных последовательностей)`.

            \param marked_sequences
                Набор помеченных последовательностей, т.е. пар `(последовательность, метка)`, где
                `последовательность` является однонаправленным (как минимум) диапазоном, состоящим
                из элементов, приводимых к типу `symbol_type`, а `метка` — это некое значение,
                ассоциированное с данной последовательностью, приводимое к типу `mapped_type`.
        */
        template <typename InputRange>
        explicit aho_corasick (InputRange && marked_sequences):
            trie_type(std::forward<InputRange>(marked_sequences))
        {
            build_suffix_links();
        }

        /*!
            \brief
                Поиск подпоследовательностей искомой последовательности в автомате

            \details
                Находит все подпоследовательности заданной последовательности, которые принимает
                автомат, и записывает метки этих подпоследовательностей в выходной итератор.

                Асимптотика:
                -   Время: `O(|A| + |[first, last)|)`;
                -   Память (для записи в выходной итератор): `O(|A|)`, где `A` — множество
                    подпоследовательностей искомой последовательности, принимаемых автоматом.
                -   Худший случай по времени и памяти достигается тогда, когда все
                    подпоследовательности искомой последовательности принимаются автоматом.
                    Например, если автомат принимает набор последовательностей `{a, aa, aaa, aaaa}`,
                    а в качестве запроса приходит последовательность `aaaa`.

            \param [first, last)
                Искомая последовательность.
            \param result
                Итератор, в который будут записаны найденные метки.

            \returns
                Итератор за последним записанным результатом.
        */
        template <typename InputIterator, typename Sentinel, typename OutputIterator, typename =
            std::enable_if_t<std::is_convertible_v<iterator_value_t<InputIterator>, symbol_type>>>
        auto match (InputIterator first, Sentinel last, OutputIterator result) const
            -> OutputIterator
        {
            auto state = this->m_trie_automaton.root();
            while (first != last)
            {
                auto [next_state, success] = next(state, *first);
                state = std::move(next_state);
                assert(success);
                static_cast<void>(success);

                result = collect_matching(state, result);
                ++first;
            }

            return result;
        }

        /*!
            \brief
                Поиск подпоследовательностей искомой последовательности в автомате

            \returns
                `match(begin(sequence), end(sequence), result)`

            \see match
        */
        template <typename InputRange, typename OutputIterator>
        OutputIterator match (const InputRange & sequence, OutputIterator result) const
        {
            using std::begin;
            using std::end;
            return match(begin(sequence), end(sequence), result);
        }

    private:
        /*!
            \brief
                Построение суффиксных ссылок для автомата Ахо-Корасик

            \details
                Реализует классический алгоритм построения суффиксных ссылок при помощи обхода
                автомата в ширину.

            \pre
                Выполнение этой функции корректно только тогда, когда уже построен бор.
        */
        void build_suffix_links ()
        {
            std::queue<state_index_type> states;

            auto & root_attributes = m_attributes.at(this->m_trie_automaton.root());
            root_attributes.suffix_link = this->m_trie_automaton.root();

            this->m_trie_automaton.visit_transitions(this->m_trie_automaton.root(),
                [this, & states] (auto /* root */, const auto & /*symbol*/, auto destination)
                {
                    auto attributes = m_attributes.find(destination);
                    assert(attributes != m_attributes.end());
                    attributes->second.suffix_link = this->m_trie_automaton.root();
                    states.push(destination);
                });

            while (not states.empty())
            {
                auto state = states.front();
                states.pop();

                this->m_trie_automaton.visit_transitions(state,
                    [this, & states] (auto source, const auto & symbol, auto destination)
                    {
                        auto [sl, success] = next(m_attributes.at(source).suffix_link, symbol);
                        assert(success);
                        static_cast<void>(success);

                        auto & destination_attributes = m_attributes.at(destination);
                        const auto & suffix_link_attributes = m_attributes.at(sl);
                        const auto & trie_attributes = m_attributes.at(sl);

                        destination_attributes.suffix_link = sl;
                        destination_attributes.accept_suffix_link =
                            trie_attributes.is_accept
                                ? sl
                                : suffix_link_attributes.accept_suffix_link;

                        states.push(destination);
                    });
            }
        }

        /*!
            \brief
                Собрать все суффиксы, принимаемые автоматом в данном состоянии

            \details
                Допустим, некая последовательность привела автомат из начального состояния в
                состояние `state`. Теперь мы хотим понять, какие суффиксы этой последовательности
                принимаются автоматом. Собственно, для чего и предназначен автомат Ахо-Корасик.

                Функция проходит по суффиксным ссылкам от заданного состояния `state` автомата до
                корня и записывает в выходной итератор метки всех принимаемых автоматом суффиксов,
                соответствующих данному состоянию `state`.

            \param state
                Состояние автомата, в который привела некая последовательность.
            \param result
                Итератор, в который будут записаны найденные метки.

            \returns
                Итератор за последним записанным результатом.
        */
        template <typename OutputIterator>
        OutputIterator collect_matching (state_index_type state, OutputIterator result) const
        {
            const auto & initial_state_trie_attributes = m_attributes.at(state);
            if (initial_state_trie_attributes.is_accept)
            {
                *result = begin()[m_value_indices.at(state)];
                ++result;
            }

            const auto & initial_state_ac_attributes = m_attributes.at(state);
            state = initial_state_ac_attributes.accept_suffix_link;

            while (state != not_set<state_index_type>)
            {
                assert(m_attributes.at(state).is_accept);
                *result = begin()[m_value_indices.at(state)];
                ++result;

                state = m_attributes.at(state).accept_suffix_link;
            }

            return result;
        }

        /*!
            \brief
                Переход к следующему состоянию в смысле автомата Ахо-Корасик

            \details
                Иначе говоря, если из состояния `source` есть переход по символу `sumbol`, то он
                производится. Если перехода нет, то происходит переход по суффиксной ссылке
                (допустим, в состояние `s`), из состояния `s` тоже происходит попытка перейти по
                символу `symbol` и т.д.

            \param source
                Исходное состояние, из которого ищется переход.
            \param symbol
                Символ, по которому происходит попытка перехода.

            \pre
                Важное предусловие: уже должны быть построены суффиксные ссылки для всех состояний,
                расстояние до которых от корня меньше расстояния до исходного состояния `source`.
                Если источник — корень, и суффиксные ссылки ещё не построены, вызов данной функции
                некорректен.

            \returns
                Состояние, которого удалось достигнуть из исходного состояния `source` или одной
                из его суффиксных ссылок, либо корень автомата, если переход так и не был найден.
        */
        template <typename S, typename =
            std::enable_if_t<std::is_convertible_v<S, symbol_type>>>
        auto next (state_index_type source, const S & symbol) const
            -> std::pair<state_index_type, bool>
        {
            while (not this->m_trie_automaton.is_root(source))
            {
                auto [candidate, success] =
                    this->m_trie_automaton.next(source, symbol);
                if (success)
                {
                    return std::pair(candidate, true);
                }
                else
                {
                    source = m_attributes.at(source).suffix_link;
                }
            }

            return std::pair(this->m_trie_automaton.next(source, symbol).first, true);
        }
    };
} // namespace textum
