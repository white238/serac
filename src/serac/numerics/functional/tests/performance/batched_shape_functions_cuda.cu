#include "mfem.hpp"

#include "mfem_PA_kernels_h1.hpp"
#include "mfem_PA_kernels_hcurl.hpp"

#include "axom/core/utilities/Timer.hpp"

#include "serac/infrastructure/accelerator.hpp"
#include "serac/numerics/functional/tensor.hpp"
#include "serac/numerics/functional/quadrature.hpp"
#include "serac/numerics/functional/finite_element.hpp"
#include "serac/numerics/functional/tuple_arithmetic.hpp"
#include "serac/numerics/functional/integral_utilities.hpp"

#include <type_traits>

namespace serac {

template <Geometry g, int Q>
struct GaussLegendreRule;

template <int Q>
struct GaussLegendreRule<Geometry::Quadrilateral, Q> {
  static constexpr auto points_1D  = GaussLegendreNodes<Q>();
  static constexpr auto weights_1D = GaussLegendreWeights<Q>();

  static constexpr double weight(int qx, int qy) { return weights_1D[qx] * weights_1D[qy]; }

  static constexpr int size() { return Q * Q; }
};

template <int Q>
struct GaussLegendreRule<Geometry::Hexahedron, Q> {
  static constexpr auto points_1D  = GaussLegendreNodes<Q>();
  static constexpr auto weights_1D = GaussLegendreWeights<Q>();

  static constexpr double weight(int qx, int qy, int qz) { return weights_1D[qx] * weights_1D[qy] * weights_1D[qz]; }

  static constexpr int size() { return Q * Q * Q; }
};

template <Geometry g, typename test, typename trial, int Q, typename lambda>
__global__ void reference_cuda_kernel(mfem::DeviceTensor< 2, const double > u, 
                                      mfem::DeviceTensor< 2, double > r, 
                                      mfem::DeviceTensor< 4, const double > J, 
                                      size_t num_elements, 
                                      lambda qf) {

  using test_element          = finite_element<g, test>;
  using trial_element         = finite_element<g, trial>;
  using element_residual_type = typename test_element::residual_type;
  static constexpr auto rule  = GaussQuadratureRule<g, Q>();
  static constexpr int  dim   = dimension_of(g);

  const int grid_stride = blockDim.x * gridDim.x;

  for (int qe = blockIdx.x * blockDim.x + threadIdx.x; qe < num_elements * rule.size(); qe += grid_stride) {

    int e = qe / rule.size();
    int q = qe % rule.size();

    auto u_elem = detail::Load<trial_element>(u, e);

    element_residual_type r_elem{};

    auto   xi  = rule.points[q];
    auto   dxi = rule.weights[q];
    auto   J_q = make_tensor<dim, dim>([&](int i, int j) { return J(q, i, j, e); });
    double dx  = det(J_q) * dxi;

    auto arg = domain_integral::Preprocess<trial_element>(u_elem, xi, J_q);

    auto qf_output = qf(arg);

    r_elem += domain_integral::Postprocess<test_element>(qf_output, xi, J_q) * dx;

    detail::Add(r, r_elem, e);

  }

}

template <int dim, int q>
void load_jacobian(const tensor< double, dim, dim, q, q > & J) {
  tensor< double, dim, dim > J_q;
  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      J_q[i][j] = J(j, i, threadIdx.z, threadIdx.y, threadIdx.x);
    }
  }
  return J;
}

template <int dim, int q>
void load_jacobian(const tensor< double, dim, dim, q, q, q > & J) {
  tensor< double, dim, dim > J_q;
  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      for (int k = 0; k < dim; k++) {
        J_q[i][j] = J(j, i, threadIdx.z, threadIdx.y, threadIdx.x);
      }
    }
  }
  return J;
}

template <typename dof_type>
__device__ void load(const dof_type& source, dof_type& destination)
{
  constexpr int ndof    = sizeof(dof_type) / sizeof(double);
  double*       src_ptr = reinterpret_cast<double*>(&element_values);
  for (int i = 0; i < ndof; i++) {
    element_values_ptr[i] = ptr[i];
  }
}

template <Geometry g, typename test, typename trial, int q, typename lambda>
void batched_cuda_kernel(const double * inputs,
                         double * outputs,
                         const double * jacobians,
                         TensorProductQuadratureRule<q> rule,
                         size_t num_elements, 
                         lambda material) {

  using test_element = finite_element<g, test>;
  using trial_element = finite_element<g, trial>;

  auto u = reinterpret_cast< const typename trial_element::dof_type * >(inputs);
  auto r = reinterpret_cast< typename test_element::dof_type * >(outputs);
  auto J = reinterpret_cast<const typename batched_jacobian<g, q>::type*>(jacobians);

  int e = blockIdx.x;

  // load the element values for this element
  __shared__ typename trial_element::dof_type u_elem;
  load(u[e], u_elem);

  // and load the jacobian for this thread's quadrature point
  auto J_q = load_jacobian(J[e]);

  // interpolate each quadrature point's value
  __shared__ typename trial_element::cache_type<q> trial_cache;
  auto stimulus = trial_element::interpolate(u_elem, J_q, rule, trial_cache);

  // evaluate the material response at each quadrature point
  auto response = material(stimulus);

  // integrate the material response against the test-space basis functions
  __shared__ typename test_element::cache_type<q> test_cache;
  test_element::integrate(sources, fluxes, J_q, rule, test_cache, r[e]);

}

}  // namespace serac

namespace compiler {
static void please_do_not_optimize_away([[maybe_unused]] void* p) { asm volatile("" : : "g"(p) : "memory"); }
}  // namespace compiler

template <typename lambda>
auto time(lambda&& f)
{
  axom::utilities::Timer stopwatch;
  stopwatch.start();
  f();
  stopwatch.stop();
  return stopwatch.elapsed();
}

template <int p, int q>
void h1_h1_test_2D(int num_elements, int num_runs)
{
  using serac::Geometry;
  using serac::H1;

  constexpr int n   = p + 1;
  constexpr int dim = 2;

  double rho = 1.0;
  double k   = 1.0;

  using test  = H1<p>;
  using trial = H1<p>;

  auto mass_plus_diffusion = [=](auto input) {
    auto [u, du_dx] = input;
    auto source     = rho * u;
    auto flux       = k * du_dx;
    return serac::tuple{source, flux};
  };

  std::default_random_engine             generator;
  std::uniform_real_distribution<double> distribution(-1.0, 1.0);

  mfem::Vector U1D(num_elements * n * n);
  mfem::Vector R1D(num_elements * n * n);
  mfem::Vector J1D(num_elements * dim * dim * q * q);
  mfem::Vector rho_dv_1D(num_elements * q * q);
  mfem::Vector k_invJ_invJT_dv_1D(num_elements * dim * dim * q * q);

  auto U               = mfem::Reshape(U1D.ReadWrite(), n, n, num_elements);
  auto J               = mfem::Reshape(J1D.ReadWrite(), q * q, dim, dim, num_elements);
  auto rho_dv          = mfem::Reshape(rho_dv_1D.ReadWrite(), q * q, num_elements);
  auto k_invJ_invJT_dv = mfem::Reshape(k_invJ_invJT_dv_1D.ReadWrite(), q * q, dim, dim, num_elements);

  serac::GaussLegendreRule<Geometry::Quadrilateral, q> rule;

  for (int e = 0; e < num_elements; e++) {
    for (int ix = 0; ix < n; ix++) {
      for (int iy = 0; iy < n; iy++) {
        U(iy, ix, e) = 0.1 * distribution(generator);
      }
    }

    for (int i = 0; i < q * q; i++) {
      serac::tensor<double, dim, dim> J_q{};

      for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
          J(i, r, c, e) = J_q[r][c] = (r == c) + 0.1 * distribution(generator);
        }
      }

      int qx = i % q;
      int qy = i / q;

      double qweight    = rule.weight(qx, qy);
      auto   invJ_invJT = dot(inv(J_q), transpose(inv(J_q)));
      double dv         = det(J_q) * qweight;

      rho_dv(i, e) = rho * dv;
      for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
          k_invJ_invJT_dv(i, r, c, e) = k * invJ_invJT[r][c] * dv;
        }
      }
    }
  }

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::reference_kernel<Geometry::Quadrilateral, test, trial, q>(U1D, R1D, J1D, num_elements,
                                                                                          mass_plus_diffusion);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average reference kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_reference = R1D;

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::cpu_batched_kernel<Geometry::Quadrilateral, test, trial, q>(
                             U1D.Read(), R1D.ReadWrite(), J1D.Read(), num_elements, mass_plus_diffusion);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average cpu batched kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_cpu_batched_kernel = R1D;
  auto error                     = answer_reference;
  error -= answer_cpu_batched_kernel;
  double relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;

#if 0
  {
    R1D                           = 0.0;
    bool                symmetric = false;
    mfem::Array<double> b_(n * q);
    mfem::Array<double> bt_(n * q);
    mfem::Array<double> g_(n * q);
    mfem::Array<double> gt_(n * q);
    auto                B  = mfem::Reshape(b_.ReadWrite(), q, n);
    auto                Bt = mfem::Reshape(bt_.ReadWrite(), n, q);

    auto G  = mfem::Reshape(g_.ReadWrite(), q, n);
    auto Gt = mfem::Reshape(gt_.ReadWrite(), n, q);

    for (int i = 0; i < q; i++) {
      auto value      = serac::GaussLobattoInterpolation<n>(rule.points_1D[i]);
      auto derivative = serac::GaussLobattoInterpolationDerivative<n>(rule.points_1D[i]);

      for (int j = 0; j < n; j++) {
        Bt(j, i) = B(i, j) = value[j];
        Gt(j, i) = G(i, j) = derivative[j];
      }
    }

    double mass_runtime = time([&]() {
                            for (int i = 0; i < num_runs; i++) {
                              mfem::SmemPAMassApply3D<n, q>(num_elements, b_, bt_, rho_dv_1D, U1D, R1D);
                              compiler::please_do_not_optimize_away(&R1D);
                            }
                          }) /
                          n;
    std::cout << "average mfem mass kernel time: " << mass_runtime / num_runs << std::endl;

    double diffusion_runtime =
        time([&]() {
          for (int i = 0; i < num_runs; i++) {
            mfem::SmemPADiffusionApply3D<n, q>(num_elements, symmetric = false, b_, g_, k_invJ_invJT_dv_1D, U1D, R1D);
            compiler::please_do_not_optimize_away(&R1D);
          }
        }) /
        n;
    std::cout << "average mfem diffusion kernel time: " << diffusion_runtime / num_runs << std::endl;

    std::cout << "average mfem combined kernel time: " << (mass_runtime + diffusion_runtime) / num_runs << std::endl;
  }
  auto answer_mfem = R1D;
  error            = answer_reference;
  error -= answer_mfem;
  relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;
#endif
}

template <int p, int q>
void h1_h1_test_3D(int num_elements, int num_runs)
{
  using serac::Geometry;
  using serac::H1;

  constexpr int n   = p + 1;
  constexpr int dim = 3;

  double rho = 1.0;
  double k   = 1.0;

  using test  = H1<p>;
  using trial = H1<p>;

  auto mass_plus_diffusion = [=](auto input) {
    auto [u, du_dx] = input;
    auto source     = rho * u;
    auto flux       = k * du_dx;
    return serac::tuple{source, flux};
  };

  std::default_random_engine             generator;
  std::uniform_real_distribution<double> distribution(-1.0, 1.0);

  mfem::Vector U1D(num_elements * n * n * n);
  mfem::Vector R1D(num_elements * n * n * n);
  mfem::Vector J1D(num_elements * dim * dim * q * q * q);
  mfem::Vector rho_dv_1D(num_elements * q * q * q);
  mfem::Vector k_invJ_invJT_dv_1D(num_elements * dim * dim * q * q * q);

  auto U               = mfem::Reshape(U1D.ReadWrite(), n, n, n, num_elements);
  auto J               = mfem::Reshape(J1D.ReadWrite(), q * q * q, dim, dim, num_elements);
  auto rho_dv          = mfem::Reshape(rho_dv_1D.ReadWrite(), q * q * q, num_elements);
  auto k_invJ_invJT_dv = mfem::Reshape(k_invJ_invJT_dv_1D.ReadWrite(), q * q * q, dim, dim, num_elements);

  serac::GaussLegendreRule<Geometry::Hexahedron, q> rule;

  for (int e = 0; e < num_elements; e++) {
    for (int ix = 0; ix < n; ix++) {
      for (int iy = 0; iy < n; iy++) {
        for (int iz = 0; iz < n; iz++) {
          U(iz, iy, ix, e) = 0.1 * distribution(generator);
        }
      }
    }

    for (int i = 0; i < q * q * q; i++) {
      serac::tensor<double, dim, dim> J_q{};

      for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
          J(i, r, c, e) = J_q[r][c] = (r == c) + 0.1 * distribution(generator);
        }
      }

      int qx = i % q;
      int qy = (i % (q * q)) / q;
      int qz = i / (q * q);

      double qweight    = rule.weight(qx, qy, qz);
      auto   invJ_invJT = dot(inv(J_q), transpose(inv(J_q)));
      double dv         = det(J_q) * qweight;

      rho_dv(i, e) = rho * dv;
      for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
          k_invJ_invJT_dv(i, r, c, e) = k * invJ_invJT[r][c] * dv;
        }
      }
    }
  }

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::reference_kernel<Geometry::Hexahedron, test, trial, q>(U1D, R1D, J1D, num_elements,
                                                                                       mass_plus_diffusion);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average reference kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_reference = R1D;

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::cpu_batched_kernel<Geometry::Hexahedron, test, trial, q>(
                             U1D.Read(), R1D.ReadWrite(), J1D.Read(), num_elements, mass_plus_diffusion);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average cpu batched kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_cpu_batched_kernel = R1D;
  auto error                     = answer_reference;
  error -= answer_cpu_batched_kernel;
  double relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;

  {
    R1D                           = 0.0;
    bool                symmetric = false;
    mfem::Array<double> b_(n * q);
    mfem::Array<double> bt_(n * q);
    mfem::Array<double> g_(n * q);
    mfem::Array<double> gt_(n * q);
    auto                B  = mfem::Reshape(b_.ReadWrite(), q, n);
    auto                Bt = mfem::Reshape(bt_.ReadWrite(), n, q);

    auto G  = mfem::Reshape(g_.ReadWrite(), q, n);
    auto Gt = mfem::Reshape(gt_.ReadWrite(), n, q);

    for (int i = 0; i < q; i++) {
      auto value      = serac::GaussLobattoInterpolation<n>(rule.points_1D[i]);
      auto derivative = serac::GaussLobattoInterpolationDerivative<n>(rule.points_1D[i]);

      for (int j = 0; j < n; j++) {
        Bt(j, i) = B(i, j) = value[j];
        Gt(j, i) = G(i, j) = derivative[j];
      }
    }

    double mass_runtime = time([&]() {
                            for (int i = 0; i < num_runs; i++) {
                              mfem::SmemPAMassApply3D<n, q>(num_elements, b_, bt_, rho_dv_1D, U1D, R1D);
                              compiler::please_do_not_optimize_away(&R1D);
                            }
                          }) /
                          n;
    std::cout << "average mfem mass kernel time: " << mass_runtime / num_runs << std::endl;

    double diffusion_runtime =
        time([&]() {
          for (int i = 0; i < num_runs; i++) {
            mfem::SmemPADiffusionApply3D<n, q>(num_elements, symmetric = false, b_, g_, k_invJ_invJT_dv_1D, U1D, R1D);
            compiler::please_do_not_optimize_away(&R1D);
          }
        }) /
        n;
    std::cout << "average mfem diffusion kernel time: " << diffusion_runtime / num_runs << std::endl;

    std::cout << "average mfem combined kernel time: " << (mass_runtime + diffusion_runtime) / num_runs << std::endl;
  }
  auto answer_mfem = R1D;
  error            = answer_reference;
  error -= answer_mfem;
  relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;
}

template <int p, int q>
void hcurl_hcurl_test_2D(int num_elements, int num_runs)
{
  using serac::Geometry;
  using serac::Hcurl;

  constexpr int n   = p + 1;
  constexpr int dim = 2;

  double rho = 1.0;
  double k   = 1.0;

  using test  = Hcurl<p>;
  using trial = Hcurl<p>;

  using trial_element = serac::finite_element<Geometry::Quadrilateral, trial>;
  using test_element  = serac::finite_element<Geometry::Quadrilateral, test>;

  auto mass_plus_curlcurl = [=](auto input) {
    auto [u, curl_u] = input;
    auto source      = rho * u;
    auto flux        = k * curl_u;
    return serac::tuple{source, flux};
  };

  std::default_random_engine             generator;
  std::uniform_real_distribution<double> distribution(-1.0, 1.0);

  mfem::Vector U1D(num_elements * trial_element::ndof);
  mfem::Vector R1D(num_elements * test_element::ndof);
  mfem::Vector J1D(num_elements * dim * dim * q * q);
  // mfem::Vector rho_invJ_invJT_dv_1D(num_elements * dim * dim * q * q);
  // mfem::Vector k_JTJ_dv_over_detJsq_1D(num_elements * dim * dim * q * q);

  auto U = mfem::Reshape(U1D.ReadWrite(), trial_element::ndof, num_elements);
  auto J = mfem::Reshape(J1D.ReadWrite(), q * q, dim, dim, num_elements);
  // auto rho_invJ_invJT_dv    = mfem::Reshape(rho_invJ_invJT_dv_1D.ReadWrite(), q * q, dim, dim, num_elements);
  // auto k_JTJ_dv_over_detJsq = mfem::Reshape(k_JTJ_dv_over_detJsq_1D.ReadWrite(), q * q, dim, dim, num_elements);

  // serac::GaussLegendreRule<Geometry::Hexahedron, q> rule;

  for (int e = 0; e < num_elements; e++) {
    for (int i = 0; i < trial_element::ndof; i++) {
      U(i, e) = 0.1 * distribution(generator);
    }

    for (int i = 0; i < q * q; i++) {
      serac::tensor<double, dim, dim> J_q{};

      for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
          J(i, r, c, e) = J_q[r][c] = (r == c) + 0.1 * distribution(generator);
        }
      }

      /*
            int qx = i % q;
            int qy = i / q;

            double qweight    = rule.weight(qx, qy, qz);
            auto   JTJ        = dot(transpose(J_q), J_q);
            auto   invJ_invJT = dot(inv(J_q), transpose(inv(J_q)));
            auto   detJ       = det(J_q);
            double dv         = det(J_q) * qweight;

            for (int r = 0; r < dim; r++) {
              for (int c = 0; c < dim; c++) {
                k_JTJ_dv_over_detJsq(i, r, c, e) = k * (JTJ[r][c] / (detJ * detJ)) * dv;
                rho_invJ_invJT_dv(i, r, c, e)    = rho * invJ_invJT[r][c] * dv;
              }
            }
      */
    }
  }

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::reference_kernel<Geometry::Quadrilateral, test, trial, q>(U1D, R1D, J1D, num_elements,
                                                                                          mass_plus_curlcurl);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average reference kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_reference = R1D;

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::cpu_batched_kernel<Geometry::Quadrilateral, test, trial, q>(
                             U1D.Read(), R1D.ReadWrite(), J1D.Read(), num_elements, mass_plus_curlcurl);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average cpu batched kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_cpu_batched_kernel = R1D;
  auto error                     = answer_reference;
  error -= answer_cpu_batched_kernel;
  double relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;

#if 0
  {
    R1D            = 0.0;
    bool symmetric = false;

    // I think the "o" and "c" are supposed to be short for
    // "open" and "closed", referring to placing interpolation
    // nodes at the gauss-legendre, and gauss-lobatto points (respectively)
    mfem::Array<double> bo_((n - 1) * q);
    mfem::Array<double> bc_(n * q);
    mfem::Array<double> bot_((n - 1) * q);
    mfem::Array<double> bct_(n * q);
    mfem::Array<double> gc_(n * q);
    mfem::Array<double> gct_(n * q);

    auto Bo  = mfem::Reshape(bo_.ReadWrite(), q, n - 1);
    auto Bc  = mfem::Reshape(bc_.ReadWrite(), q, n);
    auto Bot = mfem::Reshape(bot_.ReadWrite(), n - 1, q);
    auto Bct = mfem::Reshape(bct_.ReadWrite(), n, q);
    auto Gc  = mfem::Reshape(gc_.ReadWrite(), q, n);
    auto Gct = mfem::Reshape(gct_.ReadWrite(), n, q);

    for (int i = 0; i < q; i++) {
      auto lobatto_value      = serac::GaussLobattoInterpolation<n>(rule.points_1D[i]);
      auto lobatto_derivative = serac::GaussLobattoInterpolationDerivative<n>(rule.points_1D[i]);

      for (int j = 0; j < n; j++) {
        Bct(j, i) = Bc(i, j) = lobatto_value[j];
        Gct(j, i) = Gc(i, j) = lobatto_derivative[j];
      }

      auto legendre_value = serac::GaussLegendreInterpolation<n - 1>(rule.points_1D[i]);
      for (int j = 0; j < n - 1; j++) {
        Bot(j, i) = Bo(i, j) = legendre_value[j];
      }
    }

    double mass_runtime = time([&]() {
                            for (int i = 0; i < num_runs; i++) {
                              mfem::PAHcurlMassApply3D(n, q, num_elements, symmetric = false, bo_, bc_, bot_, bct_,
                                                       rho_invJ_invJT_dv_1D, U1D, R1D);
                              compiler::please_do_not_optimize_away(&R1D);
                            }
                          }) /
                          n;
    std::cout << "average mfem mass kernel time: " << mass_runtime / num_runs << std::endl;

    double curlcurl_runtime = time([&]() {
                                 for (int i = 0; i < num_runs; i++) {
                                   mfem::PACurlCurlApply3D<n, q>(n, q, symmetric = false, num_elements, bo_, bc_, bot_,
                                                                 bct_, gc_, gct_, k_JTJ_dv_over_detJsq_1D, U1D, R1D);
                                   compiler::please_do_not_optimize_away(&R1D);
                                 }
                               }) /
                               n;
    std::cout << "average mfem curlcurl kernel time: " << curlcurl_runtime / num_runs << std::endl;

    std::cout << "average mfem combined kernel time: " << (mass_runtime + curlcurl_runtime) / num_runs << std::endl;
  }
  auto answer_mfem = R1D;
  error            = answer_reference;
  error -= answer_mfem;
  relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;
#endif
}

template <int p, int q>
void hcurl_hcurl_test_3D(int num_elements, int num_runs)
{
  using serac::Geometry;
  using serac::Hcurl;

  constexpr int n   = p + 1;
  constexpr int dim = 3;

  double rho = 1.0;
  double k   = 1.0;

  using test  = Hcurl<p>;
  using trial = Hcurl<p>;

  using trial_element = serac::finite_element<Geometry::Hexahedron, trial>;
  using test_element  = serac::finite_element<Geometry::Hexahedron, test>;

  auto mass_plus_curlcurl = [=](auto input) {
    auto [u, curl_u] = input;
    auto source      = rho * u;
    auto flux        = k * curl_u;
    return serac::tuple{source, flux};
  };

  std::default_random_engine             generator;
  std::uniform_real_distribution<double> distribution(-1.0, 1.0);

  mfem::Vector U1D(num_elements * trial_element::ndof);
  mfem::Vector R1D(num_elements * test_element::ndof);
  mfem::Vector J1D(num_elements * dim * dim * q * q * q);
  mfem::Vector rho_invJ_invJT_dv_1D(num_elements * dim * dim * q * q * q);
  mfem::Vector k_JTJ_dv_over_detJsq_1D(num_elements * dim * dim * q * q * q);

  auto U                    = mfem::Reshape(U1D.ReadWrite(), trial_element::ndof, num_elements);
  auto J                    = mfem::Reshape(J1D.ReadWrite(), q * q * q, dim, dim, num_elements);
  auto rho_invJ_invJT_dv    = mfem::Reshape(rho_invJ_invJT_dv_1D.ReadWrite(), q * q * q, dim, dim, num_elements);
  auto k_JTJ_dv_over_detJsq = mfem::Reshape(k_JTJ_dv_over_detJsq_1D.ReadWrite(), q * q * q, dim, dim, num_elements);

  serac::GaussLegendreRule<Geometry::Hexahedron, q> rule;

  for (int e = 0; e < num_elements; e++) {
    for (int i = 0; i < trial_element::ndof; i++) {
      U(i, e) = 0.1 * distribution(generator);
    }

    for (int i = 0; i < q * q * q; i++) {
      serac::tensor<double, dim, dim> J_q{};

      for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
          J(i, r, c, e) = J_q[r][c] = (r == c) + 0.1 * distribution(generator);
        }
      }

      int qx = i % q;
      int qy = (i % (q * q)) / q;
      int qz = i / (q * q);

      double qweight    = rule.weight(qx, qy, qz);
      auto   JTJ        = dot(transpose(J_q), J_q);
      auto   invJ_invJT = dot(inv(J_q), transpose(inv(J_q)));
      auto   detJ       = det(J_q);
      double dv         = det(J_q) * qweight;

      for (int r = 0; r < dim; r++) {
        for (int c = 0; c < dim; c++) {
          k_JTJ_dv_over_detJsq(i, r, c, e) = k * (JTJ[r][c] / (detJ * detJ)) * dv;
          rho_invJ_invJT_dv(i, r, c, e)    = rho * invJ_invJT[r][c] * dv;
        }
      }
    }
  }

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::reference_kernel<Geometry::Hexahedron, test, trial, q>(U1D, R1D, J1D, num_elements,
                                                                                       mass_plus_curlcurl);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average reference kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_reference = R1D;

  {
    R1D            = 0.0;
    double runtime = time([&]() {
                       for (int i = 0; i < num_runs; i++) {
                         serac::cpu_batched_kernel<Geometry::Hexahedron, test, trial, q>(
                             U1D.Read(), R1D.ReadWrite(), J1D.Read(), num_elements, mass_plus_curlcurl);
                         compiler::please_do_not_optimize_away(&R1D);
                       }
                     }) /
                     n;
    std::cout << "average cpu batched kernel time: " << runtime / num_runs << std::endl;
  }
  auto answer_cpu_batched_kernel = R1D;
  auto error                     = answer_reference;
  error -= answer_cpu_batched_kernel;
  double relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;

  {
    R1D            = 0.0;
    bool symmetric = false;

    // I think the "o" and "c" are supposed to be short for
    // "open" and "closed", referring to placing interpolation
    // nodes at the gauss-legendre, and gauss-lobatto points (respectively)
    mfem::Array<double> bo_((n - 1) * q);
    mfem::Array<double> bc_(n * q);
    mfem::Array<double> bot_((n - 1) * q);
    mfem::Array<double> bct_(n * q);
    mfem::Array<double> gc_(n * q);
    mfem::Array<double> gct_(n * q);

    auto Bo  = mfem::Reshape(bo_.ReadWrite(), q, n - 1);
    auto Bc  = mfem::Reshape(bc_.ReadWrite(), q, n);
    auto Bot = mfem::Reshape(bot_.ReadWrite(), n - 1, q);
    auto Bct = mfem::Reshape(bct_.ReadWrite(), n, q);
    auto Gc  = mfem::Reshape(gc_.ReadWrite(), q, n);
    auto Gct = mfem::Reshape(gct_.ReadWrite(), n, q);

    for (int i = 0; i < q; i++) {
      auto lobatto_value      = serac::GaussLobattoInterpolation<n>(rule.points_1D[i]);
      auto lobatto_derivative = serac::GaussLobattoInterpolationDerivative<n>(rule.points_1D[i]);

      for (int j = 0; j < n; j++) {
        Bct(j, i) = Bc(i, j) = lobatto_value[j];
        Gct(j, i) = Gc(i, j) = lobatto_derivative[j];
      }

      auto legendre_value = serac::GaussLegendreInterpolation<n - 1>(rule.points_1D[i]);
      for (int j = 0; j < n - 1; j++) {
        Bot(j, i) = Bo(i, j) = legendre_value[j];
      }
    }

    double mass_runtime = time([&]() {
                            for (int i = 0; i < num_runs; i++) {
                              mfem::PAHcurlMassApply3D(n, q, num_elements, symmetric = false, bo_, bc_, bot_, bct_,
                                                       rho_invJ_invJT_dv_1D, U1D, R1D);
                              compiler::please_do_not_optimize_away(&R1D);
                            }
                          }) /
                          n;
    std::cout << "average mfem mass kernel time: " << mass_runtime / num_runs << std::endl;

    double curlcurl_runtime = time([&]() {
                                for (int i = 0; i < num_runs; i++) {
                                  mfem::PACurlCurlApply3D<n, q>(n, q, symmetric = false, num_elements, bo_, bc_, bot_,
                                                                bct_, gc_, gct_, k_JTJ_dv_over_detJsq_1D, U1D, R1D);
                                  compiler::please_do_not_optimize_away(&R1D);
                                }
                              }) /
                              n;
    std::cout << "average mfem curlcurl kernel time: " << curlcurl_runtime / num_runs << std::endl;

    std::cout << "average mfem combined kernel time: " << (mass_runtime + curlcurl_runtime) / num_runs << std::endl;
  }
  auto answer_mfem = R1D;
  error            = answer_reference;
  error -= answer_mfem;
  relative_error = error.Norml2() / answer_reference.Norml2();
  std::cout << "error: " << relative_error << std::endl;
}

int main()
{
  int num_runs     = 10;
  int num_elements = 1000;
  h1_h1_test_2D<2 /* polynomial order */, 3 /* quadrature points / dim */>(num_elements, num_runs);
  h1_h1_test_3D<2 /* polynomial order */, 3 /* quadrature points / dim */>(num_elements, num_runs);
  hcurl_hcurl_test_2D<2 /* polynomial order */, 3 /* quadrature points / dim */>(num_elements, num_runs);
  hcurl_hcurl_test_3D<2 /* polynomial order */, 3 /* quadrature points / dim */>(num_elements, num_runs);
}
