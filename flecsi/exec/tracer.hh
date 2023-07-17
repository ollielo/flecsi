// Copyright (c) 2016, Triad National Security, LLC
// All rights reserved.

#ifndef FLECSI_EXEC_TRACER_HH
#define FLECSI_EXEC_TRACER_HH

namespace flecsi::exec {

// Default implementation that does nothing.
struct trace {

  struct guard;
  using id_t = int;

  inline guard make_guard();

  trace() {}
  explicit trace(id_t) {}

  trace(trace &&) = default;

  void skip() {}

private:
  void start() {}
  void stop() {}
}; // struct trace

} // namespace flecsi::exec

#endif // FLECSI_EXEC_TRACER_HH
