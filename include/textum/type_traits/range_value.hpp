#pragma once

#include <textum/type_traits/remove_cvref.hpp>

namespace textum
{
    template <typename Range>
    using range_value_t = typename remove_cvref_t<Range>::value_type;
} // namespace textum
