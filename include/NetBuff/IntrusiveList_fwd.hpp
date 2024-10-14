#pragma once

#include <type_traits>

namespace nb
{

// Your custom type `A` should inherit this to store it inside `InstrusiveList<A>`
struct IntrusiveListNode;

template <typename T>
    requires std::is_base_of_v<IntrusiveListNode, T>
class IntrusiveList;

} // namespace nb
