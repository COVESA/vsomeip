#include <chrono>
#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include "client.hpp"
#include "server.hpp"

TEST(internal_routing_disabled_acceptance_test, check_connectivity)
{
    server s;
    client c;

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(15s);

    std::cout
        << "[server]\n"
        << "\tevents: " << s.get_event_count() << '\n'
        << "\tmethod requests: " << s.get_method_request_count() << '\n'
        << "\tmethod responses: " << s.get_method_response_count() << '\n';

    std::cout
        << "[client]\n"
        << "\tevents: " << c.get_event_count() << '\n'
        << "\tmethod requests: " << c.get_method_request_count() << '\n'
        << "\tmethod responses: " << c.get_method_response_count() << '\n';

    EXPECT_EQ(s.get_event_count(), 10);
    EXPECT_EQ(s.get_method_request_count(), 0);
    EXPECT_EQ(s.get_method_response_count(), 0);

    EXPECT_EQ(c.get_event_count(), 0);
    EXPECT_EQ(c.get_method_request_count(), 0);
    EXPECT_EQ(c.get_method_response_count(), 0);
}

int main(int count, char** values)
{
    testing::InitGoogleTest(&count, values);
    return RUN_ALL_TESTS();
}
