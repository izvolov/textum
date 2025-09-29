#pragma once

#include <textum/fsm.hpp>
#include <textum/levenshtein_parameters.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>
#include <ranges>
#include <stack>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace textum
{
    /*!
        \brief
            Бор (префиксное дерево) и соответствующие поисковые механизмы

        \details
            Предоставляет следующие возможности:

            1.  Поиск последовательности (эквивалентно поиску в ассоциативном массиве);
            2.  Нечёткий поиск по расстоянию Левенштейна;
            3.  Префиксный поиск (поиск всех последовательностей, начинающихся с заданного
                префикса).
    */
    template <typename Symbol, typename Mapped, typename StateIndex, typename StateAttribute>
    class basic_trie
    {
    public:
        //! Тип символов, из которых состоят хранимые последовательности.
        using symbol_type = Symbol;
        //! Тип меток, соответствующих хранимым последовательностям символов.
        using mapped_type = Mapped;

    private:
        using state_index_type = StateIndex;
        using state_attribute_type = StateAttribute;
        using value_container_type = std::vector<mapped_type>;
        using value_container_difference_type = typename value_container_type::difference_type;
        using prefix_value_container_type = std::vector<value_container_difference_type>;

    public:
        /*!
            \brief
                Создание пустого бора

            \details
                Создаёт пустой бор, в котором ничего нельзя найти.
        */
        basic_trie ():
            m_trie_automaton{},
            m_attributes{{m_trie_automaton.root(), state_attribute_type{}}},
            m_reachable_accept_values{{m_trie_automaton.root(), prefix_value_container_type{}}},
            m_values{},
            m_value_indices{}
        {
        }

        /*!
            \brief
                Создание бора

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
        template <std::ranges::input_range R>
        explicit basic_trie (R && marked_sequences):
            basic_trie()
        {
            initialize(std::forward<R>(marked_sequences));
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
        template <std::input_iterator I, std::sentinel_for<I> S>
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto find (I first, S last) const
        {
            auto [state, position] = traverse(m_trie_automaton.root(), first, last);
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
        template <std::ranges::input_range R>
            requires(std::is_convertible_v<std::ranges::range_value_t<R>, symbol_type>)
        auto find (const R & sequence) const
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
            \see visit_close_states
        */
        template
        <
            typename Arithmetic,
            typename UnaryFunction,
            typename BinaryFunction,
            std::random_access_iterator I,
            std::sentinel_for<I> S,
            std::output_iterator<std::pair<mapped_type, Arithmetic>> O
        >
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto
            find
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                I first,
                S last,
                O result
            ) const
            -> O
        {
            visit_close_states(p, m_trie_automaton.root(), first, last,
                [& result, this] (auto state, auto distance)
                {
                    if (m_attributes.at(state).is_accept)
                    {
                        *result = std::pair(begin()[m_value_indices.at(state)], distance);
                        ++result;
                    }
                });

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
            std::ranges::input_range R,
            std::output_iterator<std::pair<mapped_type, Arithmetic>> O
        >
        auto
            find
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                const R & sequence,
                O result
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
                `std::iter_difference_t<I>`.

            \returns
                `find_levenshtein(default_levenshtein<difference_type>, first, last, result)`

            \see find_levenshtein
            \see default_levenshtein
        */
        template <std::random_access_iterator I, std::sentinel_for<I> S, typename OutputIterator>
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto find_levenshtein (I first, S last, OutputIterator result) const
        {
            using difference_type = std::iter_difference_t<I>;
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
        template <std::ranges::input_range R, typename OutputIterator>
        auto find_levenshtein (const R & sequence, OutputIterator result) const
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
        template
        <
            std::input_iterator I,
            std::sentinel_for<I> S,
            std::output_iterator<mapped_type> O
        >
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto find_prefix (I first, S last, O result) const
            -> O
        {
            const auto [state, position] = traverse(m_trie_automaton.root(), first, last);
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
        template <std::ranges::input_range R, typename OutputIterator>
        auto find_prefix (const R & sequence, OutputIterator result) const
        {
            using std::begin;
            using std::end;
            return find_prefix(begin(sequence), end(sequence), result);
        }

        /*!
            \brief
                Нечёткий префиксный поиск по расстоянию Левенштейна

            \details
                Находит в автомате все состояния, соответствующие последовательностям, префикс
                которых отличается по Левенштейну от искомой последовательности не более, чем на
                `p.distance_limit`, и записывает в выходной итератор пары `(метка, расстояние)`,
                где `метка` — значение (заданное при инициализации), ассоциированное с найденной
                последовательностью, а `расстояние` — это кратчайшее расстояние по Левенштейну между
                искомой последовательностью и префиксом найденной последовательности.

                Асимптотика:
                -   Время: `O(|[first, last)| * |S| ^ 2)`;
                -   Память: `O(|[first, last)| * |S| ^ 2)`, где `S` — множество состояний автомата.
                -   Худший случай по времени и памяти достигается тогда, когда:
                    1.  Пришлось пройти по всем состояниям автомата. Например, это происходит, если
                        нет ограничения на максимальное расстояние между сравниваемыми
                        последовательностями;
                    2.  Каждое состояние автомата — принимаемое.

            \param p
                Параметры нечёткого поиска.
            \param [first, last)
                Искомый префикс. Каждый его символ должен быть приводимым к типу `symbol_type`.
            \param result
                Итератор, в который будут записаны полученные результаты.

            \returns
                Итератор за последним записанным результатом.

            \see levenshtein_parameters_t
            \see visit_close_states
            \see collect_reachable
        */
        template
        <
            typename Arithmetic,
            typename UnaryFunction,
            typename BinaryFunction,
            std::random_access_iterator I,
            std::sentinel_for<I> S,
            std::output_iterator<std::pair<mapped_type, Arithmetic>> O
        >
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto
            find_prefix
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                I first,
                S last,
                O result
            ) const
            -> O
        {
            std::vector<std::pair<mapped_type, Arithmetic>> results;
            visit_close_states(p, m_trie_automaton.root(), first, last,
                [& results, this] (auto state, auto distance)
                {
                    this->collect_reachable(state, std::back_inserter(results), distance);
                });

            std::sort(results.begin(), results.end());
            const auto unique_end =
                std::unique(results.begin(), results.end(),
                    [] (const auto & l, const auto & r)
                    {
                        return l.first == r.first;
                    });
            return std::copy(results.begin(), unique_end, result);
        }

        template
        <
            typename Arithmetic,
            typename UnaryFunction,
            typename BinaryFunction,
            std::ranges::input_range R,
            typename OutputIterator
        >
        auto
            find_prefix
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                const R & sequence,
                OutputIterator result
            ) const
        {
            using std::begin;
            using std::end;
            return find_prefix(std::move(p), begin(sequence), end(sequence), result);
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
        template <std::ranges::input_range R>
        void initialize (R && marked_sequences)
        {
            build_trie(std::forward<R>(marked_sequences));
        }

        /*!
            \brief
                Построение бора

            \details
                По набору помеченных последовательностей строит классический бор и помечает
                выделенные вершины бора метками соответствующих им последовательностей.
        */
        template <std::ranges::input_range R>
        void build_trie (R && marked_sequences)
        {
            for (auto && [sequence, mark]: std::forward<R>(marked_sequences))
            {
                auto state = insert(std::forward<decltype(sequence)>(sequence));
                const auto [value_index, success] =
                    attach_value(state, std::forward<decltype(mark)>(mark));
                if (success)
                {
                    visit_sources_of_path(sequence,
                        [value_index = value_index, this] (auto state)
                        {
                            attach_reachable_value(state, value_index);
                        });
                    attach_reachable_value(state, value_index);
                }
            }
            assert(m_attributes.size() == m_trie_automaton.size());
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
        template <std::ranges::input_range R, typename UnaryFunction>
        void visit_sources_of_path (const R & sequence, UnaryFunction && visit_state)
        {
            auto state = m_trie_automaton.root();
            for (const auto & symbol: sequence)
            {
                visit_state(state);
                auto [next_state, success] = m_trie_automaton.next(state, symbol);
                assert(success);
                static_cast<void>(success);
                state = next_state;
            }
        }

        /*!
            \brief
                Посетить узлы автомата, соответствующие строкам, отличающимся от заданной не более,
                чем на расстояние, заданное в параметрах нечёткого поиска

            \details
                Посещает все подходящие состояния автомата, и для каждого состояния, с которым
                ассоциирована последовательность, которая отличается от искомой последовательности
                не более, чем на `p.distance_limit` по Левенштейну, вызывает посетителя `visit`.

                Асимптотика:
                -   Время: `O(|[first, last)| * |S|)`;
                -   Память: `O(|[first, last)| * |S|)`, где `S` — множество состояний автомата.
                -   Худший случай по времени и памяти достигается тогда, когда пришлось пройти по
                    всем состояниям автомата. Например, это происходит, если нет ограничения на
                    максимальное расстояние между сравниваемыми последовательностями.

            \param p
                Параметры нечёткого поиска.
            \param state
            \param [first, last)
                Полуинтервал, задающий искомую последовательность символов. Символы должны быть
                приводимыми к типу `symbol_type`.
            \param visit
            \parblock
                Посетитель состояний автомата. Вызывается следующим образом:

                \code{.cpp}
                visit(s, distance);
                \endcode

                Здесь `s` — это посещённое состояние, `distance` — расстояние по Левенштейну от
                последовательности, ассоциированной с состоянием `s` до искомой
                последовательности.
            \endparblock

            \returns
                Экземпляр посетителя, который посетил все нужные состояний.

            \see levenshtein_parameters_t
        */
        template
        <
            typename Arithmetic,
            typename UnaryFunction,
            typename BinaryFunction,
            std::random_access_iterator I,
            std::sentinel_for<I> S,
            typename BinaryFunction2
        >
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto
            visit_close_states
            (
                levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction> p,
                state_index_type state,
                I first,
                S last,
                BinaryFunction2 visit
            ) const
            -> BinaryFunction2
        {
            using distance_type = Arithmetic;
            using std::distance;
            auto initial_row =
                std::vector<distance_type>(static_cast<std::size_t>(distance(first, last) + 1), 0);
            fill_initial_levenshtein_row(p, initial_row, first);

            auto stack = std::stack<std::pair<state_index_type, std::vector<distance_type>>>{};
            stack.emplace(state, std::move(initial_row));

            while (not stack.empty())
            {
                auto [source, source_row] = std::move(stack.top());
                stack.pop();

                // Может быть, стоит проверять расстояние не после вынимания из стека,
                // а перед укладкой в стек?
                if (auto distance = source_row.back(); distance <= p.distance_limit)
                {
                    visit(source, distance);
                }

                const auto not_too_far_away = [& p] (auto x) {return x <= p.distance_limit;};
                if (std::any_of(source_row.begin(), source_row.end(), not_too_far_away))
                {
                    m_trie_automaton.visit_transitions(source,
                        [& p, & stack, & source_row = source_row, & first, this]
                            (auto /*source*/, const auto & symbol, auto destination)
                            {
                                auto destination_row = source_row;
                                this->fill_levenshtein_row(p, source_row, destination_row, symbol, first);
                                stack.emplace(destination, std::move(destination_row));
                            });
                }
            }

            return visit;
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
        template <std::output_iterator<mapped_type> O>
        auto collect_reachable (state_index_type state, O result) const
            -> O
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
                Собрать метки, ассоциированные с принимаемыми состояниями автомата, достижимыми из
                заданного состояния, а также расстояние до них по Левенштейну

            \details
                Делает всё то же самое, что перегрузка с двумя аргументами, но к каждому результату
                дополнительно приклеивает расстояние `distance`.

            \param state
                Состояние автомата, в который привела некая последовательность.
            \param result
                Итератор, в который будут записаны найденные метки.
            \param distance
                Расстояние по Левенштейну до найденных меток.

            \returns
                Итератор за последним записанным результатом.
        */
        template <typename Arithmetic, std::output_iterator<std::pair<mapped_type, Arithmetic>> O>
        auto
            collect_reachable
            (
                state_index_type state,
                O result,
                Arithmetic distance
            ) const
            -> O
        {
            const auto & reachable_values = m_reachable_accept_values.at(state);
            return
                std::transform(reachable_values.begin(), reachable_values.end(), result,
                    [this, distance] (auto value_index)
                    {
                        return std::make_pair(begin()[value_index], distance);
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
                using difference_type = std::iter_difference_t<RandomAccessIterator>;
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
                using difference_type = std::iter_difference_t<RandomAccessIterator>;
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
        template <std::forward_iterator I, std::sentinel_for<I> S>
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto traverse (state_index_type state, I first, S last) const
            -> std::pair<state_index_type, I>
        {
            while (first != last)
            {
                auto [next_state, success] = m_trie_automaton.next(state, *first);
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
        template <std::ranges::forward_range R>
        auto traverse (state_index_type source, const R & sequence) const
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
        template <std::input_iterator I, std::sentinel_for<I> S>
            requires(std::is_convertible_v<std::iter_value_t<I>, symbol_type>)
        auto grow (state_index_type state, I first, S last)
            -> state_index_type
        {
            while (first != last)
            {
                auto symbol = *first;
                assert(m_trie_automaton.next(state, symbol) == std::pair(state, false));

                auto [next_state, success] =
                    m_trie_automaton.add_transition(state, std::move(symbol));
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
        template <std::forward_iterator I, std::sentinel_for<I> S>
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto insert (state_index_type source, I first, S last)
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
        template <std::forward_iterator I, std::sentinel_for<I> S>
            requires(std::convertible_to<std::iter_value_t<I>, symbol_type>)
        auto insert (I first, S last)
        {
            return insert(m_trie_automaton.root(), first, last);
        }

        /*!
            \brief
                Добавить в автомат переходы от заданного состояния

            \returns
                `insert(source, begin(sequence), end(sequence))`

            \see insert
        */
        template <std::ranges::forward_range R>
            requires(std::convertible_to<std::ranges::range_value_t<R>, symbol_type>)
        auto insert (state_index_type source, R && sequence)
        {
            using std::begin;
            using std::end;

            return
                insert
                (
                    source,
                    begin(std::forward<R>(sequence)),
                    end(std::forward<R>(sequence))
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
        template <std::ranges::forward_range R>
            requires(std::convertible_to<std::ranges::range_value_t<R>, symbol_type>)
        auto insert (R && sequence)
        {
            using std::begin;
            using std::end;

            return
                insert
                (
                    begin(std::forward<R>(sequence)),
                    end(std::forward<R>(sequence))
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
        template <typename M>
            requires(std::convertible_to<std::remove_cvref_t<M>, mapped_type>)
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

    protected:
        //! Базовый автомат, содержащий состояния и переходы между ними.
        fsm<symbol_type> m_trie_automaton;
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
