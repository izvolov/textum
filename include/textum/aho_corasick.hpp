#pragma once

#include <textum/fsm.hpp>
#include <textum/levenshtein_parameters.hpp>
#include <textum/type_traits/iterator_difference.hpp>
#include <textum/type_traits/iterator_value.hpp>
#include <textum/type_traits/range_value.hpp>
#include <textum/type_traits/remove_cvref.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>
#include <queue>
#include <stack>
#include <tuple>
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
            Содержит индикатор принимаемости, суффиксную ссылку и принимаемую суффиксную ссылку.
    */
    template <typename Integral>
    struct aho_corasick_state_attribute_t
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
        /*!
            Это булево значение, которое сигнализирует о том, что данное состояние в автомате
            является принимаемым, то есть существует последовательность, которая приводит автомат
            из его корня в данное состояние.
        */
        bool is_accept = false;
    };

    /*!
        \brief
            Автомат Ахо-Корасик и соответствующие поисковые механизмы

        \details
            Предоставляет следующие возможности:

            1.  Поиск последовательности в автомате (эквивалентно поиску в ассоциативном массиве);
            2.  Нечёткий поиск по расстоянию Левенштейна;
            3.  Префиксный поиск (поиск всех последовательностей, начинающихся с заданного
                префикса).
    */
    template <typename Symbol, typename Mapped>
    class aho_corasick
    {
    public:
        //! Тип символов, из которых состоят хранимые последовательности.
        using symbol_type = Symbol;
        //! Тип меток, соответствующих хранимым последовательностям символов.
        using mapped_type = Mapped;

    private:
        using state_index_type = std::uint32_t;
        using state_attribute_type = aho_corasick_state_attribute_t<state_index_type>;
        using value_container_type = std::vector<mapped_type>;
        using value_container_difference_type = typename value_container_type::difference_type;
        using prefix_value_container_type = std::vector<value_container_difference_type>;

    public:
        /*!
            \brief
                Создание пустого автомата Ахо-Корасик

            \details
                Создаёт пустой автомат, в котором ничего нельзя найти.
        */
        aho_corasick ():
            m_aho_corasick_automaton{},
            m_attributes{{m_aho_corasick_automaton.root(), state_attribute_type{}}},
            m_reachable_accept_values{{m_aho_corasick_automaton.root(), prefix_value_container_type{}}},
            m_values{},
            m_value_indices{}
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
        aho_corasick (InputRange && marked_sequences):
            aho_corasick()
        {
            initialize(std::forward<InputRange>(marked_sequences));
        }

        /*!
            \brief
                Количество помеченных (принимаемых автоматом) последовательностей
        */
        auto size () const
        {
            return m_values.size();
        }

        /*!
            \brief
                Индикатор отсутствия в автомате хотя бы одной помеченной последовательности
        */
        bool empty () const
        {
            return m_values.empty();
        }

        /*!
            \brief
                Начало набора меток (ассоциированных значений)

            \details
                Каждая последовательность символов ("строка") в автомате "помечена". Иначе говоря,
                с ней ассоциировано некое значение. Существует возможность пройтись по всем этим
                меткам.

                Также итераторы возвращаются в качестве результата некоторых видов поиска.

            \warning
                Порядок меток никак не регламентируется.
        */
        auto begin () const
        {
            return m_values.begin();
        }

        /*!
            \brief
                Конец набора меток (ассоциированных значений)

            \see begin
        */
        auto end () const
        {
            return m_values.end();
        }

        auto cbegin () const
        {
            return begin();
        }

        auto cend () const
        {
            return end();
        }

        /*!
            \brief
                Найти последовательность в автомате

            \details
                Асимптотика:
                -   Время: `O(|[first, last)|)`;
                -   Память: `O(1)`.

            \param [first, last)
                Полуинтервал, задающий искомую последовательность символов. Каждый символ обязан
                уметь неявно приводиться к типу `symbol_type`.

            \returns
                Итератор на метку последовательности, если автомат принимает данную
                последовательность, и `end()` в противном случае.
        */
        template <typename InputIterator, typename Sentinel, typename =
            std::enable_if_t<std::is_convertible_v<iterator_value_t<InputIterator>, symbol_type>>>
        auto find (InputIterator first, Sentinel last) const
        {
            auto [state, position] = traverse(m_aho_corasick_automaton.root(), first, last);
            if (position == last)
            {
                const auto value_index = m_value_indices.find(state);
                if (value_index != m_value_indices.end())
                {
                    return begin() + value_index->second;
                }
            }

            return end();
        }

        /*!
            \brief
                Найти последовательность в автомате

            \returns
                `find(begin(sequence), end(sequence))`

            \see find
        */
        template <typename InputRange, typename =
            std::enable_if_t<std::is_convertible_v<range_value_t<InputRange>, symbol_type>>>
        auto find (const InputRange & sequence) const
        {
            using std::begin;
            using std::end;
            return find(begin(sequence), end(sequence));
        }

        /*!
            \brief
                Нечёткий поиск по расстоянию Левенштейна с пользовательскими параметрами

            \details
                Записывает в выходной итератор пары `(метка, расстояние)`, где `метка` — это некое
                значение, ассоциированное с искомой последовательностью (заданное при
                инициализации), а `расстояние` — собственно редакционное расстояние между искомой
                последовательностью и той последовательностью, с которой ассоциирована полученная
                `метка`.

                Асимптотика:
                -   Время: `O(|[first, last)| * |S|)`;
                -   Память: `O(|[first, last)| * |S|)`, где `S` — множество состояний автомата.
                -   Худший случай по времени и памяти достигается тогда, когда пришлось пройти по
                    всем состояниям автомата. Например, это происходит, если нет ограничения на
                    максимальное расстояние между сравниваемыми последовательностями.

            \param p
                Параметры нечёткого поиска.
            \param [first, last)
                Полуинтервал, задающий искомую последовательность символов. Символы должны быть
                приводимыми к типу `symbol_type`.
            \param result
                Итератор, в который будут записаны полученные результаты.

            \returns
                Итератор за последним записанным результатом.

            \see levenshtein_parameters_t
        */
        template
        <
            typename Arithmetic,
            typename UnaryFunction,
            typename BinaryFunction,
            typename RandomAccessIterator,
            typename Sentinel,
            typename OutputIterator,
            typename =
                std::enable_if_t
                <
                    std::is_convertible_v<iterator_value_t<RandomAccessIterator>, symbol_type>
                >>
        auto
            find
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                RandomAccessIterator first,
                Sentinel last,
                OutputIterator result
            ) const
            -> OutputIterator
        {
            using distance_type = Arithmetic;
            using std::distance;
            auto initial_row =
                std::vector<distance_type>(static_cast<std::size_t>(distance(first, last) + 1), 0);
            fill_initial_levenshtein_row(p, initial_row, first);

            auto stack = std::stack<std::pair<state_index_type, std::vector<distance_type>>>{};
            stack.emplace(m_aho_corasick_automaton.root(), std::move(initial_row));

            while (not stack.empty())
            {
                auto [source, source_row] = std::move(stack.top());
                stack.pop();

                // Может быть, стоит проверять расстояние не после вынимания из стека,
                // а перед укладкой в стек?
                if (m_attributes.at(source).is_accept && source_row.back() <= p.distance_limit)
                {
                    *result = std::pair(begin()[m_value_indices.at(source)], source_row.back());
                    ++result;
                }

                const auto not_too_far_away = [& p] (auto x) {return x <= p.distance_limit;};
                if (std::any_of(source_row.begin(), source_row.end(), not_too_far_away))
                {
                    m_aho_corasick_automaton.visit_transitions(source,
                        [& p, & stack, & source_row, & first, this]
                            (auto /*source*/, const auto & symbol, auto destination)
                            {
                                auto destination_row = source_row;
                                fill_levenshtein_row(p, source_row, destination_row, symbol, first);
                                stack.emplace(destination, std::move(destination_row));
                            });
                }
            }

            return result;
        }

        /*!
            \brief
                Нечёткий поиск по расстоянию Левенштейна с пользовательскими параметрами

            \returns
                `find_levenshtein(p, begin(sequence), end(sequence), result)`

            \see find_levenshtein
            \see levenshtein_parameters_t
        */
        template
        <
            typename Arithmetic,
            typename UnaryFunction,
            typename BinaryFunction,
            typename InputRange,
            typename OutputIterator
        >
        auto
            find
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                const InputRange & sequence,
                OutputIterator result
            ) const
        {
            using std::begin;
            using std::end;
            return find(std::move(p), begin(sequence), end(sequence), result);
        }

        /*!
            \brief
                Нечёткий поиск по расстоянию Левенштейна с параметрами по умолчанию

            \details
                В качестве типа, задающего редакционное расстояние, по умолчанию берётся
                `RandomAccessIterator::difference_type`.

            \returns
                `find_levenshtein(default_levenshtein<difference_type>, first, last, result)`

            \see find_levenshtein
            \see default_levenshtein
        */
        template
        <
            typename RandomAccessIterator,
            typename Sentinel,
            typename OutputIterator,
            typename =
                std::enable_if_t
                <
                    std::is_convertible_v<iterator_value_t<RandomAccessIterator>, symbol_type>
                >>
        auto
            find_levenshtein
            (
                RandomAccessIterator first,
                Sentinel last,
                OutputIterator result
            ) const
            -> OutputIterator
        {
            using difference_type = iterator_difference_t<RandomAccessIterator>;
            return find(default_levenshtein<difference_type>, first, last, result);
        }

        /*!
            \brief
                Нечёткий поиск по расстоянию Левенштейна с параметрами по умолчанию

            \returns
                `find_levenshtein(begin(sequence), end(sequence), result)`

            \see find_levenshtein
            \see default_levenshtein
        */
        template <typename InputRange, typename OutputIterator>
        auto find_levenshtein (const InputRange & sequence, OutputIterator result) const
        {
            using std::begin;
            using std::end;
            return find_levenshtein(begin(sequence), end(sequence), result);
        }

        /*!
            \brief
                Префиксный поиск последовательности

            \details
                Находит все последовательности, начинающиеся с заданного префикса и записывает в
                выходной итератор метки найденных последовательностей (заданные при инициализации).

                Асимптотика:
                -   Время: `O(|[first, last)| + |s|)`;
                -   Память (для записи в выходной итератор): `O(|s|)`, где `s` — длиннейшая
                    из принимаемых автоматом последовательностей.
                -   Худший случай по времени и памяти достигается тогда, когда все префиксы самой
                    длинной принимаемой последовательности также принимаются автоматом, а искомый
                    префикс является одним из префиксов этой длиннейшей принимаемой
                    последовательности.

            \param [first, last)
                Искомый префикс. Каждый его символ должен быть приводимым к типу `symbol_type`.
            \param result
                Итератор, в который будут записаны найденные метки.

            \returns
                Итератор за последним записанным результатом.
        */
        template <typename InputIterator, typename Sentinel, typename OutputIterator, typename =
            std::enable_if_t<std::is_convertible_v<iterator_value_t<InputIterator>, symbol_type>>>
        auto find_prefix (InputIterator first, Sentinel last, OutputIterator result) const
        {
            const auto [state, position] = traverse(m_aho_corasick_automaton.root(), first, last);
            if (position == last)
            {
                return collect_reachable(state, result);
            }

            return result;
        }

        /*!
            \brief
                Префиксный поиск последовательности

            \returns
                `find_prefix(begin(sequence), end(sequence), result)`

            \see find_prefix
        */
        template <typename InputRange, typename OutputIterator>
        auto find_prefix (const InputRange & sequence, OutputIterator result) const
        {
            using std::begin;
            using std::end;
            return find_prefix(begin(sequence), end(sequence), result);
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
            auto state = m_aho_corasick_automaton.root();
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
                Основная функция инициализации автомата

            \details
                Принимает набор помеченных последовательностей, строит бор, прописывает
                зависимости между узлами бора и метками соответствующих последовательностей,
                прописывает достижимые вершины для вершин бора.
                В общем, формирует корректный автомат, с которым в дальнейшем будет происходить
                вся работа.
        */
        template <typename InputRange>
        void initialize (InputRange && marked_sequences)
        {
            build_trie(std::forward<InputRange>(marked_sequences));
            build_suffix_links();
        }

        /*!
            \brief
                Построение бора

            \details
                По набору помеченных последовательностей строит классический бор и помечает
                выделенные вершины бора метками соответствующих им последовательностей.
        */
        template <typename InputRange>
        void build_trie (InputRange && marked_sequences)
        {
            for (auto && [sequence, mark]: std::forward<InputRange>(marked_sequences))
            {
                auto state = insert(std::forward<decltype(sequence)>(sequence));
                const auto [value_index, success] =
                    attach_value(state, std::forward<decltype(mark)>(mark));
                if (success)
                {
                    visit_sources_of_path(sequence,
                        [value_index, this] (auto state) {attach_reachable_value(state, value_index);});
                    attach_reachable_value(state, value_index);
                }
            }
            assert(m_attributes.size() == m_aho_corasick_automaton.size());
        }

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

            auto & root_attributes = m_attributes.at(m_aho_corasick_automaton.root());
            root_attributes.suffix_link = m_aho_corasick_automaton.root();

            m_aho_corasick_automaton.visit_transitions(m_aho_corasick_automaton.root(),
                [this, & states] (auto /* root */, const auto & /*symbol*/, auto destination)
                {
                    auto attributes = m_attributes.find(destination);
                    assert(attributes != m_attributes.end());
                    attributes->second.suffix_link = m_aho_corasick_automaton.root();
                    states.push(destination);
                });

            while (not states.empty())
            {
                auto state = states.front();
                states.pop();

                m_aho_corasick_automaton.visit_transitions(state,
                    [this, & states] (auto source, const auto & symbol, auto destination)
                    {
                        auto [sl, success] = next(m_attributes.at(source).suffix_link, symbol);
                        assert(success);
                        static_cast<void>(success);

                        auto & destination_attributes = m_attributes.at(destination);
                        const auto & suffix_link_attributes = m_attributes.at(sl);

                        destination_attributes.suffix_link = sl;
                        destination_attributes.accept_suffix_link =
                            suffix_link_attributes.is_accept
                                ? sl
                                : suffix_link_attributes.accept_suffix_link;

                        states.push(destination);
                    });
            }
        }

        /*!
            \brief
                Посетить каждый узел автомата на пути по последовательности

            \details
                Проходит по последовательности начиная от корня автомата, и для каждого узла на
                этом пути вызызает посетителя. Непосещённым остаётся только последний узел, т.е.
                тот, который соответствует всей последовательности.

            \param sequence
                Последовательность, узлы которой требуется посетить.
            \param visit_state
                Посетитель узлов. Принимает описатель узла (элемент типа `state_index_type`) и
                производит над ним необходимую работу.
        */
        template <typename InputRange, typename UnaryFunction>
        void visit_sources_of_path (const InputRange & sequence, UnaryFunction && visit_state)
        {
            auto state = m_aho_corasick_automaton.root();
            for (const auto & symbol: sequence)
            {
                visit_state(state);
                auto [next_state, success] = m_aho_corasick_automaton.next(state, symbol);
                assert(success);
                static_cast<void>(success);
                state = next_state;
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
            const auto & initial_state_attributes = m_attributes.at(state);
            if (initial_state_attributes.is_accept)
            {
                *result = begin()[m_value_indices.at(state)];
                ++result;
            }
            state = initial_state_attributes.accept_suffix_link;

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
                Собрать метки, ассоциированные с принимаемыми состояниями автомата, достижимыми из
                заданного состояния

            \details
                Допустим, некая последовательность привела автомат из начального состояния в
                состояние `state`. Теперь мы хотим собрать метки всех последовательностей, префиксом
                которых является эта последовательность.

                Записывает в выходной итератор метки последовательностей, соответствующих
                принимаемым состояниям автомата, достижимым из состояния `state`.

            \param state
                Состояние автомата, в который привела некая последовательность.
            \param result
                Итератор, в который будут записаны найденные метки.

            \returns
                Итератор за последним записанным результатом.
        */
        template <typename OutputIterator>
        OutputIterator collect_reachable (state_index_type state, OutputIterator result) const
        {
            const auto & reachable_values = m_reachable_accept_values.at(state);
            return
                std::transform(reachable_values.begin(), reachable_values.end(), result,
                    [this] (auto value_index)
                    {
                        return begin()[value_index];
                    });
        }

        /*!
            \brief
                Заполнение начальной строки таблицы по алгоритму Вагнера-Фишера

            \param p
                Параметры нечёткого поиска.
            \param initial_row
                Диапазон, в который будет записана начальная строка таблицы.
            \param pattern
                Итератор на начало входной последовательности.

            \see levenshtein_parameters_t
            \see find_levenshtein
        */
        template
        <
            typename Arithmetic,
            typename UnaryFunction,
            typename BinaryFunction,
            typename RandomAccessRange,
            typename RandomAccessIterator
        >
        auto
            fill_initial_levenshtein_row
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                RandomAccessRange & initial_row,
                RandomAccessIterator pattern
            ) const
        {
            initial_row[0] = Arithmetic{0};
            for (auto i = 1ul; i < initial_row.size(); ++i)
            {
                using difference_type = iterator_difference_t<RandomAccessIterator>;
                const auto & value = pattern[static_cast<difference_type>(i) - 1];
                initial_row[i] = initial_row[i - 1] + p.deletion_or_insertion_penalty(value);
            }
        }

        /*!
            \brief
                Заполнение начальной строки таблицы по алгоритму Вагнера-Фишера со стандартными
                параметрами

            \details
                При стандартных единичных весах удаления и вставки это всё эквивалентно банальной
                йоте.

            \see fill_initial_levenshtein_row
            \see levenshtein_parameters_t
            \see find_levenshtein
        */
        template <typename Arithmetic, typename RandomAccessRange, typename RandomAccessIterator>
        auto
            fill_initial_levenshtein_row
            (
                levenshtein_parameters_t<Arithmetic> /*p*/,
                RandomAccessRange & initial_row,
                RandomAccessIterator /*pattern*/
            ) const
        {
            std::iota(initial_row.begin(), initial_row.end(), 0);
        }

        /*!
            \brief
                Заполнение одной строки таблицы по алгоритму Вагнера-Фишера

            \details
                В процессе поиска редакционного расстояния Левенштейна с помощью алгоритма
                Вагнера-Фишера заполняется таблица соответствий (см. описание алгоритма).
                Данная функция производит заполнение одной строки этой таблицы.

                Важно отметить, что в данном случае столбцам этой таблицы соответствуют символы
                некоей исследуемой входной последовательности, а строкам — символы, стоящие на
                переходах между состояниями автомата.

            \param p
                Параметры нечёткого поиска.
            \param source_row
                Последняя строка таблицы, вычисленной для последовательности, продолжением которой
                является `symbol`.
            \param destination_row
                Диапазон, в который будет записана строка таблицы, соответствующая символу `symbol`.
            \param symbol
                Символ, для которого требуется вычислить строку таблицы.
            \param pattern
                Итератор на начало входной последовательности.

            \see levenshtein_parameters_t
            \see find_levenshtein
        */
        template
        <
            typename LevenshteinParameters,
            typename RandomAccessRange1,
            typename RandomAccessRange2,
            typename RandomAccessIterator
        >
        auto
            fill_levenshtein_row
            (
                LevenshteinParameters p,
                const RandomAccessRange1 & source_row,
                RandomAccessRange2 & destination_row,
                const Symbol & symbol,
                RandomAccessIterator pattern
            ) const
        {
            destination_row[0] = source_row[0] + p.deletion_or_insertion_penalty(symbol);
            for (auto i = 1ul; i < destination_row.size(); ++i)
            {
                using difference_type = iterator_difference_t<RandomAccessIterator>;
                auto value = pattern[static_cast<difference_type>(i) - 1];
                destination_row[i] =
                    std::min
                    ({
                        destination_row[i - 1] + p.deletion_or_insertion_penalty(value),
                        source_row[i] + p.deletion_or_insertion_penalty(symbol),
                        source_row[i - 1] + p.replacement_penalty(value, symbol)
                    });
            }
        }

        /*!
            \brief
                Пройти по автомату от заданного состояния

            \details
                Переводит автомат из состояния в состояние, последовательно действуя на него
                символами входной последовательности до тех пор, пока такие переходы существуют.

            \param state
                Состояние, с которого начинается "путешествие".
            \param [first, last)
                Входная последовательность, символы которой будут последовательно подаваться в
                автомат.

            \returns
                Пару `(s, p)`, где `s` — состояние, до которого удалось дойти, а `p` — итератор на
                первый из элементов входной последовательности, по которому в автомате нет перехода
                из состояния `s`. Если удалось пройти по всем символам, то `p == last`.
        */
        template <typename ForwardIterator, typename Sentinel, typename =
            std::enable_if_t<std::is_convertible_v<iterator_value_t<ForwardIterator>, symbol_type>>>
        auto traverse (state_index_type state, ForwardIterator first, Sentinel last) const
            -> std::pair<state_index_type, ForwardIterator>
        {
            while (first != last)
            {
                auto [next_state, success] = m_aho_corasick_automaton.next(state, *first);
                if (not success)
                {
                    break;
                }

                state = next_state;
                ++first;
            }

            return std::pair(state, first);
        }

        /*!
            \brief
                Пройти по автомату от заданного состояния

            \returns
                `traverse(source, begin(sequence), end(sequence))`
        */
        template <typename ForwardRange, typename =
            std::enable_if_t<not std::is_convertible_v<ForwardRange, symbol_type>>>
        auto traverse (state_index_type source, const ForwardRange & sequence) const
        {
            using std::begin;
            using std::end;
            return traverse(source, begin(sequence), end(sequence));
        }

        /*!
            \brief
                "Нарастить" автомат от заданного состояния

            \details
                Строит переход из состояния `state` по символу `first[0]`, затем из нового
                полученного состояния строит переход по символу `first[1]` и т.д. до тех пор,
                пока входная последовательность не будет исчерпана.

            \param state
                Состояние, на которое происходит "наращивание".
            \param [first, last)
                Входная последовательность, символы которой будут последовательно подаваться в
                автомат.

            \pre
                Отсутствует переход из состояния `state` по символу `first[0]`.
        */
        template <typename InputIterator, typename Sentinel, typename =
            std::enable_if_t<std::is_convertible_v<iterator_value_t<InputIterator>, symbol_type>>>
        auto grow (state_index_type state, InputIterator first, Sentinel last)
            -> state_index_type
        {
            while (first != last)
            {
                auto symbol = *first;
                assert(m_aho_corasick_automaton.next(state, symbol) == std::pair(state, false));

                auto [next_state, success] =
                    m_aho_corasick_automaton.add_transition(state, std::move(symbol));
                assert(success);
                static_cast<void>(success);
                state = std::move(next_state);
                m_attributes[state] = state_attribute_type{};

                ++first;
            }

            return state;
        }

        /*!
            \brief
                Добавить в автомат переходы от заданного состояния

            \details
                Проходит по автомату из состояния `state` по символу `first[0]`, из полученного
                состояния — по символу `first[1]` и т.д. Если какого-либо из переходов не
                существовало ранее, достраивает этот переход.

            \param source
                Состояние, относительно которого происходит вставка.
            \param [first, last)
                Входная последовательность, символы которой будут последовательно подаваться в
                автомат.

            \returns
                Состояние, соответствующее всей входной последовательности.

            \see traverse
            \see grow
        */
        template <typename ForwardIterator, typename Sentinel, typename =
            std::enable_if_t<std::is_convertible_v<iterator_value_t<ForwardIterator>, symbol_type>>>
        auto insert (state_index_type source, ForwardIterator first, Sentinel last)
            -> state_index_type
        {
            auto [state, position] = traverse(source, first, last);
            return grow(state, position, last);
        }

        /*!
            \brief
                Добавить в автомат переходы от корня

            \details
                В качестве начального состояния берётся корень автомата.

            \returns
                `insert(root(), first, last)`

            \see insert
        */
        template <typename ForwardIterator, typename Sentinel, typename =
            std::enable_if_t<std::is_convertible_v<iterator_value_t<ForwardIterator>, symbol_type>>>
        auto insert (ForwardIterator first, Sentinel last)
        {
            return insert(m_aho_corasick_automaton.root(), first, last);
        }

        /*!
            \brief
                Добавить в автомат переходы от заданного состояния

            \returns
                `insert(source, begin(sequence), end(sequence))`

            \see insert
        */
        template <typename ForwardRange, typename =
            std::enable_if_t<std::is_convertible_v<range_value_t<ForwardRange>, symbol_type>>>
        auto insert (state_index_type source, ForwardRange && sequence)
        {
            using std::begin;
            using std::end;

            return
                insert
                (
                    source,
                    begin(std::forward<ForwardRange>(sequence)),
                    end(std::forward<ForwardRange>(sequence))
                );
        }


        /*!
            \brief
                Добавить в автомат переходы от корня

            \details
                В качестве начального состояния берётся корень автомата.

            \returns
                `insert(begin(sequence), end(sequence))`

            \see insert
        */
        template <typename ForwardRange, typename =
            std::enable_if_t<std::is_convertible_v<range_value_t<ForwardRange>, symbol_type>>>
        auto insert (ForwardRange && sequence)
        {
            using std::begin;
            using std::end;

            return
                insert
                (
                    begin(std::forward<ForwardRange>(sequence)),
                    end(std::forward<ForwardRange>(sequence))
                );
        }

        /*!
            \brief
                Пометить состояние автомата

            \details
                Помечает состояние автомата `state`, т.е. ассоциирует с ним некое значение
                `mapped_value`.

            \param state
                Состояние автомата, которое требуется пометить.
            \param mapped_value
                Метка, или ассоциированное значение для состояния.

            \returns
                Пару `(i, b)`, где `i` — индекс значения, соответствующего состоянию (причём в
                результате выполнения функции становится истинным выражение
                `begin()[i] == mapped_value`), а `b` — булево значение, истинное, если вставилось
                новое значение, и ложное, если состояние уже было помечено ранее.
        */
        template <typename M, typename =
            std::enable_if_t<std::is_convertible_v<remove_cvref_t<M>, mapped_type>>>
        auto attach_value (state_index_type state, M && mapped_value)
            -> std::pair<value_container_difference_type, bool>
        {
            auto new_value_index = static_cast<value_container_difference_type>(m_values.size());
            auto [position, success] = m_value_indices.emplace(state, new_value_index);
            if (success)
            {
                m_values.emplace_back(std::forward<M>(mapped_value));
                m_attributes.at(state).is_accept = true;
            }
            return std::pair(position->second, success);
        }

        /*!
            \brief
                Закешировать индекс значения, достижимого из заданного состояния

            \details
                Если из некоторого состояния автомата есть какие-либо переходы, то может
                существовать состояние, достижимое из данного состояния (в графовом смысле),
                которое является одним из принимаемых автоматом состояний.
                Данные достижимые принимаемые состояния предпосчитываются на этапе построения
                автомата с тем, чтобы в дальнейшем получить более эффективный поиск.

            \param state
                Состояние, для которого требуется закешировать индекс значения.
            \param value_index
                Индекс в массиве меток, по которому лежит значение, достижимое из состояния `state`,
                и которое требуется закешировать.
        */
        void
            attach_reachable_value
            (
                state_index_type state,
                value_container_difference_type value_index
            )
        {
            m_reachable_accept_values[state].push_back(value_index);
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
            while (not m_aho_corasick_automaton.is_root(source))
            {
                auto [candidate, success] =
                    m_aho_corasick_automaton.next(source, symbol);
                if (success)
                {
                    return std::pair(candidate, true);
                }
                else
                {
                    source = m_attributes.at(source).suffix_link;
                }
            }

            return std::pair(m_aho_corasick_automaton.next(source, symbol).first, true);
        }

        //! Базовый автомат, содержащий состояния и переходы между ними.
        fsm<symbol_type> m_aho_corasick_automaton;
        //! Дополнительные атрибуты состояний.
        std::unordered_map<state_index_type, state_attribute_type> m_attributes;
        //! Кэш достижимых принимаемых состояний.
        std::unordered_map<state_index_type, prefix_value_container_type> m_reachable_accept_values;
        //! Массив меток.
        value_container_type m_values;
        //! Индексы меток состояний в массиве меток.
        std::unordered_map<state_index_type, value_container_difference_type> m_value_indices;
    };
} // namespace textum
