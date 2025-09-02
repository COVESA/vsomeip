#include <gtest/gtest.h>
#include <vsomeip/internal/logger.hpp>
#include <vsomeip/vsomeip.hpp>
#include <thread>
#include <future>
#include "../../../implementation/utility/include/bithelper.hpp"

namespace multicast_group_test {

// Unique identifier of the test service.
constexpr vsomeip::service_t SERVICE_ID = 0x1111;

// Unique identifier of the test service instance.
constexpr vsomeip::instance_t INSTANCE_ID = 0x0001;

// Major version of the test service interface.
constexpr vsomeip::major_version_t MAJOR_VERSION = 0x1;

// Minor version of the test service interface.
constexpr vsomeip::minor_version_t MINOR_VERSION = 0x0;

// Unique identifier of the event offered by the service.
constexpr vsomeip::event_t EVENT_ID = 0x0001;

// Unique identifier of the event group to which the event belongs.
constexpr vsomeip::eventgroup_t EVENTGROUP_ID = 0x0001;

}
