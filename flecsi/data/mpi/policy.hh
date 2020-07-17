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

#include <cstddef>

#include "flecsi/data/field_info.hh"
#include "flecsi/topo/core.hh" // single_space

namespace flecsi {
namespace data {

struct region {
  region(size2, const fields &) {}

  size2 size() const {
    return {};
  }
};

struct partition {
  using row = std::size_t;
  static row make_row(std::size_t, std::size_t n) {
    return n;
  }

  explicit partition(const region &) {}
  partition(const region &,
    const partition &,
    field_id_t,
    completeness = incomplete) {}

  std::size_t colors() const {
    return 0;
  }
  template<topo::single_space>
  const partition & get_partition() const {
    return *this;
  }
};

} // namespace data
} // namespace flecsi
