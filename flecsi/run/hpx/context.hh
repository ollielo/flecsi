// Copyright (c) 2016, Triad National Security, LLC
// All rights reserved.

#ifndef FLECSI_RUN_HPX_CONTEXT_HH
#define FLECSI_RUN_HPX_CONTEXT_HH

#include <flecsi-config.h>

#if !defined(FLECSI_ENABLE_HPX)
#error FLECSI_ENABLE_HPX not defined! This file depends on HPX!
#endif

#include <hpx/modules/collectives.hpp>
#include <hpx/modules/synchronization.hpp>

#include "flecsi/run/local/context.hh"

#include <cstddef>
#include <map>
#include <utility>

namespace flecsi::run {
/// \defgroup hpx-runtime HPX Runtime
/// Global state.
/// \ingroup runtime
/// \{

struct context_t : local::context {

  //--------------------------------------------------------------------------//
  //  Runtime.
  //--------------------------------------------------------------------------//
  context_t(const arguments::config &);

  int start(const std::function<int()> &);

  Color process() const {
    return process_;
  }

  Color processes() const {
    return processes_;
  }

  Color threads_per_process() const {
    return threads_per_process_;
  }

  Color threads() const {
    return threads_;
  }

  static int task_depth() {
    return 0;
  } // task_depth

  Color color() const {
    return process_;
  }

  Color colors() const {
    return processes_;
  }

  using channel_communicator_data =
    std::pair<::hpx::collectives::channel_communicator, std::size_t>;
  using communicator_data =
    std::pair<::hpx::collectives::communicator, std::size_t>;

  channel_communicator_data p2p_comm(std::string name);
  communicator_data world_comm(std::string name);

  static void termination_detection();

private:
  template<typename Map, typename CreateComm>
  auto
  get_communicator_data(Map & map, std::string name, CreateComm && create_comm);

  arguments::argv argv;
  ::hpx::spinlock mtx;
  std::map<std::string, channel_communicator_data> p2p_comms_;
  std::map<std::string, communicator_data> world_comms_;
};

/// \}
} // namespace flecsi::run

namespace flecsi {
namespace detail {

using task_local_data = std::map<void *, void *>;

// manage task local storage for this task
void create_storage();
task_local_data * storage() noexcept;
void reset_storage() noexcept;

template<typename T>
void
add(void * key) {
  [[maybe_unused]] auto p = storage()->emplace(key, new T());
  flog_assert(
    p.second, "task local storage element should not have been created yet");
}

template<typename T>
void
erase(void * key) noexcept {
  auto * stg = storage();
  auto it = stg->find(key);
  flog_assert(
    it != stg->end(), "task local storage element should have been created");
  delete static_cast<T *>((*it).second);
  (*it).second = nullptr;
}

template<typename T>
T *
get(void * key) noexcept {
  auto * stg = storage();
  auto it = stg->find(key);
  flog_assert(
    it != stg->end(), "task local storage element should have been created");
  return static_cast<T *>((*it).second);
}
} // namespace detail

template<typename T>
struct task_local : private run::task_local_base {
  T & operator*() noexcept {
    return *get();
  }
  T * operator->() noexcept {
    return get();
  }

private:
  void emplace() override {
    detail::add<T>(this);
  }
  void reset() noexcept override {
    detail::erase<T>(this);
  }
  void create_storage() override {
    detail::create_storage();
  }
  void reset_storage() noexcept override {
    detail::reset_storage();
  }

  T * get() noexcept {
    return detail::get<T>(this);
  }
};
} // namespace flecsi

#endif // FLECSI_RUN_HPX_CONTEXT_HH
