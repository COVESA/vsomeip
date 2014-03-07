
#include <vsomeip_internal/daemon_impl.hpp>

namespace vsomeip {

daemon * daemon::get_instance() {
	return daemon_impl::get_instance();
}

} // namespace vsomeip

