// Copyright (c) 2019-2023, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

/**
 * @file metaprogramming.hpp
 *
 * @brief Utilities for C++ metaprogramming
 */

#pragma once

#include <tuple>
#include <utility>
#include <type_traits>

#include "serac/infrastructure/accelerator.hpp"

/**
 * @brief return the Ith integer in `{n...}`
 */
template <int I, int... n>
constexpr auto get(std::integer_sequence<int, n...>)
{
  constexpr int values[sizeof...(n)] = {n...};
  return values[I];
}

/// @cond
namespace detail {

template <typename T>
struct always_false : std::false_type {
};

/**
 * @brief unfortunately std::integral_constant doesn't have __host__ __device__ annotations
 * and we're not using --expt-relaxed-constexpr, so we need to implement something similar
 * to use it in a device context
 *
 * @tparam i the value represented by this struct
 */
template <int i>
struct integral_constant {
  SERAC_HOST_DEVICE constexpr operator int() { return i; }
};

template <int... i, typename lambda>
SERAC_HOST_DEVICE constexpr void for_constexpr(lambda&& f, std::integer_sequence<int, i...>) {
    (f(integral_constant<i>{}), ...);
}

}

template <int n, typename lambda>
SERAC_HOST_DEVICE constexpr void for_constexpr(lambda&& f) {
    detail::for_constexpr(f, std::make_integer_sequence<int, n>{});
}
