#pragma once

#include <textum/type_traits/remove_cvref.hpp>

#include <iterator>

namespace textum
{
    template <typename Iterator>
    using iterator_value_t = remove_cvref_t<typename std::iterator_traits<Iterator>::value_type>;
} // namespace textum
