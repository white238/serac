#------------------------------------------------------------------------------
# !!!! This is a generated file, edit at own risk !!!!
#------------------------------------------------------------------------------
# CMake executable path: /usr/bin/cmake
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Compilers
#------------------------------------------------------------------------------
# Compiler Spec: gcc@11.1.0
#------------------------------------------------------------------------------
if(DEFINED ENV{SPACK_CC})

  set(CMAKE_C_COMPILER "/home/serac/serac_tpls/spack/lib/spack/env/gcc/gcc" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/home/serac/serac_tpls/spack/lib/spack/env/gcc/g++" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/home/serac/serac_tpls/spack/lib/spack/env/gcc/gfortran" CACHE PATH "")

else()

  set(CMAKE_C_COMPILER "/usr/bin/gcc-11" CACHE PATH "")

  set(CMAKE_CXX_COMPILER "/usr/bin/g++-11" CACHE PATH "")

  set(CMAKE_Fortran_COMPILER "/usr/bin/gfortran" CACHE PATH "")

endif()

set(CMAKE_C_FLAGS "-pthread" CACHE STRING "")

set(CMAKE_CXX_FLAGS "-pthread" CACHE STRING "")

#------------------------------------------------------------------------------
# MPI
#------------------------------------------------------------------------------

set(MPI_C_COMPILER "/usr/bin/mpicc" CACHE PATH "")

set(MPI_CXX_COMPILER "/usr/bin/mpic++" CACHE PATH "")

set(MPI_Fortran_COMPILER "/usr/bin/mpif90" CACHE PATH "")

set(MPIEXEC_EXECUTABLE "/usr/bin/mpirun" CACHE PATH "")

set(MPIEXEC_NUMPROC_FLAG "-np" CACHE STRING "")

set(ENABLE_MPI ON CACHE BOOL "")

#------------------------------------------------------------------------------
# Hardware
#------------------------------------------------------------------------------

set(ENABLE_OPENMP ON CACHE BOOL "")

#------------------------------------------------------------------------------
# TPLs
#------------------------------------------------------------------------------

set(TPL_ROOT "/home/serac/serac_tpls/gcc-11.1.0" CACHE PATH "")

set(AXOM_DIR "${TPL_ROOT}/axom-0.7.0.4-ayvyfa3qlinwthocurhwhjsbtr6gzn55" CACHE PATH "")

set(CAMP_DIR "${TPL_ROOT}/camp-2022.03.2-ecxxjszodysrw4tiujtia5kma7whjszv" CACHE PATH "")

set(CONDUIT_DIR "${TPL_ROOT}/conduit-0.8.4-pfqoghwltlucmgrplqq364e5o6yhq7jr" CACHE PATH "")

set(LUA_DIR "${TPL_ROOT}/lua-5.4.4-ve6ws2zzw7qkrzupei6mnhtwdcxxz6jf" CACHE PATH "")

set(MFEM_DIR "${TPL_ROOT}/mfem-4.5.2.1-25cchjn54wk45jh43hwyufbvb4xd7rxu" CACHE PATH "")

set(HDF5_DIR "${TPL_ROOT}/hdf5-1.8.21-h5dpyqnqdcwv6rcpdz6ywjp53yeqst7a" CACHE PATH "")

set(HYPRE_DIR "${TPL_ROOT}/hypre-2.18.2-rnodffs5cuycfsjth3zbqqxodj7o6gn4" CACHE PATH "")

set(METIS_DIR "${TPL_ROOT}/metis-5.1.0-rlnws7c5nyehxcnegqsto5rgtwhho7ru" CACHE PATH "")

set(PARMETIS_DIR "${TPL_ROOT}/parmetis-4.0.3-lxhbfjt6ldkee4qychk54v64kklppzlt" CACHE PATH "")

set(NETCDF_DIR "${TPL_ROOT}/netcdf-c-4.7.4-ote37ajjhrkqu7ntxkqxryn2g5pc2apx" CACHE PATH "")

set(SUPERLUDIST_DIR "${TPL_ROOT}/superlu-dist-6.1.1-jke7rcnlsep6bbtvimfqkl3sgmgrjpzr" CACHE PATH "")

# ADIAK not built

# AMGX not built

# CALIPER not built

# PETSC not built

set(RAJA_DIR "${TPL_ROOT}/raja-2022.03.0-raeo25xb7cumpvjlhqmrx6fncfuwef55" CACHE PATH "")

set(SUNDIALS_DIR "${TPL_ROOT}/sundials-6.4.1-6qtgnji6ypjvm7jpzfh4xtlohtxetnx6" CACHE PATH "")

set(UMPIRE_DIR "${TPL_ROOT}/umpire-2022.03.1-pieu6hp5juhqvvm6dp645xxlea4igxj3" CACHE PATH "")

#------------------------------------------------------------------------------
# Devtools
#------------------------------------------------------------------------------

# Code checks disabled due to disabled devtools

set(SERAC_ENABLE_CODE_CHECKS OFF CACHE BOOL "")

set(ENABLE_CLANGFORMAT OFF CACHE BOOL "")

set(ENABLE_CLANGTIDY OFF CACHE BOOL "")

set(ENABLE_DOCS OFF CACHE BOOL "")


