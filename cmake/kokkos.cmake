# Copyright (c) 2016, Triad National Security, LLC
# All rights reserved

option(ENABLE_KOKKOS "Enable Kokkos" OFF)

if(ENABLE_KOKKOS)
  find_package(Kokkos REQUIRED)
  
  if(Kokkos_ENABLE_CUDA AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
    NOT ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 8)
    message(FATAL_ERROR "Clang version 8 or greater required for Kokkos")
  endif()

  get_target_property(KOKKOS_COMPILE_OPTIONS Kokkos::kokkoscore
    INTERFACE_COMPILE_OPTIONS)

  list(APPEND TPL_LIBRARIES Kokkos::kokkos)
endif()
