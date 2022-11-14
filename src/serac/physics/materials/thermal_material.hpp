// Copyright (c) 2019-2022, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

/**
 * @file thermal_material.hpp
 *
 * @brief The material and load types for the thermal functional physics module
 */

#pragma once

#include "serac/numerics/functional/functional.hpp"

namespace serac::heat_transfer {

/// Linear isotropic thermal conduction material model
struct LinearIsotropicConductor {
  /**
   * @brief Construct a new Linear Isotropic Conductor object
   *
   * @param input_density Density of the material (mass/volume)
   * @param input_specific_heat_capacity Specific heat capacity of the material (energy / (mass * temp))
   * @param input_conductivity Thermal conductivity of the material (power / (length * temp))
   */
  LinearIsotropicConductor(double input_density = 1.0, double input_specific_heat_capacity = 1.0,
                           double input_conductivity = 1.0)
      : density(input_density), specific_heat_capacity(input_specific_heat_capacity), conductivity(input_conductivity)
  {
    SLIC_ERROR_ROOT_IF(conductivity < 0.0,
                       "Conductivity must be positive in the linear isotropic conductor material model.");

    SLIC_ERROR_ROOT_IF(density < 0.0, "Density must be positive in the linear isotropic conductor material model.");

    SLIC_ERROR_ROOT_IF(specific_heat_capacity < 0.0,
                       "Specific heat capacity must be positive in the linear isotropic conductor material model.");
  }

  /**
   * @brief Material response call for a linear isotropic material
   *
   * @tparam T1 Spatial position type
   * @tparam T2 Temperature type
   * @tparam T3 Temperature gradient type
   * @param[in] temperature_gradient Temperature gradient
   * @return The calculated material response (density, specific heat capacity, thermal flux) for a linear
   * isotropic material
   */
  template <typename T1, typename T2, typename T3>
  SERAC_HOST_DEVICE auto operator()(const T1& /* x */, const T2& /* temperature */,
                                    const T3& temperature_gradient) const
  {
    return -1.0 * conductivity * temperature_gradient;
  }

  /// Density
  double density;

  /// Specific heat capacity
  double specific_heat_capacity;

  /// Constant isotropic thermal conductivity
  double conductivity;
};

/**
 * @brief Linear anisotropic thermal material model
 *
 * @tparam dim Spatial dimension
 */
template <int dim>
struct LinearConductor {
  /**
   * @brief Construct a new Linear Isotropic Conductor object
   *
   * @param input_density Density of the material (mass/volume)
   * @param input_specific_heat_capacity Specific heat capacity of the material (energy / (mass * temp))
   * @param input_conductivity Thermal conductivity of the material (power / (length * temp))
   */
  LinearConductor(double input_density = 1.0, double input_specific_heat_capacity = 1.0,
                  tensor<double, dim, dim> input_conductivity = Identity<dim>())
      : density(input_density), specific_heat_capacity(input_specific_heat_capacity), conductivity(input_conductivity)
  {
    SLIC_ERROR_ROOT_IF(density < 0.0, "Density must be positive in the linear conductor material model.");

    SLIC_ERROR_ROOT_IF(specific_heat_capacity < 0.0,
                       "Specific heat capacity must be positive in the linear conductor material model.");

    SLIC_ERROR_ROOT_IF(!is_symmetric_and_positive_definite(conductivity),
                       "Conductivity tensor must be symmetric and positive definite.");
  }

  /**
   * @brief Material response call for a linear anisotropic material
   *
   * @tparam T1 Spatial position type
   * @tparam T2 Temperature type
   * @tparam T3 Temperature gradient type
   * @param[in] temperature_gradient Temperature gradient
   * @return The calculated material response (density, specific heat capacity, thermal flux) for a linear
   * anisotropic material
   */
  template <typename T1, typename T2, typename T3>
  SERAC_HOST_DEVICE auto operator()(const T1& /* x */, const T2& /* temperature */,
                                    const T3& temperature_gradient) const
  {
    return -1.0 * conductivity * temperature_gradient;
  }

  /// Density
  double density;

  /// Specific heat capacity
  double specific_heat_capacity;

  /// Constant thermal conductivity
  tensor<double, dim, dim> conductivity;
};

/// Constant thermal source model
struct ConstantSource {
  /// The constant source
  double source_ = 0.0;

  /**
   * @brief Evaluation function for the constant thermal source model
   *
   * @tparam T1 type of the position vector
   * @tparam T2 type of the temperature
   * @tparam T3 type of the temperature gradient
   * @tparam dim The dimension of the problem
   * @return The thermal source value
   */
  template <typename T1, typename T2, typename T3>
  SERAC_HOST_DEVICE auto operator()(const T1& /* x */, const double /* time */, const T2& /* temperature */,
                                    const T3& /* temperature_gradient */) const
  {
    return source_;
  }
};

/// Constant thermal flux boundary model
struct ConstantFlux {
  /// The constant flux applied to the boundary
  double flux_ = 0.0;

  /**
   * @brief Evaluation function for the thermal flux on a boundary
   *
   * @tparam T1 Type of the position vector
   * @tparam T2 Type of the normal vector
   * @tparam T3 Type of the temperature on the boundary
   * @tparam dim The dimension of the problem
   * @return The flux applied to the boundary
   */
  template <typename T1, typename T2, typename T3>
  SERAC_HOST_DEVICE auto operator()(const T1& /* x */, const T2& /* normal */, const T3& /* temperature */) const
  {
    return flux_;
  }
};

}  // namespace serac::heat_transfer
