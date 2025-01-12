// Copyright (c) 2016, Triad National Security, LLC
// All rights reserved.

#ifndef FLECSI_TOPO_NARRAY_INTERFACE_HH
#define FLECSI_TOPO_NARRAY_INTERFACE_HH

#include "flecsi/data/accessor.hh"
#include "flecsi/data/copy_plan.hh"
#include "flecsi/data/layout.hh"
#include "flecsi/data/privilege.hh"
#include "flecsi/flog.hh"
#include "flecsi/topo/core.hh"
#include "flecsi/topo/index.hh"
#include "flecsi/topo/narray/coloring_utils.hh"
#include "flecsi/topo/narray/types.hh"
#include "flecsi/topo/types.hh"
#include "flecsi/util/array_ref.hh"

#include <memory>
#include <utility>

namespace flecsi {
namespace topo {
/// \defgroup narray Multi-dimensional Array
/// Configurable multi-dimensional array topology.
/// Can be used for structured meshes.
/// \ingroup topology
/// \{

/*!
  Narray Topology.
  \tparam Policy the specialization, following
   \ref narray_specialization.

  \sa topo::specialization, topo::topology
 *----------------------------------------------------------------------------*/
template<typename Policy>
struct narray : narray_base, with_ragged<Policy>, with_meta<Policy> {

  using index_space = typename Policy::index_space;
  using index_spaces = typename Policy::index_spaces;
  using axis = typename Policy::axis;
  using axes = typename Policy::axes;
  using id = util::id;

  static constexpr Dimension dimension = Policy::dimension;

  /// This type is the topology accessor base type "B"
  /// from which specialization interface type is derived.
  /// \sa core
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
  repartition & get_partition() {
    return part_.template get<S>();
  }

  template<typename Type,
    data::layout Layout,
    typename Policy::index_space Space>
  void ghost_copy(
    data::field_reference<Type, Layout, Policy, Space> const & f) {
    plan_.template get<Space>().issue_copy(f.fid());
  }

private:
  /// Structural information about one color.
  /// \image html narray-layout.png "Layouts for each possible orientation."
  struct meta_data {
    /// Two bits for each axis that give the position of the color along the
    /// axis (\e low, \e interior, or \e high).
    std::uint32_t orientation;

    using scoord = util::key_array<std::size_t, axes>;
    using shypercube = std::array<scoord, 2>;

    /// Global extents.
    /// These are necessarily the same on every color.
    scoord global;
    /// The global offsets to the beginning of the color's region, excluding
    /// any non-physical boundary padding.
    /// Use to map from local to global ids.
    scoord offset;
    /// The size of the color's region, including ghosts and boundaries.
    scoord extents;
    /// The domain of the color's elements that logically exist.
    /// Ghosts and boundaries are not included.
    shypercube logical;
    /// The domain of the color's elements, including boundaries but not
    /// ghosts.
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
    auto lm = data::launch::make(this->meta);
    execute<set_meta<Value...>, mpi>(meta_field(lm), c);
    init_policy_meta(c);
  }

  /*!
   Method to create copy plans for entities of an index-sapce.
   @param colors  The number of colors
   @param vpc  Vector of process_colors, where an index into vpc provides
   coloring information corresponding to a particular color.
  */
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

    // In this method, a mpi task "idx_itvls" is invoked, which computes couple
    // of information: intervals and points. The intervals encode local ghost
    // intervals, whereas points capture the  local offset and corresponding
    // remote/shared offset on remote/shared color. The intervals and points are
    // used to create function objects "dest_tasks" and "ptrs_tasks" that is
    // subsequently used by copy plan to perform the data communication. Note
    // that after this call the copy plan objects have been created. The actual
    // communication is invoked as part of task execution depending upon the
    // privilege requirements of the task.

    execute<idx_itvls, mpi>(vpc, num_intervals, intervals, points, comm);

    // clang-format off
    auto dest_task = [&intervals, &comm](auto f) {
      auto lm = data::launch::make(f.topology());
      execute<set_dests, mpi>(lm(f), intervals, comm);
    };

    auto ptrs_task = [&points, &comm](auto f) {
      auto lm = data::launch::make(f.topology());
      execute<set_ptrs<Policy::template privilege_count<S>>, mpi>(
        lm(f), points, comm);
    };
    // clang-format on

    return {*this, p, num_intervals, dest_task, ptrs_task, util::constant<S>()};
  }

  static void set_meta_idx(meta_data & md, const process_color & pc) {
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

    md.orientation = pc.orientation;
    copy(pc.global, md.global);
    copy(pc.offset, md.offset);
    copy(pc.extents, md.extents);
    copy2(pc.logical, md.logical);
    copy2(pc.extended, md.extended);
  }

  template<auto... Value> // index_spaces
  static void set_meta(
    data::multi<typename field<util::key_array<meta_data, index_spaces>,
      data::single>::template accessor<wo>> mm,
    narray_base::coloring const & c) {
    const auto ma = mm.accessors();
    for(auto i = ma.size(); i--;) {
      std::size_t index{0};
      (set_meta_idx(ma[i]->template get<Value>(), c.idx_colorings[index++][i]),
        ...);
    }
  }

  static void set_policy_meta(typename field<typename Policy::meta_data,
    data::single>::template accessor<wo>) {}

  /// Initialization for user policy meta_data.
  /// Executes a mpi task "set_policy_meta" which
  /// copies the relevant user-define meta data as part of the topology.
  void init_policy_meta(narray_base::coloring const &) {
    execute<set_policy_meta>(policy_meta_field(this->meta));
  }

  auto & get_sizes(std::size_t i) {
    return part_[i].sz;
  }

  /*--------------------------------------------------------------------------*
    Private data members.
   *--------------------------------------------------------------------------*/

  // fields for storing topology meta data per index-space
  static inline const typename field<util::key_array<meta_data, index_spaces>,
    data::single>::template definition<meta<Policy>>
    meta_field;

  // field for storing user-defined meta data
  static inline const typename field<typename Policy::meta_data,
    data::single>::template definition<meta<Policy>>
    policy_meta_field;

  // index-space specific parts
  util::key_array<repartitioned, index_spaces> part_;

  // index-space specific copy plans
  util::key_array<data::copy_plan, index_spaces> plan_;

}; // struct narray

template<class P>
struct borrow_extra<narray<P>> {
  borrow_extra(narray<P> &, claims::core &, bool) {}

  auto & get_sizes(std::size_t i) {
    return borrow_base::derived(*this).spc[i].sz;
  }
};

/*----------------------------------------------------------------------------*
  Narray Access.
 *----------------------------------------------------------------------------*/

template<typename Policy>
template<Privileges Priv>
struct narray<Policy>::access {
  friend Policy;

  ///  This method provides a mdspan of the field underlying data.
  ///  It can be used to create data views with the shape appropriate to S.
  /// This function is \ref topology "host-accessible", although the values in
  /// \a a are typically not.
  template<index_space S, typename T, Privileges P>
  FLECSI_INLINE_TARGET auto mdspan(
    data::accessor<data::dense, T, P> const & a) const {
    auto const s = a.span();
    return util::mdspan<typename decltype(s)::element_type, dimension>(
      s.data(), extents<S>());
  }
  /// Create a Fortran-like view of a field.
  /// This function is \ref topology "host-accessible", although the values in
  /// \a a are typically not.
  /// \return \c\ref mdcolex
  template<index_space S, typename T, Privileges P>
  FLECSI_INLINE_TARGET auto mdcolex(
    data::accessor<data::dense, T, P> const & a) const {
    return util::mdcolex<
      typename std::remove_reference_t<decltype(a)>::element_type,
      dimension>(a.span().data(), extents<S>());
  }

  template<class F>
  void send(F && f) {
    std::size_t i{0};
    for(auto & a : size_)
      a.topology_send(
        f, [&i](auto & n) -> auto & { return n.get_sizes(i++); });
    const auto meta = [](auto & n) -> auto & {
      return n.meta;
    };
    meta_.topology_send(f, meta);
    policy_meta_.topology_send(f, meta);
  }

private:
  data::scalar_access<narray::policy_meta_field, Priv> policy_meta_;
  util::key_array<data::scalar_access<topo::resize::field, Priv>, index_spaces>
    size_;

  data::scalar_access<narray::meta_field, Priv> meta_;

  access() {}

  /*!
   This domain enumeration provides a classification of the various
   types of partition entities that can be requested out of a topology
   specialization created using this type. The following describes what each
   of the domain enumeration means in a mesh part returned by the coloring
   algorithm. For the structured mesh partitioning, the partition info is
   specified per axis.

   These domains are used in many of the interface methods to provide
   information such as size, extents, offsets about them.
  */
  enum class domain : std::size_t {
    logical, ///<  the logical, i.e., the owned part of the axis
    extended, ///< the boundary padding along with the logical part
    all, ///< the ghost padding along with the logical part
    boundary_low, ///< the boundary padding on the lower bound of the axis
    boundary_high, ///< the boundary padding on the upper bound of the axis
    ghost_low, ///< the ghost padding on the lower bound of the axis
    ghost_high, ///< the ghost padding on the upper bound of the axis
    global ///< global info about the mesh, the meaning depends on what is being
           ///< queried
  };

  using hypercubes = index::has<domain::logical,
    domain::extended,
    domain::all,
    domain::boundary_low,
    domain::boundary_high,
    domain::ghost_low,
    domain::ghost_high,
    domain::global>;

  template<index_space S, axis A>
  FLECSI_INLINE_TARGET std::size_t global_id(std::size_t i) const {
    return offset<S, A>() + i;
  }

  template<index_space S, axis A>
  FLECSI_INLINE_TARGET std::uint32_t orientation() const {
    return meta_->template get<S>().orientation >> to_idx<A>() * 2;
  }

  template<index_space S, axis A>
  FLECSI_INLINE_TARGET std::size_t global() const {
    return meta_->template get<S>().global.template get<A>();
  }

  template<index_space S, axis A>
  FLECSI_INLINE_TARGET std::size_t offset() const {
    return meta_->template get<S>().offset.template get<A>();
  }

  /// \deprecated Renamed to \c range (currently a type).
  template<index_space S, axis A>
  FLECSI_INLINE_TARGET std::size_t extents() const {
    return meta_->template get<S>().extents.template get<A>();
  }

  template<index_space S>
  FLECSI_INLINE_TARGET auto extents() const {
    return meta_->template get<S>().extents;
  }

  template<index_space S, axis A, std::size_t P>
  FLECSI_INLINE_TARGET std::size_t logical() const {
    return meta_->template get<S>().logical[P].template get<A>();
  }

  template<index_space S, axis A, std::size_t P>
  FLECSI_INLINE_TARGET std::size_t extended() const {
    return meta_->template get<S>().extended[P].template get<A>();
  }

  /*!
   Method to check if an axis of the local mesh is incident on the lower
   bound of the corresponding axis of the global mesh.
   This function is \ref topology "host-accessible".
   \sa meta_data, process_color
  */
  template<index_space S, axis A>
  FLECSI_INLINE_TARGET bool is_low() const {
    return orientation<S, A>() & narray_impl::low;
  }

  /*!
   Method to check if an axis of the local mesh is incident on the upper
   bound of the corresponding axis of the global mesh.
   This function is \ref topology "host-accessible".
   \sa meta_data, process_color
  */
  template<index_space S, axis A>
  FLECSI_INLINE_TARGET bool is_high() const {
    return orientation<S, A>() & narray_impl::high;
  }

  /*!
   Method to check if axis A of index-space S is in between the lower and upper
   bound along axis A of the global domain.
   This function is \ref topology "host-accessible".
   \sa meta_data, process_color
  */
  template<index_space S, axis A>
  FLECSI_INLINE_TARGET bool is_interior() const {
    return !is_low<S, A>() && !is_high<S, A>();
  }

  template<index_space S, axis A>
  FLECSI_INLINE_TARGET bool is_degenerate() const {
    return is_low<S, A>() && is_high<S, A>();
  }

  /*!
    Method to return size of the index-space S along axis A for domain SE.
    This function is \ref topology "host-accessible".
    \sa enum domain
  */
  template<index_space S, axis A, domain DM>
  FLECSI_INLINE_TARGET std::size_t size() const {
    static_assert(
      std::size_t(DM) < hypercubes::size, "invalid size identifier");
    if constexpr(DM == domain::logical) {
      return logical<S, A, 1>() - logical<S, A, 0>();
    }
    else if constexpr(DM == domain::extended) {
      return extended<S, A, 1>() - extended<S, A, 0>();
    }
    else if constexpr(DM == domain::all) {
      return extents<S, A>();
    }
    else if constexpr(DM == domain::boundary_low) {
      return logical<S, A, 0>() - extended<S, A, 0>();
    }
    else if constexpr(DM == domain::boundary_high) {
      return extended<S, A, 1>() - logical<S, A, 1>();
    }
    else if constexpr(DM == domain::ghost_low) {
      if(!is_low<S, A>())
        return logical<S, A, 0>();
      else
        return 0;
    }
    else if constexpr(DM == domain::ghost_high) {
      if(!is_high<S, A>())
        return extents<S, A>() - logical<S, A, 1>();
      else
        return 0;
    }
    else if constexpr(DM == domain::global) {
      return global<S, A>();
    }
  }

  /*!
    Method to return an iterator over the extents of the index-space S along
    axis A for domain DM.
    \tparam DM not \c domain::global
    This function is \ref topology "host-accessible".
  */
  template<index_space S, axis A, domain DM>
  FLECSI_INLINE_TARGET auto range() const {
    static_assert(
      std::size_t(DM) < hypercubes::size, "invalid domain identifier");

    if constexpr(DM == domain::logical) {
      return make_ids<S>(
        util::iota_view<util::id>(logical<S, A, 0>(), logical<S, A, 1>()));
    }
    else if constexpr(DM == domain::extended) {
      return make_ids<S>(
        util::iota_view<util::id>(extended<S, A, 0>(), extended<S, A, 1>()));
    }
    else if constexpr(DM == domain::all) {
      return make_ids<S>(util::iota_view<util::id>(0, extents<S, A>()));
    }
    else if constexpr(DM == domain::boundary_low) {
      return make_ids<S>(util::iota_view<util::id>(0, size<S, A, DM>()));
    }
    else if constexpr(DM == domain::boundary_high) {
      return make_ids<S>(util::iota_view<util::id>(
        logical<S, A, 1>(), logical<S, A, 1>() + size<S, A, DM>()));
    }
    else if constexpr(DM == domain::ghost_low) {
      return make_ids<S>(util::iota_view<util::id>(0, size<S, A, DM>()));
    }
    else if constexpr(DM == domain::ghost_high) {
      return make_ids<S>(util::iota_view<util::id>(
        logical<S, A, 1>(), logical<S, A, 1>() + size<S, A, DM>()));
    }
    else {
      flog_error("invalid domain");
    }
  }

  /*!
    Method to return an offset of the index-space S along axis A for domain SE.
    This function is \ref topology "host-accessible".
    \sa enum domain
  */
  template<index_space S, axis A, domain DM>
  FLECSI_INLINE_TARGET std::size_t offset() const {
    static_assert(
      std::size_t(DM) < hypercubes::size, "invalid offset identifier");
    if constexpr(DM == domain::logical) {
      return logical<S, A, 0>();
    }
    else if constexpr(DM == domain::extended) {
      return extended<S, A, 0>();
    }
    else if constexpr(DM == domain::all) {
      return 0;
    }
    else if constexpr(DM == domain::boundary_low) {
      return extended<S, A, 0>();
    }
    else if constexpr(DM == domain::boundary_high) {
      return logical<S, A, 1>();
    }
    else if constexpr(DM == domain::ghost_low) {
      return 0;
    }
    else if constexpr(DM == domain::ghost_high) {
      return logical<S, A, 1>();
    }
    else if constexpr(DM == domain::global) {
      return offset<S, A>();
    }
  }

  template<axis A>
  FLECSI_TARGET static constexpr std::uint32_t to_idx() {
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

#ifdef DOXYGEN
/// Example specialization which is not really implemented.
struct narray_specialization : specialization<narray, narray_specialization> {

  /// Enumeration of the axes, they should be
  /// consistent with the dimension of mesh.
  enum axis { x, y };
  /// Axes to store.
  /// The format is\code
  /// has<x, y, ..>
  /// \endcode
  using axes = has<x, y>;

  /// mesh dimension
  static constexpr Dimension dimension = 2;
}
#endif

/// \}
} // namespace topo
} // namespace flecsi

#endif
