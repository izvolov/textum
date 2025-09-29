#pragma once

#include <textum/trie_state_attribute.hpp>

#include <limits>

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
} // namespace textum
