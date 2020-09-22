#pragma once

#include <cstdint>
#include <limits>

namespace textum
{
    template <typename Arithmetic>
    constexpr auto infinity = std::numeric_limits<Arithmetic>::max();

    template <typename Arithmetic>
    struct inversed_indicator_t
    {
        template <typename T, typename U>
        Arithmetic operator () (const T & t, const U & u) const
        {
            return t == u ? 0 : 1;
        }
    };
    template <typename Arithmetic>
    constexpr auto inversed_indicator = inversed_indicator_t<Arithmetic>{};

    template <typename Arithmetic>
    struct always_one_t
    {
        template <typename T>
        Arithmetic operator () (const T &) const
        {
            return 1;
        }
    };
    template <typename Arithmetic>
    constexpr auto always_one = always_one_t<Arithmetic>{};

    /*!
        \brief
            Параметры нечёткого поиска по расстоянию Левенштейна

        \details
            Содержит ограничение на расстояние между исследуемыми последовательностями, а также
            функции, с помощью которых будут взвешиваться редакционные операции.
    */
    template
    <
        typename Arithmetic,
        typename UnaryFunction = always_one_t<Arithmetic>,
        typename BinaryFunction = inversed_indicator_t<Arithmetic>
    >
    struct levenshtein_parameters_t
    {
        /*!
            Расстояние, устанавливающее порог на сравнение последовательностей.
            Последовательности, расстояние до которых превысит данный порог, должны быть выброшены
            из рассмотрения.
        */
        Arithmetic distance_limit = infinity<Arithmetic>;
        /*!
            Цена удаления и вставки символа.
            Удаление и вставка — это одно и то же в силу симметричности расстояния (удаление из
            первой последовательности эквивалентно вставке во вторую, и наоборот).
        */
        UnaryFunction deletion_or_insertion_penalty = always_one<Arithmetic>;
        /*!
            Цена замены одного символа на другой.
            Эта функция обрабатывает и ситуацию, когда взвешивается замена символа на самого себя.
            Цена замены символа на равный ему символ по умолчанию равна нулю.
        */
        BinaryFunction replacement_penalty = inversed_indicator<Arithmetic>;
    };
    template <typename Arithmetic = std::int32_t>
    constexpr auto default_levenshtein = levenshtein_parameters_t<Arithmetic>{};

    /*!
        \brief
            Создание параметров поиска по Левенштейну

        \param d
            Порог расстояния для поиска.

        \returns
            Экземпляр `levenshtein_parameters_t` с порогом, равным `d`, и базовыми ценами
            редакционных операций: и вставка, и удаление, и замена будут стоить единицу.

        \see levenshtein_parameters_t
    */
    template <typename Arithmetic>
    constexpr auto levenshtein (Arithmetic d)
        -> levenshtein_parameters_t<Arithmetic>
    {
        return {d};
    }

    template <typename Arithmetic, typename UnaryFunction, typename BinaryFunction>
    constexpr auto
        levenshtein
        (
            Arithmetic distance,
            UnaryFunction deletion_or_insertion,
            BinaryFunction replacement
        )
        -> levenshtein_parameters_t<Arithmetic, UnaryFunction, BinaryFunction>
    {
        return {distance, std::move(deletion_or_insertion), std::move(replacement)};
    }
} // namespace textum
