#include <chrono>
#include <cstring>
#include <thread>

#ifdef __QNX__
#include <libgen.h>
#endif

#include <vsomeip/internal/logger.hpp>

#include "../include/simple_connector.hpp"

#ifndef VSOMEIP_ENV_USE_ASYNCHRONOUS_SD
#define VSOMEIP_ENV_USE_ASYNCHRONOUS_SD "VSOMEIP_USE_ASYNCHRONOUS_SD"
#endif

namespace vsomeip_v3 {

simple_connector::simple_connector()
    : network_ready_(
#ifdef __QNX__
          false
#else
          true
#endif
      ) {
}

simple_connector::~simple_connector() {
  if (wait_for_network_thread_.joinable()) {
    wait_for_network_thread_.join();
  }
}

void simple_connector::register_net_if_changes_handler(
    const simple_net_ready_handler_t &_handler) {
  handler_ = _handler;
}

void simple_connector::unregister_net_if_changes_handler() {
  handler_ = nullptr;
}

void simple_connector::stop() { return; }

bool simple_connector::wait_for_interface() {
#ifdef __QNX__
  namespace cr = std::chrono;
  static std::string_view constexpr path = VSOMEIP_ENV_NETWORK_INT_READY_FILE;
  if (path.empty()) {
    VSOMEIP_ERROR << "No network interface signal path defined, service "
                     "discovery will effectively be disabled.";
    return false;
  }
  // Indefinite delay.  If the condition we're waiting for doesn't occur then
  // we are in an error state and thus should not continue.
  static auto constexpr delay_ms = std::numeric_limits<int>::max();
  static int constexpr poll_ms = 50;

  auto const start = cr::steady_clock::now();
  VSOMEIP_DEBUG
      << "Waiting (blocking) indefinitely on network interface (signal=" << path
      << ")";
  auto const r = waitfor(path.data(), delay_ms, poll_ms);
  auto const end = cr::steady_clock::now();
  auto diff = end - start;
  if (0 == r) {
    VSOMEIP_DEBUG << "Waited (blocked) for network interface (signal=" << path
                  << ") for "
                  << cr::duration_cast<cr::milliseconds>(diff).count()
                  << " ms.";
    return true;
  } else {
    VSOMEIP_ERROR << "Timedout waiting for network interface (signal=" << path
                  << ") after "
                  << cr::duration_cast<cr::milliseconds>(diff).count()
                  << " ms: errno=" << errno << ", msg=" << strerror(errno);
    return false;
  }
#else
  // Omitting a non-QNX implementation for now.  The QNX implementation might be
  // modified to use pps in the future, similar to how the Linux implementation
  // uses Netlink..  A Linux implementation could also mirror what is above
  // using ionotify, but it would only be useful for testing - in practice it
  // would simple be a worse implementation than using netlink
  return true;
#endif
}

void simple_connector::start() {
  auto *const use_async_sd = getenv(VSOMEIP_ENV_USE_ASYNCHRONOUS_SD);
  if (!use_async_sd) {
    // If VSOMEIP_ENV_USE_ASYNCHRONOUS_SD is not set, so we will assume the
    // network is "ready" and proceed normally.
    network_ready_ = true;
    handler_(true, if_name_.data(), true);
    handler_(false, if_name_.data(), true);

    return;
  }

  wait_for_network_thread_ = std::thread([this]() {
#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
    {
      auto err = pthread_setname_np(wait_for_network_thread_.native_handle(),
                                    "wait_network");
      if (err) {
        VSOMEIP_ERROR << "Could not rename SD thread: " << errno << ":"
                      << std::strerror(errno);
      }
    }
#endif
    network_ready_ = wait_for_interface();

    handler_(true, if_name_.data(), network_ready_);
    handler_(false, if_name_.data(), network_ready_);
  });

  return;
}

} // namespace vsomeip_v3
