#ifndef FLECSI_TOPO_NARRAY_TEST_NARRAY_HH
#define FLECSI_TOPO_NARRAY_TEST_NARRAY_HH

#include "flecsi/data.hh"
#include "flecsi/flog.hh"
#include "flecsi/topo/narray/coloring_utils.hh"
#include "flecsi/topo/narray/interface.hh"

using namespace flecsi;

struct mesh_helper : topo::specialization<topo::narray, mesh_helper> {};

template<std::size_t D>
struct axes_helper {};

template<>
struct axes_helper<1> {
  enum axis { x_axis };
  using axes = mesh_helper::has<x_axis>;
};

template<>
struct axes_helper<2> {
  enum axis { x_axis, y_axis };
  using axes = mesh_helper::has<x_axis, y_axis>;
};

template<>
struct axes_helper<3> {
  enum axis { x_axis, y_axis, z_axis };
  using axes = mesh_helper::has<x_axis, y_axis, z_axis>;
};

template<>
struct axes_helper<4> {
  enum axis { x_axis, y_axis, z_axis, t_axis };
  using axes = mesh_helper::has<x_axis, y_axis, z_axis, t_axis>;
};

template<std::size_t D>
struct mesh : topo::specialization<topo::narray, mesh<D>>, axes_helper<D> {
  static_assert((D >= 1 && D <= 4), "Invalid dimension for testing !");

  using domain = typename mesh::base::domain;

  using axis = typename axes_helper<D>::axis;
  using axes = typename axes_helper<D>::axes;

  struct meta_data {
    double delta;
  };

  static constexpr Dimension dimension = D;

  template<auto>
  static constexpr PrivilegeCount privilege_count = 2;

  using coord = typename mesh::base::coord;
  using gcoord = typename mesh::base::gcoord;
  using coloring_definition = typename mesh::base::coloring_definition;
  using coloring = typename mesh::base::coloring;
  using color_map = typename mesh::base::color_map;
  static coloring color(coloring_definition const & cd) {
    auto [colors, ne, pcs, partitions] =
      topo::narray_utils::color(cd, MPI_COMM_WORLD);
    coloring c;
    c.comm = MPI_COMM_WORLD;
    c.colors = colors;
    c.idx_colorings.emplace_back(std::move(pcs));
    c.partitions.emplace_back(std::move(partitions));
    return c;
  } // color

  /*--------------------------------------------------------------------------*
    Interface
   *--------------------------------------------------------------------------*/

  template<class B>
  struct interface : B {
    template<axis A, domain DM = domain::logical>
    std::size_t size() const {
      return B::template size<topo::elements, A, DM>();
    }

    template<axis A, domain DM = domain::logical>
    auto range() const {
      return B::template range<topo::elements, A, DM>();
    }

    template<axis A, domain DM = domain::logical>
    auto offset() const {
      return B::template offset<topo::elements, A, DM>();
    }
  };
}; // mesh

#endif
