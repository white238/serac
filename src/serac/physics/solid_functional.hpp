// Copyright (c) 2019-2022, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

/**
 * @file solid_functional.hpp
 *
 * @brief An object containing the solver for total Lagrangian finite deformation solid mechanics
 */

#pragma once

#include "mfem.hpp"

#include "serac/physics/common.hpp"
#include "serac/physics/base_physics.hpp"
#include "serac/numerics/odes.hpp"
#include "serac/numerics/stdfunction_operator.hpp"
#include "serac/numerics/functional/functional.hpp"
#include "serac/physics/state/state_manager.hpp"
#include "serac/physics/solid.hpp"
#include "serac/physics/materials/functional_material_utils.hpp"

namespace serac {

namespace solid_mechanics {

/**
 * @brief default method and tolerances for solving the
 * systems of linear equations that show up in implicit
 * solid mechanics simulations
 */
const IterativeSolverOptions default_linear_options = {.rel_tol     = 1.0e-6,
                                                       .abs_tol     = 1.0e-10,
                                                       .print_level = 0,
                                                       .max_iter    = 500,
                                                       .lin_solver  = LinearSolver::GMRES,
                                                       .prec        = HypreBoomerAMGPrec{}};

/**
 * @brief default iteration limits, tolerances and verbosity for solving the
 * systems of nonlinear equations that show up in implicit
 * solid mechanics simulations
 */
const NonlinearSolverOptions default_nonlinear_options = {
    .rel_tol = 1.0e-4, .abs_tol = 1.0e-8, .max_iter = 10, .print_level = 1};

/// the default linear and nonlinear solver options for (quasi-)static analyses
const SolverOptions default_static_options = {default_linear_options, default_nonlinear_options};

/// the default solver and time integration options for dynamic analyses
const SolverOptions default_dynamic_options = {
    default_linear_options, default_nonlinear_options,
    TimesteppingOptions{TimestepMethod::AverageAcceleration, DirichletEnforcementMethod::RateControl}};

}  // namespace solid_mechanics

/**
 * @brief The nonlinear solid solver class
 *
 * The nonlinear total Lagrangian quasi-static and dynamic
 * hyperelastic solver object. This uses Functional to compute the tangent
 * stiffness matrices.
 *
 * @tparam order The order of the discretization of the displacement and velocity fields
 * @tparam dim The spatial dimension of the mesh
 */
template <int order, int dim, typename parameters = Parameters<>,
          typename parameter_indices = std::make_integer_sequence<int, parameters::n>>
class SolidFunctional;

/// @overload
template <int order, int dim, typename... parameter_space, int... parameter_indices>
class SolidFunctional<order, dim, Parameters<parameter_space...>, std::integer_sequence<int, parameter_indices...>>
    : public BasePhysics {
public:
  //! @cond Doxygen_Suppress
  static constexpr int  VALUE = 0, DERIVATIVE = 1;
  static constexpr auto I = Identity<dim>();
  //! @endcond

  /**
   * @brief a list of the currently supported element geometries, by dimension
   * @note: this is hardcoded for now, since we currently
   * only support tensor product elements (1 element type per spatial dimension)
   */
  static constexpr Geometry geom = supported_geometries[dim];

  /**
   * @brief Construct a new Solid Functional object
   *
   * @param options The options for the linear, nonlinear, and ODE solves
   * @param geom_nonlin Flag to include geometric nonlinearities
   * @param keep_deformation Flag to keep the deformation in the underlying mesh post-destruction
   * @param name An optional name for the physics module instance
   */
  SolidFunctional(
      const SolverOptions& options, GeometricNonlinearities geom_nonlin = GeometricNonlinearities::On,
      FinalMeshOption keep_deformation = FinalMeshOption::Deformed, const std::string& name = "")
      : BasePhysics(2, order, name),
        velocity_(StateManager::newState(FiniteElementState::Options{
            .order = order, .vector_dim = mesh_.Dimension(), .name = detail::addPrefix(name, "velocity")})),
        displacement_(StateManager::newState(FiniteElementState::Options{
            .order = order, .vector_dim = mesh_.Dimension(), .name = detail::addPrefix(name, "displacement")})),
        adjoint_displacement_(StateManager::newState(FiniteElementState::Options{
            .order = order, .vector_dim = mesh_.Dimension(), .name = detail::addPrefix(name, "adjoint_displacement")})),
        nodal_forces_(mesh_, displacement_.space(), "nodal_forces"),
        ode2_(displacement_.space().TrueVSize(), {.c0 = c0_, .c1 = c1_, .u = u_, .du_dt = du_dt_, .d2u_dt2 = previous_},
              nonlin_solver_, bcs_),
        c0_(0.0),
        c1_(0.0),
        geom_nonlin_(geom_nonlin),
        keep_deformation_(keep_deformation)
  {
    SLIC_ERROR_ROOT_IF(mesh_.Dimension() != dim,
                       axom::fmt::format("Compile time dimension and runtime mesh dimension mismatch"));

    state_.push_back(velocity_);
    state_.push_back(displacement_);
    state_.push_back(adjoint_displacement_);

    // Create a pack of the primal field and parameter finite element spaces
    mfem::ParFiniteElementSpace*                                             test_space = &displacement_.space();
    std::array<mfem::ParFiniteElementSpace*, sizeof...(parameter_space) + 2> trial_spaces;
    trial_spaces[0] = &displacement_.space();
    trial_spaces[1] = &displacement_.space();

    if constexpr (sizeof...(parameter_space) > 0) {
      tuple<parameter_space...> types{};
      for_constexpr<sizeof...(parameter_space)>([&](auto i) {
        trial_spaces[i+2] =
            generateParFiniteElementSpace<typename std::remove_reference<decltype(get<i>(types))>::type>(&mesh_);
        parameter_sensitivities_[i] = std::make_unique<FiniteElementDual>(mesh_, *trial_spaces[i + 2]);
      });
    }

    residual_ = std::make_unique<Functional<test(trial, trial, parameter_space...)>>(test_space, trial_spaces);

    state_.push_back(velocity_);
    state_.push_back(displacement_);

    // Initialize the mesh node pointers
    reference_nodes_ = std::make_unique<mfem::ParGridFunction>(&displacement_.space());
    mesh_.EnsureNodes();
    mesh_.GetNodes(*reference_nodes_);

    deformed_nodes_ = std::make_unique<mfem::ParGridFunction>(*reference_nodes_);

    displacement_ = 0.0;
    velocity_     = 0.0;

    const auto& lin_options = options.linear;
    // If the user wants the AMG preconditioner with a linear solver, set the pfes
    // to be the displacement
    const auto& augmented_options = mfem_ext::AugmentAMGForElasticity(lin_options, displacement_.space());

    nonlin_solver_ = mfem_ext::EquationSolver(mesh_.GetComm(), augmented_options, options.nonlinear);

    // Check for dynamic mode
    if (options.dynamic) {
      ode2_.SetTimestepper(options.dynamic->timestepper);
      ode2_.SetEnforcementMethod(options.dynamic->enforcement_method);
      is_quasistatic_ = false;
    } else {
      is_quasistatic_ = true;
    }

    int true_size = velocity_.space().TrueVSize();

    u_.SetSize(true_size);
    du_dt_.SetSize(true_size);
    previous_.SetSize(true_size);
    previous_ = 0.0;

    du_.SetSize(true_size);
    du_ = 0.0;

    dr_.SetSize(true_size);
    dr_ = 0.0;

    predicted_displacement_.SetSize(true_size);
    predicted_displacement_ = 0.0;

    zero_.SetSize(true_size);
    zero_ = 0.0;
  }

  /// @brief Destroy the Solid Functional object
  ~SolidFunctional()
  {
    // Update the mesh with the new deformed nodes if requested
    if (keep_deformation_ == FinalMeshOption::Deformed) {
      *reference_nodes_ += displacement_.gridFunction();
    }

    // Build a new grid function to store the mesh nodes post-destruction
    // NOTE: MFEM will manage the memory of these objects

    auto mesh_fe_coll  = new mfem::H1_FECollection(order_, mesh_.Dimension());
    auto mesh_fe_space = new mfem::ParFiniteElementSpace(displacement_.space(), &mesh_, mesh_fe_coll);
    auto mesh_nodes    = new mfem::ParGridFunction(mesh_fe_space);
    mesh_nodes->MakeOwner(mesh_fe_coll);

    *mesh_nodes = *reference_nodes_;

    // Set the mesh to the newly created nodes object and pass ownership
    mesh_.NewNodes(*mesh_nodes, true);
  }

  void setParameter(const FiniteElementState& parameter_state, size_t i)
  {
    parameter_states_[i] = &parameter_state;
  }

  /**
   * @brief Create a shared ptr to a quadrature data buffer for the given material type
   *
   * @tparam T the type to be created at each quadrature point
   * @param initial_state the value to be broadcast to each quadrature point
   * @return std::shared_ptr< QuadratureData<T> >
   */
  template <typename T>
  std::shared_ptr<QuadratureData<T>> createQuadratureDataBuffer(T initial_state)
  {
    constexpr auto Q = order + 1;

    size_t num_elements        = size_t(mesh_.GetNE());
    size_t qpoints_per_element = GaussQuadratureRule<geom, Q>().size();

    auto  qdata     = std::make_shared<QuadratureData<T>>(num_elements, qpoints_per_element);
    auto& container = *qdata;
    for (size_t e = 0; e < num_elements; e++) {
      for (size_t q = 0; q < qpoints_per_element; q++) {
        container(e, q) = initial_state;
      }
    }

    return qdata;
  }

  /**
   * @brief Set essential displacement boundary conditions (strongly enforced)
   *
   * @param[in] disp_bdr The boundary attributes on which to enforce a displacement
   * @param[in] disp The prescribed boundary displacement function
   */
  void setDisplacementBCs(const std::set<int>&                                           disp_bdr,
                          std::function<void(const mfem::Vector& x, mfem::Vector& disp)> disp)
  {
    // Project the coefficient onto the grid function
    disp_bdr_coef_ = std::make_shared<mfem::VectorFunctionCoefficient>(dim, disp);

    bcs_.addEssential(disp_bdr, disp_bdr_coef_, displacement_.space());
  }

  /**
   * @brief Set essential displacement boundary conditions (strongly enforced)
   *
   * @param[in] disp_bdr The boundary attributes on which to enforce a displacement
   * @param[in] disp The time-dependent prescribed boundary displacement function
   */
  void setDisplacementBCs(const std::set<int>&                                            disp_bdr,
                          std::function<void(const mfem::Vector&, double, mfem::Vector&)> disp)
  {
    // Project the coefficient onto the grid function
    disp_bdr_coef_ = std::make_shared<mfem::VectorFunctionCoefficient>(dim, disp);

    bcs_.addEssential(disp_bdr, disp_bdr_coef_, displacement_.space());
  }

  /**
   * @brief Set the displacement essential boundary conditions on a single component
   *
   * @param[in] disp_bdr The set of boundary attributes to set the displacement on
   * @param[in] disp The vector function containing the set displacement values
   * @param[in] component The component to set the displacment on
   */
  void setDisplacementBCs(const std::set<int>& disp_bdr, std::function<double(const mfem::Vector& x)> disp,
                          int component)
  {
    // Project the coefficient onto the grid function
    component_disp_bdr_coef_ = std::make_shared<mfem::FunctionCoefficient>(disp);

    bcs_.addEssential(disp_bdr, component_disp_bdr_coef_, displacement_.space(), component);
  }

  /**
   * @brief Set the material stress response and mass properties for the physics module
   *
   * @tparam MaterialType The solid material type
   * @tparam StateType the type that contains the internal variables for MaterialType
   * @param material A material that provides a function to evaluate stress
   * @param qdata the buffer of material internal variables at each quadrature point
   *
   * @pre MaterialType must have a public member variable `density`
   * @pre MaterialType must define operator() that returns the Kirchoff stress
   */
  template <typename MaterialType, typename StateType>
  void setMaterial(MaterialType material, std::shared_ptr<QuadratureData<StateType>> qdata)
  {
    residual_->AddDomainIntegral(
        Dimension<dim>{},
        [this, material](auto /*x*/, auto& state, auto displacement, auto acceleration, auto... params) {
          auto a          = get<VALUE>(acceleration);
          auto du_dX      = get<DERIVATIVE>(displacement);
          auto body_force = material.density * a + 0.0 * du_dX[0];
          auto stress     = material(state, du_dX, params...);

          if (geom_nonlin_ == GeometricNonlinearities::On) {
            auto F     = I + du_dX;
            body_force = body_force * det(F);
            stress     = dot(stress, inv(transpose(F)));
          }

          return serac::tuple{body_force, stress};
        },
        mesh_, qdata);
  }

  /// @overload
  template <typename MaterialType>
  void setMaterial(MaterialType material)
  {
    setMaterial(material, EmptyQData);
  }

  /**
   * @brief Set the underlying finite element state to a prescribed displacement
   *
   * @param disp The function describing the displacement field
   */
  void setDisplacement(std::function<void(const mfem::Vector& x, mfem::Vector& disp)> disp)
  {
    // Project the coefficient onto the grid function
    mfem::VectorFunctionCoefficient disp_coef(dim, disp);
    displacement_.project(disp_coef);
    gf_initialized_[1] = true;
  }

  /**
   * @brief Set the underlying finite element state to a prescribed velocity
   *
   * @param vel The function describing the velocity field
   */
  void setVelocity(std::function<void(const mfem::Vector& x, mfem::Vector& vel)> vel)
  {
    // Project the coefficient onto the grid function
    mfem::VectorFunctionCoefficient vel_coef(dim, vel);
    velocity_.project(vel_coef);
    gf_initialized_[0] = true;
  }

  /**
   * @brief Set the body forcefunction
   *
   * @tparam BodyForceType The type of the body force load
   * @param body_force A source function for a prescribed body load
   *
   * @pre BodyForceType must have the operator (x, time, displacement, d displacement_dx) defined as the body force
   */
  template <typename BodyForceType>
  void addBodyForce(BodyForceType body_force)
  {
    residual_->AddDomainIntegral(
        Dimension<dim>{},
        [body_force, this](auto x, auto displacement, auto /* acceleration */, auto... /*params*/) {
          auto du_dX    = get<DERIVATIVE>(displacement);
          auto one_dual = (1.0 + 0.0 * du_dX[0][0]);
          auto source   = body_force(x, time_) * one_dual;
          if (geom_nonlin_ == GeometricNonlinearities::On) {
            source = source * det(I + du_dX);
          }
          return serac::tuple{source, zero{}};
        },
        mesh_);
  }

  /**
   * @brief Set the traction boundary condition
   *
   * @tparam TractionType The type of the traction load
   * @param traction_function A function describing the traction applied to a boundary
   *
   * @pre TractionType must have the operator (x, normal, time) to return the thermal flux value
   */
  template <typename TractionType>
  void setPiolaTraction(TractionType traction_function)
  {
    residual_->AddBoundaryIntegral(
        Dimension<dim - 1>{},
        [this, traction_function](auto x, auto n, auto, auto, auto... params) {
          return -1.0 * traction_function(x, n, time_, params...);
        },
        mesh_);
  }

  /// @brief Build the quasi-static operator corresponding to the total Lagrangian formulation
  std::unique_ptr<mfem_ext::StdFunctionOperator> buildQuasistaticOperator()
  {
    // the quasistatic case is entirely described by the residual,
    // there is no ordinary differential equation
    return std::make_unique<mfem_ext::StdFunctionOperator>(
        displacement_.space().TrueVSize(),

        // residual function
        [this](const mfem::Vector& u, mfem::Vector& r) {
          r = (*residual_)(u, zero_, *parameter_states_[parameter_indices]...);
          r.SetSubVector(bcs_.allEssentialTrueDofs(), 0.0);
        },

        // gradient of residual function
        [this](const mfem::Vector& u) -> mfem::Operator& {
          auto [r, drdu] = (*residual_)(differentiate_wrt(u), zero_, *parameter_states_[parameter_indices]...);
          J_             = assemble(drdu);
          bcs_.eliminateAllEssentialDofsFromMatrix(*J_);
          return *J_;
        });
  }

  /**
   * @brief Complete the initialization and allocation of the data structures.
   *
   * @note This must be called before AdvanceTimestep().
   */
  void completeSetup() override
  {
    if constexpr (sizeof ... (parameter_space) > 0) {
      for (size_t i = 0; i < sizeof ... (parameter_space); i++) {
        SLIC_ERROR_ROOT_IF(!parameter_states_[i], "all parameters fields must be initialized before calling completeSetup()");
      }
    }

    // Build the dof array lookup tables
    displacement_.space().BuildDofToArrays();

    if (is_quasistatic_) {
      residual_with_bcs_ = buildQuasistaticOperator();

      // the residual calculation uses the old stiffness matrix
      // to help apply essential boundary conditions, so we
      // compute J here to prime the pump for the first solve
      residual_with_bcs_->GetGradient(displacement_);

    } else {
      // the dynamic case is described by a residual function and a second order
      // ordinary differential equation. Here, we define the residual function in
      // terms of an acceleration.
      residual_with_bcs_ = std::make_unique<mfem_ext::StdFunctionOperator>(
          displacement_.space().TrueVSize(),

          [this](const mfem::Vector& d2u_dt2, mfem::Vector& r) {
            add(1.0, u_, c0_, d2u_dt2, predicted_displacement_);
            r = (*residual_)(predicted_displacement_, d2u_dt2, *parameter_states_[parameter_indices]...);
            r.SetSubVector(bcs_.allEssentialTrueDofs(), 0.0);
          },

          [this](const mfem::Vector& d2u_dt2) -> mfem::Operator& {
            add(1.0, u_, c0_, d2u_dt2, predicted_displacement_);

            // K := dR/du
            auto K = serac::get<DERIVATIVE>((*residual_)(differentiate_wrt(predicted_displacement_), d2u_dt2,
                                                *parameter_states_[parameter_indices]...));
            std::unique_ptr<mfem::HypreParMatrix> k_mat(assemble(K));

            // M := dR/da
            auto M = serac::get<DERIVATIVE>((*residual_)(predicted_displacement_, differentiate_wrt(d2u_dt2),
                                                *parameter_states_[parameter_indices]...));
            std::unique_ptr<mfem::HypreParMatrix> m_mat(assemble(M));

            // J = M + c0 * K
            J_.reset(mfem::Add(1.0, *m_mat, c0_, *k_mat));
            bcs_.eliminateAllEssentialDofsFromMatrix(*J_);

            return *J_;
          });
    }

    nonlin_solver_.SetOperator(*residual_with_bcs_);
  }

  /// @brief Solve the Quasi-static Newton system
  void quasiStaticSolve(double dt)
  {
    time_ += dt;

    // the 30 lines of code below are essentially equivalent to the 1-liner
    // u += dot(inv(J), dot(J_elim[:, dofs], (U(t + dt) - u)[dofs]));
    {
      for (auto& bc : bcs_.essentials()) {
        bc.setDofs(du_, time_);
      }

      auto& constrained_dofs = bcs_.allEssentialTrueDofs();
      for (int i = 0; i < constrained_dofs.Size(); i++) {
        du_[constrained_dofs[i]] -= displacement_(constrained_dofs[i]);
      }

      dr_ = 0.0;
      for (const auto& bc : bcs_.essentials()) {
        bc.apply(*J_, dr_, du_);
      }

      auto& lin_solver = nonlin_solver_.LinearSolver();

      lin_solver.SetOperator(*J_);

      lin_solver.Mult(dr_, du_);

      displacement_ += du_;

      for (auto& bc : bcs_.essentials()) {
        bc.setDofs(du_, time_);
      }

      // do I have to do this?
      nonlin_solver_.SetOperator(*residual_with_bcs_);
    }

    nonlin_solver_.Mult(zero_, displacement_);
  }

  /**
   * @brief Advance the timestep
   *
   * @param[inout] dt The timestep to attempt. This will return the actual timestep for adaptive timestepping
   * schemes
   * @pre SolidFunctional::completeSetup() must be called prior to this call
   */
  void advanceTimestep(double& dt) override
  {
    SLIC_ERROR_ROOT_IF(!residual_, "completeSetup() must be called prior to advanceTimestep(dt) in SolidFunctional.");

    // Set the mesh nodes to the reference configuration
    if (geom_nonlin_ == GeometricNonlinearities::On) {
      mesh_.NewNodes(*reference_nodes_);
    }

    // bcs_.setTime(time_);

    if (is_quasistatic_) {
      quasiStaticSolve(dt);
    } else {
      ode2_.Step(displacement_, velocity_, time_, dt);
    }

    {
      // after finding displacements that satisfy equilibrium,
      // compute the residual one more time, this time enabling
      // the material state buffers to be updated
      residual_->update_qdata = true;

      // this seems like the wrong way to be doing this assignment, but
      // nodal_forces_ = residual(displacement, ...);
      // isn't currently supported
      nodal_forces_.Vector::operator=((*residual_)(displacement_, zero_, *parameter_states_[parameter_indices]...));

      residual_->update_qdata = false;
    }

    if (geom_nonlin_ == GeometricNonlinearities::On) {
      // Update the mesh with the new deformed nodes
      deformed_nodes_->Set(1.0, displacement_.gridFunction());
      deformed_nodes_->Add(1.0, *reference_nodes_);

      mesh_.NewNodes(*deformed_nodes_);
    }

    cycle_ += 1;
  }

  /**
   * @brief Solve the adjoint problem
   * @pre It is expected that the forward analysis is complete and the current displacement state is valid
   * @note If the essential boundary state is not specified, homogeneous essential boundary conditions are applied
   *
   * @param[in] adjoint_load The dual state that contains the right hand side of the adjoint system (d quantity of
   * interest/d displacement)
   * @param[in] dual_with_essential_boundary A optional finite element dual containing the non-homogenous essential
   * boundary condition data for the adjoint problem
   * @return The computed adjoint finite element state
   */
  virtual const serac::FiniteElementState& solveAdjoint(FiniteElementDual& adjoint_load,
                                                        FiniteElementDual* dual_with_essential_boundary = nullptr)
  {
    if (geom_nonlin_ == GeometricNonlinearities::On) {
      // Set the mesh nodes to the reference configuration
      mesh_.NewNodes(*reference_nodes_);
    }

    mfem::HypreParVector adjoint_load_vector(adjoint_load);

    // Add the sign correction to move the term to the RHS
    adjoint_load_vector *= -1.0;

    auto& lin_solver = nonlin_solver_.LinearSolver();

    // By default, use a homogeneous essential boundary condition
    mfem::HypreParVector adjoint_essential(adjoint_load);
    adjoint_essential = 0.0;

    // sam: is this the right thing to be doing for dynamics simulations,
    // or are we implicitly assuming this should only be used in quasistatic analyses?
    auto drdu =
        serac::get<DERIVATIVE>((*residual_)(differentiate_wrt(displacement_), zero_, *parameter_states_[parameter_indices]...));
    auto jacobian = assemble(drdu);
    auto J_T      = std::unique_ptr<mfem::HypreParMatrix>(jacobian->Transpose());

    // If we have a non-homogeneous essential boundary condition, extract it from the given state
    if (dual_with_essential_boundary) {
      adjoint_essential = *dual_with_essential_boundary;
    }

    for (const auto& bc : bcs_.essentials()) {
      bc.apply(*J_T, adjoint_load_vector, adjoint_essential);
    }

    lin_solver.SetOperator(*J_T);
    lin_solver.Mult(adjoint_load_vector, adjoint_displacement_);

    // Reset the equation solver to use the full nonlinear residual operator
    nonlin_solver_.SetOperator(*residual_with_bcs_);

    if (geom_nonlin_ == GeometricNonlinearities::On) {
      // Update the mesh with the new deformed nodes
      mesh_.NewNodes(*deformed_nodes_);
    }

    return adjoint_displacement_;
  }

  /**
   * @brief Compute the implicit sensitivity of the quantity of interest used in defining the load for the adjoint
   * problem with respect to the parameter field
   *
   * @tparam parameter_field The index of the parameter to take a derivative with respect to
   * @return The sensitivity with respect to the parameter
   *
   * @pre `solveAdjoint` with an appropriate adjoint load must be called prior to this method.
   */
  template <int parameter_field>
  FiniteElementDual& computeSensitivity()
  {
    if (geom_nonlin_ == GeometricNonlinearities::On) {
      // Set the mesh nodes to the reference configuration
      mesh_.NewNodes(*reference_nodes_);
    }

    auto drdparam = serac::get<DERIVATIVE>((*residual_)(DifferentiateWRT<parameter_field + 2>{}, displacement_, zero_,
                                               *parameter_states_[parameter_indices]...));

    auto drdparam_mat = assemble(drdparam);

    drdparam_mat->MultTranspose(adjoint_displacement_, *parameter_sensitivities_[parameter_field]);

    if (geom_nonlin_ == GeometricNonlinearities::On) {
      // Set the mesh nodes back to the reference configuration
      mesh_.NewNodes(*deformed_nodes_);
    }

    return *parameter_sensitivities_[parameter_field];
  }

  /**
   * @brief Get the displacement state
   *
   * @return A reference to the current displacement finite element state
   */
  const serac::FiniteElementState& displacement() const { return displacement_; };

  /// @overload
  serac::FiniteElementState& displacement() { return displacement_; };

  /**
   * @brief Get the adjoint displacement state
   *
   * @return A reference to the current adjoint displacement finite element state
   */
  const serac::FiniteElementState& adjointDisplacement() const { return adjoint_displacement_; };

  /// @overload
  serac::FiniteElementState& adjointDisplacement() { return adjoint_displacement_; };

  /**
   * @brief Get the velocity state
   *
   * @return A reference to the current velocity finite element state
   */
  const serac::FiniteElementState& velocity() const { return velocity_; };

  /// @overload
  serac::FiniteElementState& velocity() { return velocity_; };

  /// @brief getter for nodal forces (before zeroing-out essential dofs)
  const serac::FiniteElementDual& nodalForces() { return nodal_forces_; };

  /// @brief Reset the mesh, displacement, and velocity to the reference (stress-free) configuration
  void resetToReferenceConfiguration()
  {
    displacement_ = 0.0;
    velocity_     = 0.0;

    mesh_.NewNodes(*reference_nodes_);
  }

protected:
  /// The compile-time finite element trial space for displacement and velocity (H1 of order p)
  using trial = H1<order, dim>;

  /// The compile-time finite element test space for displacement and velocity (H1 of order p)
  using test = H1<order, dim>;

  /// The velocity finite element state
  FiniteElementState velocity_;

  /// The displacement finite element state
  FiniteElementState displacement_;

  /// The displacement finite element state
  FiniteElementState adjoint_displacement_;

  /// nodal forces
  FiniteElementDual nodal_forces_;

  /// serac::Functional that is used to calculate the residual and its derivatives
  std::unique_ptr<Functional<test(trial, trial, parameter_space...)>> residual_;

  /// mfem::Operator that calculates the residual after applying essential boundary conditions
  std::unique_ptr<mfem_ext::StdFunctionOperator> residual_with_bcs_;

  /// The finite element states representing user-defined parameter fields
  std::array<const FiniteElementState *, sizeof...(parameter_space)> parameter_states_;

  /// The sensitivities (dual vectors) with repect to each of the input parameter fields
  std::array<std::unique_ptr<FiniteElementDual>, sizeof...(parameter_space)> parameter_sensitivities_;

  /**
   * @brief the ordinary differential equation that describes
   * how to solve for the second time derivative of displacement, given
   * the current displacement, velocity, and source terms
   */
  mfem_ext::SecondOrderODE ode2_;

  /// the specific methods and tolerances specified to solve the nonlinear residual equations
  mfem_ext::EquationSolver nonlin_solver_;

  /// Assembled sparse matrix for the Jacobian
  std::unique_ptr<mfem::HypreParMatrix> J_;

  /// an intermediate variable used to store the predicted end-step displacement
  mfem::Vector predicted_displacement_;

  /// vector used to store the change in essential bcs between timesteps
  mfem::Vector du_;

  /// vector used to store forces arising from du_ when applying time-dependent bcs
  mfem::Vector dr_;

  /// @brief used to communicate the ODE solver's predicted displacement to the residual operator
  mfem::Vector u_;

  /// @brief used to communicate the ODE solver's predicted velocity to the residual operator
  mfem::Vector du_dt_;

  /// @brief the previous acceleration, used as a starting guess for newton's method
  mfem::Vector previous_;

  /// coefficient used to calculate predicted displacement: u_p := u + c0 * d2u_dt2
  double c0_;

  /// coefficient used to calculate predicted velocity: dudt_p := dudt + c1 * d2u_dt2
  double c1_;

  /// @brief A flag denoting whether to compute geometric nonlinearities in the residual
  GeometricNonlinearities geom_nonlin_;

  /// @brief Pointer to the reference mesh data
  std::unique_ptr<mfem::ParGridFunction> reference_nodes_;

  /// @brief Flag to indicate the final mesh node state post-destruction
  FinalMeshOption keep_deformation_;

  /// @brief Pointer to the deformed mesh data
  std::unique_ptr<mfem::ParGridFunction> deformed_nodes_;

  /// @brief Coefficient containing the essential boundary values
  std::shared_ptr<mfem::VectorCoefficient> disp_bdr_coef_;

  /// @brief Coefficient containing the essential boundary values
  std::shared_ptr<mfem::Coefficient> component_disp_bdr_coef_;

  /// @brief An auxilliary zero vector
  mfem::Vector zero_;
};

}  // namespace serac
