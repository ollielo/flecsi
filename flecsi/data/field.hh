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

#include "flecsi/data/topology_slot.hh"
#include <flecsi/data/privilege.hh>
#include <flecsi/data/storage_classes.hh>
#include <flecsi/runtime/backend.hh>
#include <flecsi/runtime/types.hh>

namespace flecsi {
namespace data {

/// A data accessor.
/// \tparam STORAGE_CLASS data layout
/// \tparam TOPOLOGY_TYPE core topology type
/// \tparam T data type
/// \tparam Priv access privileges
template<storage_label_t STORAGE_CLASS,
  typename TOPOLOGY_TYPE,
  typename T,
  std::size_t Priv>
struct accessor;

/*!
  The field_reference_t type is used to reference fields. It adds a \em
  topology field to the \c reference_base to track the
  associated topology instance.
 */
template<class Topo>
struct field_reference_t {
  // The use of the slot allows creating field references statically, before
  // the topology_data has been allocated.
  using topology_t = topology_slot<Topo>;

  field_reference_t(const field_info_t & info, const topology_t & topology)
    : fid_(info.fid), topology_(&topology) {}

  field_id_t fid() const {
    return fid_;
  }
  const topology_t & topology() const {
    return *topology_;
  } // topology_identifier

private:
  field_id_t fid_;
  const topology_t * topology_;

}; // struct field_reference

/// A \c field_reference is a \c field_reference_t tagged with a data type.
/// \tparam T data type (merely for type safety)
template<class T, class Topo>
struct field_reference : field_reference_t<Topo> {
  using value_type = T;
  using field_reference_t<Topo>::field_reference_t;
};

/*!
  The field_member type provides a mechanism to define and register
  fundamental field types with the FleCSI runtime.

  @tparam DATA_TYPE     A compact type that will be defined at each index of
                        the associated topological index space. FleCSI defines
                        a compact type as either a P.O.D. type, or a
                        user-defined type that does not have external
                        references, i.e., sizeof for a compact type is equal to
                        the logical serialization of that type.
  @tparam STORAGE_CLASS The label of the storage class for the field member.
  @tparam TOPOLOGY_TYPE A specialization of a core FleCSI topology type.
  @tparam INDEX_SPACE   The id of the index space on which to define the field.
 */

template<typename DATA_TYPE,
  storage_label_t STORAGE_CLASS,
  typename TOPOLOGY_TYPE,
  size_t INDEX_SPACE>
struct field_member : field_info_t {

  using topology_reference_t = topology_slot<TOPOLOGY_TYPE>;

  template<size_t... PRIVILEGES>
  using accessor = accessor<STORAGE_CLASS,
    TOPOLOGY_TYPE,
    DATA_TYPE,
    privilege_pack<PRIVILEGES...>::value>;

  field_member()
    : field_info_t{unique_fid_t::instance().next(), sizeof(DATA_TYPE)} {
    runtime::context_t::instance().add_field_info<TOPOLOGY_TYPE, INDEX_SPACE>(
      STORAGE_CLASS, *this);
  }
  // Copying/moving is inadvisable because the context knows the address.
  field_member(const field_member &) = delete;
  field_member & operator=(const field_member &) = delete;

  /*!
    Return a reference to the field instance associated with the given topology
    reference.

    @param topology_reference A reference to a valid topology instance.
   */

  field_reference<DATA_TYPE, TOPOLOGY_TYPE> operator()(
    topology_reference_t const & topology_reference) const {

    return {*this, topology_reference};
  } // operator()
}; // struct field_member

} // namespace data
} // namespace flecsi