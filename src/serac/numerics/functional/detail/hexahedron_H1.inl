// Copyright (c) 2019-2022, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

/**
 * @file hexahedron_H1.inl
 *
 * @brief Specialization of finite_element for H1 on hexahedron geometry
 */

// this specialization defines shape functions (and their gradients) that
// interpolate at Gauss-Lobatto nodes for the appropriate polynomial order
//
// note: mfem assumes the parent element domain is [0,1]x[0,1]x[0,1]
// for additional information on the finite_element concept requirements, see finite_element.hpp
/// @cond
template <int p, int c>
struct finite_element<Geometry::Hexahedron, H1<p, c> > {
  static constexpr auto geometry   = Geometry::Hexahedron;
  static constexpr auto family     = Family::H1;
  static constexpr int  components = c;
  static constexpr int  dim        = 3;
  static constexpr int  n          = (p + 1);
  static constexpr int  ndof       = (p + 1) * (p + 1) * (p + 1);
  static constexpr int  order      = p;


  // TODO: remove this
  using residual_type =
      typename std::conditional<components == 1, tensor<double, ndof>, tensor<double, ndof, components> >::type;

  using dof_type = tensor< double, c, p + 1, p + 1, p + 1 >;

  /**
   * @brief this type is used when calling the batched interpolate/integrate
   *        routines, to provide memory for calculating intermediates
   */
  template < int q >
  struct cache_type {
    tensor<double, 2, n, n, q> A1;
    tensor<double, 3, n, q, q> A2;
  };

  template < typename T >
  using simd_dof_type = tensor< tensor< T, c >, p + 1, p + 1, p + 1 >;

  template < typename T, int q >
  struct simd_cache_type {
    tensor<T, 2, n, n, q> A1;
    tensor<T, 3, n, q, q> A2;
  };

  template < int q >
  using cpu_batched_values_type = tensor< tensor< double, c >, q, q, q >;

  template < int q >
  using cpu_batched_derivatives_type = tensor< tensor< double, c, 3 >, q, q, q >;

  SERAC_HOST_DEVICE static constexpr tensor<double, ndof> shape_functions(tensor<double, dim> xi)
  {
    auto N_xi   = GaussLobattoInterpolation<p + 1>(xi[0]);
    auto N_eta  = GaussLobattoInterpolation<p + 1>(xi[1]);
    auto N_zeta = GaussLobattoInterpolation<p + 1>(xi[2]);

    int count = 0;

    tensor<double, ndof> N{};
    for (int k = 0; k < p + 1; k++) {
      for (int j = 0; j < p + 1; j++) {
        for (int i = 0; i < p + 1; i++) {
          N[count++] = N_xi[i] * N_eta[j] * N_zeta[k];
        }
      }
    }
    return N;
  }

  SERAC_HOST_DEVICE static constexpr tensor<double, ndof, dim> shape_function_gradients(tensor<double, dim> xi)
  {
    auto N_xi    = GaussLobattoInterpolation<p + 1>(xi[0]);
    auto N_eta   = GaussLobattoInterpolation<p + 1>(xi[1]);
    auto N_zeta  = GaussLobattoInterpolation<p + 1>(xi[2]);
    auto dN_xi   = GaussLobattoInterpolationDerivative<p + 1>(xi[0]);
    auto dN_eta  = GaussLobattoInterpolationDerivative<p + 1>(xi[1]);
    auto dN_zeta = GaussLobattoInterpolationDerivative<p + 1>(xi[2]);

    int count = 0;

    // clang-format off
    tensor<double, ndof, dim> dN{};
    for (int k = 0; k < p + 1; k++) {
      for (int j = 0; j < p + 1; j++) {
        for (int i = 0; i < p + 1; i++) {
          dN[count++] = {
            dN_xi[i] *  N_eta[j] *  N_zeta[k], 
             N_xi[i] * dN_eta[j] *  N_zeta[k],
             N_xi[i] *  N_eta[j] * dN_zeta[k]
          };
        }
      }
    }
    return dN;
    // clang-format on
  }

  template < int q >
  static auto interpolate(const dof_type & X, 
                          const tensor < double, dim, dim, q, q, q > & jacobians,
                          const TensorProductQuadratureRule<q> &) {

    // we want to compute the following:
    //
    // X_q(u, v, w) := (B(u, i) * B(v, j) * B(w, k)) * X_e(i, j, k)
    //
    // where 
    //   X_q(u, v, w) are the quadrature-point values at position {u, v, w}, 
    //   B(u, i) is the i^{th} 1D interpolation/differentiation (shape) function, 
    //           evaluated at the u^{th} 1D quadrature point, and
    //   X_e(i, j, k) are the values at node {i, j, k} to be interpolated 
    //
    // this algorithm carries out the above calculation in 3 steps:
    //
    // A1(dz, dy, qx)  := B(qx, dx) * X_e(dz, dy, dx)
    // A2(dz, qy, qx)  := B(qy, dy) * A1(dz, dy, qx)
    // X_q(qz, qy, qx) := B(qz, dz) * A2(dz, qy, qx)

    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B = [=](){
      tensor< double, q, n > B_{};
      for (int i = 0; i < q; i++) {
        B_[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B_;
    }();

    static constexpr auto G = [=](){
      tensor< double, q, n > G_{};
      for (int i = 0; i < q; i++) {
        G_[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G_;
    }();

    cache_type<q> cache;

    serac::tuple < cpu_batched_values_type<q>, cpu_batched_derivatives_type<q> > values_and_derivatives{};

    for (int i = 0; i < c; i++) {

      for (int dz = 0; dz < n; dz++) {
        for (int dy = 0; dy < n; dy++) {
          for (int qx = 0; qx < q; qx++) {
            double sum[2]{};
            for (int dx = 0; dx < n; dx++) {
              sum[0] += B(qx, dx) * X(i, dz, dy, dx);
              sum[1] += G(qx, dx) * X(i, dz, dy, dx);
            }
            cache.A1(0, dz, dy, qx) = sum[0];
            cache.A1(1, dz, dy, qx) = sum[1];
          }
        }
      }

      for (int dz = 0; dz < n; dz++) {
        for (int qy = 0; qy < q; qy++) {
          for (int qx = 0; qx < q; qx++) {
            double sum[3]{};
            for (int dy = 0; dy < n; dy++) {
              sum[0] += B(qy, dy) * cache.A1(0, dz, dy, qx);
              sum[1] += B(qy, dy) * cache.A1(1, dz, dy, qx);
              sum[2] += G(qy, dy) * cache.A1(0, dz, dy, qx);
            }
            cache.A2(0, dz, qy, qx) = sum[0];
            cache.A2(1, dz, qy, qx) = sum[1];
            cache.A2(2, dz, qy, qx) = sum[2];
          }
        }
      }

      for (int qz = 0; qz < q; qz++) {
        for (int qy = 0; qy < q; qy++) {
          for (int qx = 0; qx < q; qx++) {
            for (int dz = 0; dz <n; dz++) {
              serac::get<0>(values_and_derivatives)(qz, qy, qx)(i)    += B(qz, dz) * cache.A2(0, dz, qy, qx);
              serac::get<1>(values_and_derivatives)(qz, qy, qx)(i, 0) += B(qz, dz) * cache.A2(1, dz, qy, qx);
              serac::get<1>(values_and_derivatives)(qz, qy, qx)(i, 1) += B(qz, dz) * cache.A2(2, dz, qy, qx);
              serac::get<1>(values_and_derivatives)(qz, qy, qx)(i, 2) += G(qz, dz) * cache.A2(0, dz, qy, qx);
            }
          }
        }
      }

      for (int qz = 0; qz < q; qz++) {
        for (int qy = 0; qy < q; qy++) {
          for (int qx = 0; qx < q; qx++) {
            tensor< double, dim, dim > J;
            for (int row = 0; row < dim; row++) {
              for (int col = 0; col < dim; col++) {
                J[row][col] = jacobians(col, row, qz, qy, qx);
              }
            }
            auto grad_u = serac::get<1>(values_and_derivatives)(qz, qy, qx);
            serac::get<1>(values_and_derivatives)(qz, qy, qx) = dot(grad_u, inv(J));
          }
        }
      }

    }

    return values_and_derivatives;
  }



  template < int q >
  static void integrate(cpu_batched_values_type<q> & sources,
                        cpu_batched_derivatives_type<q> & fluxes,
                        const tensor < double, dim, dim, q, q, q > & jacobians,
                        const TensorProductQuadratureRule<q> &, 
                        dof_type & element_residual) {

    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto weights1D = GaussLegendreWeights<q>();
    static constexpr auto B = [=](){
      tensor< double, q, n > B_{};
      for (int i = 0; i < q; i++) {
        B_[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B_;
    }();

    static constexpr auto G = [=](){
      tensor< double, q, n > G_{};
      for (int i = 0; i < q; i++) {
        G_[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G_;
    }();

    cache_type<q> cache{};

    for (int qz = 0; qz < q; qz++) {
      for (int qy = 0; qy < q; qy++) {
        for (int qx = 0; qx < q; qx++) {
          tensor< double, dim, dim > J_T;
          for (int row = 0; row < dim; row++) {
            for (int col = 0; col < dim; col++) {
              J_T[row][col] = jacobians(row, col, qz, qy, qx);
            }
          }
          auto dv = det(J_T) * weights1D[qx] * weights1D[qy] * weights1D[qz];
          sources(qz, qy, qx) = sources(qz, qy, qx) * dv;
          fluxes(qz, qy, qx) = dot(fluxes(qz, qy, qx), inv(J_T)) * dv;
        }
      }
    }

    for (int i = 0; i < c; i++) {

      for (int dx = 0; dx < n; dx++) {
        for (int qy = 0; qy < q; qy++) {
          for (int qz = 0; qz < q; qz++) {
            double sum[3]{};
            for (int qx = 0; qx < q; qx++) {
              sum[0] += B(qx, dx) * sources(qz, qy, qx)[i];
              sum[0] += G(qx, dx) * fluxes(qz, qy, qx)[i][0];
              sum[1] += B(qx, dx) * fluxes(qz, qy, qx)[i][1];
              sum[2] += B(qx, dx) * fluxes(qz, qy, qx)[i][2];
            }
            cache.A2(0, dx, qy, qz) = sum[0];
            cache.A2(1, dx, qy, qz) = sum[1];
            cache.A2(2, dx, qy, qz) = sum[2];
          }
        }
      }

      for (int dx = 0; dx < n; dx++) {
        for (int dy = 0; dy < n; dy++) {
          for (int qz = 0; qz < q; qz++) {
            double sum[2]{};
            for (int qy = 0; qy < q; qy++) {
              sum[0] += B(qy, dy) * cache.A2(0, dx, qy, qz);
              sum[0] += G(qy, dy) * cache.A2(1, dx, qy, qz);
              sum[1] += B(qy, dy) * cache.A2(2, dx, qy, qz);
            }
            cache.A1(0, dx, dy, qz) = sum[0];
            cache.A1(1, dx, dy, qz) = sum[1];
          }
        }
      }

      for (int dx = 0; dx < n; dx++) {
        for (int dy = 0; dy < n; dy++) {
          for (int dz = 0; dz < n; dz++) {
            double sum = 0.0;
            for (int qz = 0; qz < q; qz++) {
              sum += B(qz, dz) * cache.A1(0, dx, dy, qz);
              sum += G(qz, dz) * cache.A1(1, dx, dy, qz);
            }
            element_residual(i,dz,dy,dx) += sum;
          }
        }
      }

    }

  }

  template < typename T, int q >
  static auto interpolate(const simd_dof_type<T> & X, 
                          const TensorProductQuadratureRule<q> &) {

    // we want to compute the following:
    //
    // X_q(u, v, w) := (B(u, i) * B(v, j) * B(w, k)) * X_e(i, j, k)
    //
    // where 
    //   X_q(u, v, w) are the quadrature-point values at position {u, v, w}, 
    //   B(u, i) is the i^{th} 1D interpolation/differentiation (shape) function, 
    //           evaluated at the u^{th} 1D quadrature point, and
    //   X_e(i, j, k) are the values at node {i, j, k} to be interpolated 
    //
    // this algorithm carries out the above calculation in 3 steps:
    //
    // A1(dz, dy, qx)  := B(qx, dx) * X_e(dz, dy, dx)
    // A2(dz, qy, qx)  := B(qy, dy) * A1(dz, dy, qx)
    // X_q(qz, qy, qx) := B(qz, dz) * A2(dz, qy, qx)

    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B = [=](){
      tensor< double, q, n > B_{};
      for (int i = 0; i < q; i++) {
        B_[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B_;
    }();

    static constexpr auto G = [=](){
      tensor< double, q, n > G_{};
      for (int i = 0; i < q; i++) {
        G_[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G_;
    }();

    simd_cache_type<T, q> cache;

    serac::tuple < tensor< tensor< T, c >, q, q, q >, 
                   tensor< tensor< T, c, 3 >, q, q, q > > values_and_derivatives{};

    for (int i = 0; i < c; i++) {

      for (int dz = 0; dz < n; dz++) {
        for (int dy = 0; dy < n; dy++) {
          for (int qx = 0; qx < q; qx++) {
            T sum[2]{};
            for (int dx = 0; dx < n; dx++) {
              sum[0] += B(qx, dx) * X(dz, dy, dx)[i];
              sum[1] += G(qx, dx) * X(dz, dy, dx)[i];
            }
            cache.A1(0, dz, dy, qx) = sum[0];
            cache.A1(1, dz, dy, qx) = sum[1];
          }
        }
      }

      for (int dz = 0; dz < n; dz++) {
        for (int qy = 0; qy < q; qy++) {
          for (int qx = 0; qx < q; qx++) {
            T sum[3]{};
            for (int dy = 0; dy < n; dy++) {
              sum[0] += B(qy, dy) * cache.A1(0, dz, dy, qx);
              sum[1] += B(qy, dy) * cache.A1(1, dz, dy, qx);
              sum[2] += G(qy, dy) * cache.A1(0, dz, dy, qx);
            }
            cache.A2(0, dz, qy, qx) = sum[0];
            cache.A2(1, dz, qy, qx) = sum[1];
            cache.A2(2, dz, qy, qx) = sum[2];
          }
        }
      }

      for (int qz = 0; qz < q; qz++) {
        for (int qy = 0; qy < q; qy++) {
          for (int qx = 0; qx < q; qx++) {
            for (int dz = 0; dz < n; dz++) {
              serac::get<0>(values_and_derivatives)(qz, qy, qx)(i)    += B(qz, dz) * cache.A2(0, dz, qy, qx);
              serac::get<1>(values_and_derivatives)(qz, qy, qx)(i, 0) += B(qz, dz) * cache.A2(1, dz, qy, qx);
              serac::get<1>(values_and_derivatives)(qz, qy, qx)(i, 1) += B(qz, dz) * cache.A2(2, dz, qy, qx);
              serac::get<1>(values_and_derivatives)(qz, qy, qx)(i, 2) += G(qz, dz) * cache.A2(0, dz, qy, qx);
            }
          }
        }
      }

    }

    return values_and_derivatives;
  }

  template < typename T, typename source_type, typename flux_type, int q >
  static void integrate(source_type & sources, flux_type & fluxes,
                        const TensorProductQuadratureRule<q> &, 
                        simd_dof_type<T> & element_residual) {

    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B = [=](){
      tensor< double, q, n > B_{};
      for (int i = 0; i < q; i++) {
        B_[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B_;
    }();

    static constexpr auto G = [=](){
      tensor< double, q, n > G_{};
      for (int i = 0; i < q; i++) {
        G_[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G_;
    }();

    simd_cache_type<T,q> cache{};

    for (int i = 0; i < c; i++) {

      for (int dx = 0; dx < n; dx++) {
        for (int qy = 0; qy < q; qy++) {
          for (int qz = 0; qz < q; qz++) {
            T sum[3]{};
            for (int qx = 0; qx < q; qx++) {
              sum[0] += B(qx, dx) * sources(qz, qy, qx)[i];
              sum[0] += G(qx, dx) * fluxes(qz, qy, qx)[i][0];
              sum[1] += B(qx, dx) * fluxes(qz, qy, qx)[i][1];
              sum[2] += B(qx, dx) * fluxes(qz, qy, qx)[i][2];
            }
            cache.A2(0, dx, qy, qz) = sum[0];
            cache.A2(1, dx, qy, qz) = sum[1];
            cache.A2(2, dx, qy, qz) = sum[2];
          }
        }
      }

      for (int dx = 0; dx < n; dx++) {
        for (int dy = 0; dy < n; dy++) {
          for (int qz = 0; qz < q; qz++) {
            T sum[2]{};
            for (int qy = 0; qy < q; qy++) {
              sum[0] += B(qy, dy) * cache.A2(0, dx, qy, qz);
              sum[0] += G(qy, dy) * cache.A2(1, dx, qy, qz);
              sum[1] += B(qy, dy) * cache.A2(2, dx, qy, qz);
            }
            cache.A1(0, dx, dy, qz) = sum[0];
            cache.A1(1, dx, dy, qz) = sum[1];
          }
        }
      }

      for (int dx = 0; dx < n; dx++) {
        for (int dy = 0; dy < n; dy++) {
          for (int dz = 0; dz < n; dz++) {
            T sum{};
            for (int qz = 0; qz < q; qz++) {
              sum += B(qz, dz) * cache.A1(0, dx, dy, qz);
              sum += G(qz, dz) * cache.A1(1, dx, dy, qz);
            }
            element_residual(dz,dy,dx)[i] += sum;
          }
        }
      }

    }

  }

#ifdef __CUDACC__
  template < int q >
  static SERAC_DEVICE auto interpolate(const tensor<double, c, n, n, n>& X, const TensorProductQuadratureRule<q> & rule, 
                              tensor<double, 2, n, n, q>& A1, tensor<double, 3, n, q, q>& A2) {

    // we want to compute the following:
    //
    // X_q(u, v, w) := (B(u, i) * B(v, j) * B(w, k)) * X_e(i, j, k)
    //
    // where 
    //   X_q(u, v, w) are the quadrature-point values at position {u, v, w}, 
    //   B(u, i) is the i^{th} 1D interpolation/differentiation (shape) function, 
    //           evaluated at the u^{th} 1D quadrature point, and
    //   X_e(i, j, k) are the values at node {i, j, k} to be interpolated 
    //
    // this algorithm carries out the above calculation in 3 steps:
    //
    // A1(dz, dy, qx)  := B(qx, dx) * X_e(dz, dy, dx)
    // A2(dz, qy, qx)  := B(qy, dy) * A1(dz, dy, qx)
    // X_q(qz, qy, qx) := B(qz, dz) * A2(dz, qy, qx)

    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B_ = [=](){
      tensor< double, q, n > B{};
      for (int i = 0; i < q; i++) {
        B[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B;
    }();

    static constexpr auto G_ = [=](){
      tensor< double, q, n > G{};
      for (int i = 0; i < q; i++) {
        G[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G;
    }();

    __shared__ tensor< double, q, n > B;
    __shared__ tensor< double, q, n > G;
    if (threadIdx.z == 0) {
      for (int j = threadIdx.y; j < q; j += blockDim.y) {
        for (int i = threadIdx.x; i < n; i += blockDim.x) {
          B(j, i) = B_(j, i);
          G(j, i) = G_(j, i);
        }
      }
    }
    __syncthreads();

    tuple < tensor<double, c>, tensor<double, c, 3> > qf_input{};

    for (int i = 0; i < c; i++) {

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            double sum[2]{};
            for (int dx = 0; dx < n; dx++) {
              sum[0] += B(qx, dx) * X(i, dz, dy, dx);
              sum[1] += G(qx, dx) * X(i, dz, dy, dx);
            }
            A1(0, dz, dy, qx) = sum[0];
            A1(1, dz, dy, qx) = sum[1];
          }
        }
      }
      __syncthreads();

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            double sum[3]{};
            for (int dy = 0; dy < n; dy++) {
              sum[0] += B(qy, dy) * A1(0, dz, dy, qx);
              sum[1] += B(qy, dy) * A1(1, dz, dy, qx);
              sum[2] += G(qy, dy) * A1(0, dz, dy, qx);
            }
            A2(0, dz, qy, qx) = sum[0];
            A2(1, dz, qy, qx) = sum[1];
            A2(2, dz, qy, qx) = sum[2];
          }
        }
      }
      __syncthreads();

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            for (int dz = 0; dz <n; dz++) {
              serac::get<0>(qf_input)[i]    += B(qz, dz) * A2(0, dz, qy, qx);
              serac::get<1>(qf_input)[i][0] += B(qz, dz) * A2(1, dz, qy, qx);
              serac::get<1>(qf_input)[i][1] += B(qz, dz) * A2(2, dz, qy, qx);
              serac::get<1>(qf_input)[i][2] += G(qz, dz) * A2(0, dz, qy, qx);
            }
          }
        }
      }

    }

    return qf_input;
  }

  // value-only interpolation
  template < int q >
  static SERAC_DEVICE auto interpolate(const tensor<double, c, n, n, n>& X, const TensorProductQuadratureRule<q> & rule, 
                              tensor<double, n, n, q>& A1, tensor<double, n, q, q>& A2) {

    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B_ = [=](){
      tensor< double, q, n > B{};
      for (int i = 0; i < q; i++) {
        B[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B;
    }();

    __shared__ tensor< double, q, n > B;
    if (threadIdx.z == 0) {
      for (int j = threadIdx.y; j < q; j += blockDim.y) {
        for (int i = threadIdx.x; i < n; i += blockDim.x) {
          B(j, i) = B_(j, i);
        }
      }
    }
    __syncthreads();

    tensor<double, c> qf_input{};

    for (int i = 0; i < c; i++) {

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            double sum = 0.0;
            for (int dx = 0; dx < n; dx++) {
              sum += B(qx, dx) * X(i, dz, dy, dx);
            }
            A1(dz, dy, qx) = sum;
          }
        }
      }
      __syncthreads();

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            double sum = 0.0;
            for (int dy = 0; dy < n; dy++) {
              sum += B(qy, dy) * A1(dz, dy, qx);
            }
            A2(dz, qy, qx) = sum;
          }
        }
      }
      __syncthreads();

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            for (int dz = 0; dz <n; dz++) {
              qf_input[i] += B(qz, dz) * A2(dz, qy, qx);
            }
          }
        }
      }

    }

    return qf_input;
  }

  // gradient-only calculation
  template < int q >
  static SERAC_DEVICE auto gradient(const tensor<double, c, n, n, n>& X, const TensorProductQuadratureRule<q> & rule, 
                              tensor<double, 2, n, n, q>& A1, tensor<double, 3, n, q, q>& A2) {

    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B_ = [=](){
      tensor< double, q, n > B{};
      for (int i = 0; i < q; i++) {
        B[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B;
    }();

    static constexpr auto G_ = [=](){
      tensor< double, q, n > G{};
      for (int i = 0; i < q; i++) {
        G[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G;
    }();

    __shared__ tensor< double, q, n > B;
    __shared__ tensor< double, q, n > G;
    if (threadIdx.z == 0) {
      for (int j = threadIdx.y; j < q; j += blockDim.y) {
        for (int i = threadIdx.x; i < n; i += blockDim.x) {
          B(j, i) = B_(j, i);
          G(j, i) = G_(j, i);
        }
      }
    }
    __syncthreads();

    tensor<double, c, 3> qf_input{};

    for (int i = 0; i < c; i++) {

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            double sum[2]{};
            for (int dx = 0; dx < n; dx++) {
              sum[0] += B(qx, dx) * X(i, dz, dy, dx);
              sum[1] += G(qx, dx) * X(i, dz, dy, dx);
            }
            A1(0, dz, dy, qx) = sum[0];
            A1(1, dz, dy, qx) = sum[1];
          }
        }
      }
      __syncthreads();

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            double sum[3]{};
            for (int dy = 0; dy < n; dy++) {
              sum[0] += B(qy, dy) * A1(0, dz, dy, qx);
              sum[1] += B(qy, dy) * A1(1, dz, dy, qx);
              sum[2] += G(qy, dy) * A1(0, dz, dy, qx);
            }
            A2(0, dz, qy, qx) = sum[0];
            A2(1, dz, qy, qx) = sum[1];
            A2(2, dz, qy, qx) = sum[2];
          }
        }
      }
      __syncthreads();

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int qx = threadIdx.x; qx < q; qx += blockDim.x) {
            for (int dz = 0; dz <n; dz++) {
              qf_input[i][0] += B(qz, dz) * A2(1, dz, qy, qx);
              qf_input[i][1] += B(qz, dz) * A2(2, dz, qy, qx);
              qf_input[i][2] += G(qz, dz) * A2(0, dz, qy, qx);
            }
          }
        }
      }

    }

    return qf_input;
  }

  template <int q>
  static SERAC_DEVICE void integrate(const tensor<double, c, q, q, q> & source, const tensor<double, 3, c, q, q, q> & flux,
                                     const TensorProductQuadratureRule<q> & rule, 
                                     mfem::DeviceTensor<5, double> r_e, int e,
                                     tensor<double, 3, q, q, n>& A1, tensor<double, 2, q, n, n>& A2) {
                                      
    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B_ = [=](){
      tensor< double, q, n > B{};
      for (int i = 0; i < q; i++) {
        B[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B;
    }();

    static constexpr auto G_ = [=](){
      tensor< double, q, n > G{};
      for (int i = 0; i < q; i++) {
        G[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G;
    }();

    __shared__ tensor< double, q, n > B;
    __shared__ tensor< double, q, n > G;
    if (threadIdx.z == 0) {
      for (int j = threadIdx.y; j < q; j += blockDim.y) {
        for (int i = threadIdx.x; i < n; i += blockDim.x) {
          B(j, i) = B_(j, i);
          G(j, i) = G_(j, i);
        }
      }
    }
    __syncthreads();

    for (int i = 0; i < c; i++) {

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum[3]{};
            for (int qx = 0; qx < q; qx++) {
              sum[0] += B(qx, dx) * source(i, qz, qy, qx);
              sum[0] += G(qx, dx) * flux(0, i, qz, qy, qx);
              sum[1] += B(qx, dx) * flux(1, i, qz, qy, qx);
              sum[2] += B(qx, dx) * flux(2, i, qz, qy, qx);
            }
            A1(0, qz, qy, dx) = sum[0];
            A1(1, qz, qy, dx) = sum[1];
            A1(2, qz, qy, dx) = sum[2];
          }
        }
      }
      __syncthreads();

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum[2]{};
            for (int qy = 0; qy < q; qy++) {
              sum[0] += B(qy, dy) * A1(0, qz, qy, dx);
              sum[0] += G(qy, dy) * A1(1, qz, qy, dx);
              sum[1] += B(qy, dy) * A1(2, qz, qy, dx);
            }
            A2(0, qz, dy, dx) = sum[0];
            A2(1, qz, dy, dx) = sum[1];
          }
        }
      }
      __syncthreads();

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum = 0.0;
            for (int qz = 0; qz < q; qz++) {
              sum += B(qz, dz) * A2(0, qz, dy, dx);
              sum += G(qz, dz) * A2(1, qz, dy, dx);
            }
            r_e(dx,dy,dz,i,e) += sum;
          }
        }
      }

    }

  }

  // source-only integrate
  template <int q>
  SERAC_DEVICE static void integrate(const tensor<double, c, q, q, q> & source,
                                     const TensorProductQuadratureRule<q> & rule, 
                                     mfem::DeviceTensor<5, double> r_e, int e,
                                     tensor<double, q, q, n>& A1, tensor<double, q, n, n>& A2) {
                                      
    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B_ = [=](){
      tensor< double, q, n > B{};
      for (int i = 0; i < q; i++) {
        B[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B;
    }();

    __shared__ tensor< double, q, n > B;
    if (threadIdx.z == 0) {
      for (int j = threadIdx.y; j < q; j += blockDim.y) {
        for (int i = threadIdx.x; i < n; i += blockDim.x) {
          B(j, i) = B_(j, i);
        }
      }
    }
    __syncthreads();

    for (int i = 0; i < c; i++) {

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum = 0.0;
            for (int qx = 0; qx < q; qx++) {
              sum += B(qx, dx) * source(i, qz, qy, qx);
            }
            A1(qz, qy, dx) = sum;
          }
        }
      }
      __syncthreads();

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum = 0.0;
            for (int qy = 0; qy < q; qy++) {
              sum += B(qy, dy) * A1(qz, qy, dx);
            }
            A2(qz, dy, dx) = sum;
          }
        }
      }
      __syncthreads();

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum = 0.0;
            for (int qz = 0; qz < q; qz++) {
              sum += B(qz, dz) * A2(qz, dy, dx);
            }
            r_e(dx,dy,dz,i,e) += sum;
          }
        }
      }

    }

  }


  // flux-only integrate
  template <int q>
  SERAC_DEVICE static void integrate(const tensor<double, 3, c, q, q, q> & flux,
                                     const TensorProductQuadratureRule<q> & rule, 
                                     mfem::DeviceTensor<5, double> r_e, int e,
                                     tensor<double, 3, q, q, n>& A1, tensor<double, 2, q, n, n>& A2) {
                                      
    static constexpr auto points1D = GaussLegendreNodes<q>();
    static constexpr auto B_ = [=](){
      tensor< double, q, n > B{};
      for (int i = 0; i < q; i++) {
        B[i] = GaussLobattoInterpolation<n>(points1D[i]);
      }
      return B;
    }();

    static constexpr auto G_ = [=](){
      tensor< double, q, n > G{};
      for (int i = 0; i < q; i++) {
        G[i] = GaussLobattoInterpolationDerivative<n>(points1D[i]);
      }
      return G;
    }();

    __shared__ tensor< double, q, n > B;
    __shared__ tensor< double, q, n > G;
    if (threadIdx.z == 0) {
      for (int j = threadIdx.y; j < q; j += blockDim.y) {
        for (int i = threadIdx.x; i < n; i += blockDim.x) {
          B(j, i) = B_(j, i);
          G(j, i) = G_(j, i);
        }
      }
    }
    __syncthreads();

    for (int i = 0; i < c; i++) {

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int qy = threadIdx.y; qy < q; qy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum[3]{};
            for (int qx = 0; qx < q; qx++) {
              sum[0] += G(qx, dx) * flux(0, i, qz, qy, qx);
              sum[1] += B(qx, dx) * flux(1, i, qz, qy, qx);
              sum[2] += B(qx, dx) * flux(2, i, qz, qy, qx);
            }
            A1(0, qz, qy, dx) = sum[0];
            A1(1, qz, qy, dx) = sum[1];
            A1(2, qz, qy, dx) = sum[2];
          }
        }
      }
      __syncthreads();

      for (int qz = threadIdx.z; qz < q; qz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum[2]{};
            for (int qy = 0; qy < q; qy++) {
              sum[0] += B(qy, dy) * A1(0, qz, qy, dx);
              sum[0] += G(qy, dy) * A1(1, qz, qy, dx);
              sum[1] += B(qy, dy) * A1(2, qz, qy, dx);
            }
            A2(0, qz, dy, dx) = sum[0];
            A2(1, qz, dy, dx) = sum[1];
          }
        }
      }
      __syncthreads();

      for (int dz = threadIdx.z; dz < n; dz += blockDim.z) {
        for (int dy = threadIdx.y; dy < n; dy += blockDim.y) {
          for (int dx = threadIdx.x; dx < n; dx += blockDim.x) {
            double sum = 0.0;
            for (int qz = 0; qz < q; qz++) {
              sum += B(qz, dz) * A2(0, qz, dy, dx);
              sum += G(qz, dz) * A2(1, qz, dy, dx);
            }
            r_e(dx,dy,dz,i,e) += sum;
          }
        }
      }

    }

  }
#endif

};
/// @endcond
