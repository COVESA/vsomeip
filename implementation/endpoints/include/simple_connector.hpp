#ifndef VSOMEIP_V3_SIMPLE_CONNECTOR_HPP_
#define VSOMEIP_V3_SIMPLE_CONNECTOR_HPP_

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

#include <boost/enable_shared_from_this.hpp>

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif

namespace vsomeip_v3 {

using simple_net_ready_handler_t =
    std::function<void(bool,        // true = is interface, false = is route
                       std::string, // interface name
                       bool)        // available?
                  >;

/** \brief simple network connector, designed for use on QNX.
 *
 * This class is a simple network connector that is designed to be used on QNX
 * to detect when the network interface is available.  This is done by waiting
 * for (`waitfor`) the presence of VSOMEIP_NETWORK_INT_READY_FILE which is
 * created externally of vsomeip.  An alternative implementation would be to use
 * a service like PPS.
 */
class simple_connector : public std::enable_shared_from_this<simple_connector> {
public:
  simple_connector();
  ~simple_connector();

  void
  register_net_if_changes_handler(const simple_net_ready_handler_t &_handler);
  void unregister_net_if_changes_handler();

  void start();
  void stop();

private:
  bool wait_for_interface();

  static constexpr std::string_view if_name_ = "emac0";
  std::atomic<bool> network_ready_;

  std::thread wait_for_network_thread_;

  simple_net_ready_handler_t handler_{nullptr};
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_SIMPLE_CONNECTOR_HPP_
