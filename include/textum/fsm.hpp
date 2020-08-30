#pragma once

#include <cassert>
#include <cstdint>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace textum
{
    /*!
        \brief
            Конечный автомат

        \details
            Бомж-вариант автомата. По мере развития заменить на что-нибудь готовое, возможно, из
            BGL или что-то подобное.

        \tparam Symbol
            Тим символов, которыми подписаны дуги переходов.
        \tparam TransitionMap
            Тип таблицы переходов. У каждого состояния свой экземпляр таблицы переходов.
    */
    template
    <
        typename Symbol = char,
        typename TransitionMap = std::map<Symbol, std::uint32_t>
    >
    class fsm
    {
    public:
        using symbol_type = Symbol;
        using state_index_type = std::uint32_t;
        using transition_map_type = TransitionMap;

    public:
        /*!
            \brief
                Создание пустого автомата

            \post
                Размер автомата равен единице.
        */
        fsm ():
            m_transitions{transition_map_type{}}
        {
        }

        /*!
            \brief
                Получить индекс корня автомата

            \details
                Выполняется следующее условие:

                    `a.is_root(a.root()) == true`
        */
        state_index_type root () const
        {
            return 0;
        }

        /*!
            \brief
                Проверить, является ли состояние корнем автомата

            \details
                Выполняется следующее условие:

                    `a.is_root(a.root()) == true`
        */
        bool is_root (state_index_type state) const
        {
            return state == root();
        }

        /*!
            \brief
                Переход из заданного состояния автомата по заданному символу

            \param source
                Состояние, из которого требуется найти переход.
            \param symbol
                Символ, переход по которому требуется найти из состояния `source`.

            \post
                `(success && destination != source) || (not success && destination == source)`

            \returns
                Пару `(destination, success)`, где `success` — булево значение, указывающее на то,
                был ли найден требуемый переход, а `destination` — состояние, в которое привёл
                переход в том случае, если он был найден, либо `source` в случае, если переход не
                был найден (иначе говоря, если переход не найден, то остаёмся на месте).
        */
        template <typename S, typename =
            std::enable_if_t<std::is_convertible_v<S, symbol_type>>>
        auto next (state_index_type source, const S & symbol) const
            -> std::pair<state_index_type, bool>
        {
            assert(source < m_transitions.size());

            const auto destination = m_transitions[source].find(symbol);
            if (destination != m_transitions[source].end())
            {
                return std::pair(destination->second, true);
            }
            else
            {
                return std::pair(source, false);
            }
        }

        /*!
            \brief
                Построение нового перехода из заданного состояния по заданному символу

            \param source
                Состояние, из которого требуется построить переход.
            \param symbol
                Символ, переход по которому требуется построить из состояния `source`.

            \post
                `(success && размер автомата увеличился на единицу) ||
                (not success && размер автомата остался прежним)`

            \returns
                Пару `(destination, success)`, где `success` — булево значение, указывающее на то,
                был ли построен требуемый переход, а `destination` — состояние, в которое перешёл
                автомат. Если `success` истинно, то было построено новое состояние и новый переход
                к этому состоянию. Если `success` ложно, то это значит, то переход из состояния
                `source` по символу `symbol` в автомате уже существует, поэтому новые состояния и
                переходы построены не будет, а возвращено будет то состояние, куда вёл уже
                существующий переход.
        */
        template <typename S, typename =
            std::enable_if_t<std::is_convertible_v<S, symbol_type>>>
        auto add_transition (state_index_type source, S && symbol)
            -> std::pair<state_index_type, bool>
        {
            assert(source < m_transitions.size());

            const auto possible_destination =
                static_cast<state_index_type>(m_transitions.size());
            const auto [destination, has_been_inserted] =
                m_transitions[source].emplace(std::forward<S>(symbol), possible_destination);
            if (has_been_inserted)
            {
                m_transitions.resize(m_transitions.size() + 1);
            }

            return std::pair(destination->second, has_been_inserted);
        }

        /*!
            \brief
                Посетить все переходы из заданного состояния

            \details
                Перебирает все переходы из состояния `source` таким образом, что к каждой тройке
                `(source, symbol_i, destination_i)` будет применена трёхместная функция `visit`,
                где `symbol_i` и `destination_i` — символы, по которым есть переход из состояния
                `source`, а `destination_i` — состояние, в которое привёт соответствующий символ.
        */
        template <typename TernaryFunction>
        void visit_transitions (state_index_type source, TernaryFunction && visit) const
        {
            assert(source < m_transitions.size());

            const auto & transitions = m_transitions[source];
            for (const auto & [symbol, destination]: transitions)
            {
                visit(source, symbol, destination);
            }
        }

        /*!
            \brief
                Размер автомата

            \details
                Размер автомата — это количество состояний этого автомата.
        */
        auto size () const
        {
            return m_transitions.size();
        }

    private:
        std::vector<transition_map_type> m_transitions;
    };
} // namespace textum
