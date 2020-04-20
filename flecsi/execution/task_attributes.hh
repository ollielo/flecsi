/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#if !defined(__FLECSI_PRIVATE__)
#error Do not include this file directly
#endif

#include <flecsi/utils/debruijn.hh>

#include <bitset>

namespace flecsi {

/*!
  The task_attributes_mask_t type allows conversion from an eunumeration
  to a bit mask.

  @note This enumeration is not scoped so that users can create
        masks as in:
        @code
        task_attributes_mask_t m(inner | idempotent);
        @endcode
 */

enum task_attributes_mask_t : size_t {
  leaf = 0x01,
  inner = 0x02,
  idempotent = 0x04,
  loc = 0x08,
  toc = 0x10,
  mpi = 0x20
}; // task_attributes_mask_t

namespace execution {

/*!
  Enumeration of task types.
 */

enum class task_type_t : size_t { leaf, inner, idempotent }; // task_type_t

/*!
  Enumeration of processor types.
 */

enum class task_processor_type_t : size_t {
  loc,
  toc,
  mpi
}; // task_processor_type_t

// Bits for representing task attributes
constexpr size_t task_attributes_bits = 8;
constexpr size_t task_type_bits = 3;

using task_attributes_bitset_t = std::bitset<task_attributes_bits>;

inline task_type_t
mask_to_task_type(size_t mask) {
  return static_cast<task_type_t>(flecsi::utils::debruijn32_t::index(mask));
} // mask_to_task_type

constexpr task_processor_type_t
mask_to_processor_type(size_t mask) {
  const size_t processor_mask = mask >> task_type_bits;

  //  flog_assert(processor_mask && !(processor_mask & (processor_mask - 1)),
  //    "only one processor type can be specified");

  return static_cast<task_processor_type_t>(
    flecsi::utils::debruijn32_t::index(processor_mask));
} // mask_to_processor_type

constexpr bool
leaf_task(task_attributes_bitset_t const & bs) {
  return bs[static_cast<size_t>(task_type_t::leaf)];
}

constexpr bool
inner_task(task_attributes_bitset_t const & bs) {
  return bs[static_cast<size_t>(task_type_t::inner)];
}

constexpr bool
idempotent_task(task_attributes_bitset_t const & bs) {
  return bs[static_cast<size_t>(task_type_t::idempotent)];
}

} // namespace execution
} // namespace flecsi
