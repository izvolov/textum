#pragma once

#include <textum/basic_trie.hpp>
#include <textum/trie_state_attribute.hpp>

#include <cstdint>

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

        \see
            basic_trie
    */
    template <typename Symbol, typename Mapped>
    class trie: public basic_trie<Symbol, Mapped, std::uint32_t, trie_state_attribute_t>
    {
    private:
        using base_type = basic_trie<Symbol, Mapped, std::uint32_t, trie_state_attribute_t>;

    public:
        using base_type::base_type;
    };
} // namespace textum
