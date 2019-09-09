#include <textum/fsm.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

TEST_CASE("Размер пустого автомата равен единице (начальное состояние есть всегда)")
{
    textum::fsm a;

    CHECK(a.size() == 1);
}

TEST_CASE("Вставка нового перехода из корня автомата возвращает пару, состоящую из индекса нового "
    "состояния и индикатора успеха — в данном случае истинного")
{
    textum::fsm<char> a;

    const auto [s, success] = a.add_transition(a.root(), 'a');

    CHECK(s != a.root());
    CHECK(success);
}

TEST_CASE("Вставка нового перехода в пустой автомат увеличивает размер автомата на единицу")
{
    textum::fsm a;

    a.add_transition(a.root(), 'q');

    CHECK(a.size() == 1 + 1);
}

TEST_CASE("Вставка нового перехода из корня непустого автомата увеличивает размер автомата на "
    "единицу")
{
    textum::fsm<int> a;
    a.add_transition(a.root(), 17);
    const auto initial_size = a.size();

    a.add_transition(a.root(), 19);

    CHECK(a.size() == initial_size + 1);
}

TEST_CASE("Вставка нового перехода из некоторого состояния непустого автомата увеличивает размер "
    "автомата на единицу")
{
    textum::fsm<int> a;
    const auto [s, success] = a.add_transition(a.root(), 17);
    REQUIRE(success);
    const auto initial_size = a.size();

    a.add_transition(s, 19);

    CHECK(a.size() == initial_size + 1);
}

TEST_CASE("Вставка нового перехода из состояния S_1 по символу A возвращает состояние S_2, до "
    "которого можно дойти из состояния S_1 по символу A")
{
    textum::fsm<std::string> a;
    const auto s1 = a.root();
    const auto symbol = std::string("qwe");

    const auto s2 = a.add_transition(s1, symbol).first;

    const auto [state, success] = a.next(s1, symbol);
    CHECK(success);
    CHECK(state == s2);
}

TEST_CASE("Посещение переходов из состояния производится при помощи трёхместной функции, "
    "принимающей начальную точку перехода, символ, по которому производится переход, и "
    "конечную точку перехода")
{
    textum::fsm<char> a;
    const auto s1 = a.add_transition(a.root(), '1').first;
    const auto s2 = a.add_transition(s1, '2').first;
    const auto s3 = a.add_transition(s1, '3').first;
    const auto s4 = a.add_transition(s1, '4').first;

    std::vector<std::pair<char, textum::fsm<char>::state_index_type>> visited_transitions;
    a.visit_transitions(s1, [& s1, & visited_transitions]
        (auto source, auto symbol, auto destination)
        {
            CHECK(source == s1);
            visited_transitions.emplace_back(symbol, destination);
        });
    std::sort(visited_transitions.begin(), visited_transitions.end());

    const auto expected_transitions =
        std::vector{std::pair('2', s2), std::pair('3', s3), std::pair('4', s4)};
    CHECK(visited_transitions == expected_transitions);
}
