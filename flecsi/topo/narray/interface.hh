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

#include "flecsi/data/accessor.hh"
#include "flecsi/data/copy_plan.hh"
#include "flecsi/data/layout.hh"
#include "flecsi/data/privilege.hh"
#include "flecsi/flog.hh"
#include "flecsi/topo/core.hh"
#include "flecsi/topo/index.hh"
#include "flecsi/topo/narray/coloring_utils.hh"
#include "flecsi/topo/narray/types.hh"
#include "flecsi/topo/utility_types.hh"
#include "flecsi/util/array_ref.hh"

#include <memory>
#include <utility>

namespace flecsi {
namespace topo {

/*----------------------------------------------------------------------------*
  Narray Topology.
 *----------------------------------------------------------------------------*/

template<typename Policy>
struct narray : narray_base, with_ragged<Policy>, with_meta<Policy> {

  using index_space = typename Policy::index_space;
  using index_spaces = typename Policy::index_spaces;
  using axis = typename Policy::axis;
  using axes = typename Policy::axes;
  using id = util::id;

  static constexpr Dimension dimension = Policy::dimension;

  template<Privileges>
  struct access;

  narray(coloring const & c)
    : narray(
        [&c]() -> auto & {
          flog_assert(c.idx_colorings.size() == index_spaces::size,
            c.idx_colorings.size()
              << " sizes for " << index_spaces::size << " index spaces");
          return c;
        }(),
        index_spaces(),
        std::make_index_sequence<index_spaces::size>()) {}

  Color colors() const {
    return part_.front().colors();
  }

  template<index_space S>
  data::region & get_region() {
    return part_.template get<S>();
  }

  template<index_space S>
  const data::partition & get_partition(field_id_t) const {
    return part_.template get<S>();
  }

  template<typename Type,
    data::layout Layout,
    typename Topo,
    typename Topo::index_space Space>
  void ghost_copy(data::field_reference<Type, Layout, Topo, Space> const & f) {
    plan_.template get<Space>().issue_copy(f.fid());
  }

private:
  struct meta_data {
    std::uint32_t orientation;

    using scoord = util::key_array<std::size_t, axes>;
    using shypercube = std::array<scoord, 2>;

    scoord global;
    scoord offset;
    scoord extents;
    shypercube logical;
    shypercube extended;
  };

  template<auto... Value, std::size_t... Index>
  narray(const coloring & c,
    util::constants<Value...> /* index spaces to deduce pack */,
    std::index_sequence<Index...>)
    : with_ragged<Policy>(c.colors), with_meta<Policy>(c.colors),
      part_{{make_repartitioned<Policy, Value>(c.colors,
        make_partial<idx_size>(c.partitions[Index]))...}},
      plan_{{make_copy_plan<Value>(c.colors,
        c.idx_colorings[Index],
        part_[Index],
        c.comm)...}} {
    init_meta(c);
    init_policy_meta(c);
  }

  template<index_space S>
  data::copy_plan make_copy_plan(Color colors,
    std::vector<process_color> const & vpc,
    repartitioned & p,
    MPI_Comm const & comm) {

    std::vector<std::size_t> num_intervals(colors, 0);
    std::vector<std::vector<std::pair<std::size_t, std::size_t>>> intervals;
    std::vector<
      std::map<Color, std::vector<std::pair<std::size_t, std::size_t>>>>
      points;

    execute<idx_itvls, mpi>(vpc, num_intervals, intervals, points, comm);

    // clang-format off
    auto dest_task = [&intervals, &comm](auto f) {
      execute<set_dests, mpi>(f, intervals, comm);
    };

    auto ptrs_task = [&points, &comm](auto f) {
      execute<set_ptrs<Policy::template privilege_count<S>>, mpi>(
        f, points, comm);
    };
    // clang-format on

    return {*this, p, num_intervals, dest_task, ptrs_task, util::constant<S>()};
  }

  static void set_meta_idx(meta_data & md,
    std::vector<process_color> const & vpc) {
    // clang-format off
    static constexpr auto copy = [](const coord & c,
      typename meta_data::scoord & s) {
      const auto n = s.size();
      flog_assert(
        c.size() == n, "invalid #axes(" << c.size() << ") must be: " << n);
      std::copy_n(c.begin(), n, s.begin());
    };

    static constexpr auto copy2 = [](const hypercube & h,
      typename meta_data::shypercube & s) {
      for(auto i = h.size(); i--;)
        copy(h[i], s[i]);
    };
    // clang-format on

    md.orientation = vpc[0].orientation;
    copy(vpc[0].global, md.global);
    copy(vpc[0].offset, md.offset);
    copy(vpc[0].extents, md.extents);
    copy2(vpc[0].logical, md.logical);
    copy2(vpc[0].extended, md.extended);
  }

  template<auto... Value>
  static void visit_meta_is(util::key_array<meta_data, index_spaces> & m,
    narray_base::coloring const & c,
    util::constants<Value...> /* index spaces to deduce pack */) {
    std::size_t index{0};
    (set_meta_idx(m.template get<Value>(), c.idx_colorings[index++]), ...);
  }

  static void set_meta(typename field<util::key_array<meta_data, index_spaces>,
                         data::single>::template accessor<wo> m,
    narray_base::coloring const & c) {
    visit_meta_is(m, c, index_spaces());
  }

  void init_meta(narray_base::coloring const & c) {
    execute<set_meta, mpi>(meta_field(this->meta), c);
  }

  // Default initilaization for user policy meta data
  static void set_policy_meta(typename field<typename Policy::meta_data,
    data::single>::template accessor<wo>) {}

  void init_policy_meta(narray_base::coloring const &) {
    execute<set_policy_meta, mpi>(policy_meta_field(this->meta));
  }

  /*--------------------------------------------------------------------------*
    Private data members.
   *--------------------------------------------------------------------------*/

  static inline const typename field<util::key_array<meta_data, index_spaces>,
    data::single>::template definition<meta<Policy>>
    meta_field;

  static inline const typename field<typename Policy::meta_data,
    data::single>::template definition<meta<Policy>>
    policy_meta_field;

  util::key_array<repartitioned, index_spaces> part_;
  util::key_array<data::copy_plan, index_spaces> plan_;

}; // struct narray

/*----------------------------------------------------------------------------*
  Narray Access.
 *----------------------------------------------------------------------------*/

template<typename Policy>
template<Privileges Priv>
struct narray<Policy>::access {
  friend Policy;

  template<index_space S, typename T, Privileges P>
  auto mdspan(data::accessor<data::dense, T, P> const & a) const {
    auto const s = a.span();
    return util::mdspan<typename decltype(s)::element_type, dimension>(
      s.data(), extents<S>());
  }
  template<index_space S, typename T, Privileges P>
  auto mdcolex(data::accessor<data::dense, T, P> const & a) const {
    return util::mdcolex<
      typename std::remove_reference_t<decltype(a)>::element_type,
      dimension>(a.span().data(), extents<S>());
  }

  template<class F>
  void send(F && f) {
    std::size_t i{0};
    for(auto & a : size_)
      a.topology_send(
        f, [&i](narray & n) -> auto & { return n.part_[i++].sz; });

    meta_.topology_send(f, &narray::meta);
    policy_meta_.topology_send(f, &narray::meta);
  }

private:
  data::scalar_access<narray::policy_meta_field, Priv> policy_meta_;
  util::key_array<data::scalar_access<topo::resize::field, Priv>, index_spaces>
    size_;

  data::scalar_access<narray::meta_field, Priv> meta_;

  access() {}

  enum class range : std::size_t {
    logical,
    extended,
    all,
    boundary_low,
    boundary_high,
    ghost_low,
    ghost_high,
    global
  };

  using hypercubes = index::has<range::logical,
    range::extended,
    range::all,
    range::boundary_low,
    range::boundary_high,
    range::ghost_low,
    range::ghost_high,
    range::global>;

  template<index_space S>
  std::uint32_t orientation() const {
    return meta_->template get<S>().orientation;
  }

  template<index_space S, axis A>
  std::size_t global() const {
    return meta_->template get<S>().global.template get<A>();
  }

  template<index_space S, axis A>
  std::size_t offset() const {
    return meta_->template get<S>().offset.template get<A>();
  }

  template<index_space S, axis A>
  std::size_t extents() const {
    return meta_->template get<S>().extents.template get<A>();
  }

  template<index_space S>
  auto extents() const {
    return meta_->template get<S>().extents;
  }

  template<index_space S, std::size_t P, axis A>
  std::size_t logical() const {
    return meta_->template get<S>().logical[P].template get<A>();
  }

  template<index_space S, std::size_t P, axis A>
  std::size_t extended() const {
    return meta_->template get<S>().extended[P].template get<A>();
  }

  template<index_space S, axis A>
  bool is_low() const {
    return (orientation<S>() >> to_idx<A>() * 2) & narray_impl::low;
  }

  template<index_space S, axis A>
  bool is_high() const {
    return (orientation<S>() >> to_idx<A>() * 2) & narray_impl::high;
  }

  template<axis A>
  bool is_interior() const {
    return !is_low<A>() && !is_high<A>();
  }

  template<axis A>
  bool is_degenerate() const {
    return is_low<A>() && is_high<A>();
  }

  template<index_space S, axis A, range SE>
  std::size_t size() const {
    static_assert(
      std::size_t(SE) < hypercubes::size, "invalid size identifier");
    if constexpr(SE == range::logical) {
      return logical<S, 1, A>() - logical<S, 0, A>();
    }
    else if constexpr(SE == range::extended) {
      return extended<S, 1, A>() - extended<S, 0, A>();
    }
    else if constexpr(SE == range::all) {
      return extents<S, A>();
    }
    else if constexpr(SE == range::boundary_low) {
      return logical<S, 0, A>() - extended<S, 0, A>();
    }
    else if constexpr(SE == range::boundary_high) {
      return extended<S, 1, A>() - logical<S, 1, A>();
    }
    else if constexpr(SE == range::ghost_low) {
      if(!is_low<S, A>())
        return logical<S, 0, A>();
      else
        return 0;
    }
    else if constexpr(SE == range::ghost_high) {
      if(!is_high<S, A>())
        return extents<S, A>() - logical<S, 1, A>();
      else
        return 0;
    }
    else if constexpr(SE == range::global) {
      return global<S, A>();
    }
  }

  template<index_space S, axis A, range SE>
  auto extents() const {
    static_assert(
      std::size_t(SE) < hypercubes::size, "invalid range identifier");

    if constexpr(SE == range::logical) {
      return make_ids<S>(
        util::iota_view<util::id>(logical<S, 0, A>(), logical<S, 1, A>()));
    }
    else if constexpr(SE == range::extended) {
      return make_ids<S>(
        util::iota_view<util::id>(extended<S, 0, A>(), extended<S, 1, A>()));
    }
    else if constexpr(SE == range::all) {
      return make_ids<S>(util::iota_view<util::id>(0, extents<S, A>()));
    }
    else if constexpr(SE == range::boundary_low) {
      return make_ids<S>(util::iota_view<util::id>(0, size<S, A, SE>()));
    }
    else if constexpr(SE == range::boundary_high) {
      return make_ids<S>(util::iota_view<util::id>(
        logical<S, 1, A>(), logical<S, 1, A>() + size<S, A, SE>()));
    }
    else if constexpr(SE == range::ghost_low) {
      return make_ids<S>(util::iota_view<util::id>(0, size<S, A, SE>()));
    }
    else if constexpr(SE == range::ghost_high) {
      return make_ids<S>(util::iota_view<util::id>(
        logical<S, 1, A>(), logical<S, 1, A>() + size<S, A, SE>()));
    }
    else {
      flog_error("invalid range");
    }
  }

  template<index_space S, axis A, range SE>
  std::size_t offset() const {
    static_assert(
      std::size_t(SE) < hypercubes::size, "invalid offset identifier");
    if constexpr(SE == range::logical) {
      return logical<S, 0, A>();
    }
    else if constexpr(SE == range::extended) {
      return extended<S, 0, A>();
    }
    else if constexpr(SE == range::all) {
      return 0;
    }
    else if constexpr(SE == range::boundary_low) {
      return extended<S, 0, A>();
    }
    else if constexpr(SE == range::boundary_high) {
      return logical<S, 1, A>();
    }
    else if constexpr(SE == range::ghost_low) {
      return 0;
    }
    else if constexpr(SE == range::ghost_high) {
      return logical<S, 1, A>();
    }
    else if constexpr(SE == range::global) {
      return offset<S, A>();
    }
  }

  template<axis A>
  static constexpr std::uint32_t to_idx() {
    using axis_t = typename std::underlying_type_t<axis>;
    static_assert(std::is_convertible_v<axis_t, std::uint32_t>,
      "invalid axis type: cannot be converted to std::uint32_t");
    return static_cast<std::uint32_t>(A);
  }
}; // struct narray<Policy>::access

/*----------------------------------------------------------------------------*
  Define Base.
 *----------------------------------------------------------------------------*/

template<>
struct detail::base<narray> {
  using type = narray_base;
}; // struct detail::base<narray>

} // namespace topo
} // namespace flecsi
