#include <vsomeip/vsomeip.hpp>
#include <iostream>

int main() {
    auto app = vsomeip::runtime::get()->create_application("RoutingManagerClient");
    if (!app->init()) {
        std::cerr << "init failed\n";
        return 1;
    }
    app->register_state_handler([](vsomeip::state_type_e st) {
        std::cout << "routing-manager state: " << int(st) << std::endl;
    });
    app->start();
    return 0;
}