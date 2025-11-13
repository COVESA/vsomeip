#include <csignal>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

#include <vsomeip/vsomeip.hpp>
#include "sample-ids.hpp"

class ServiceClient {
public:
    ServiceClient(std::shared_ptr<vsomeip::application> app,
                  vsomeip::service_t service_id,
                  vsomeip::instance_t instance_id,
                  bool subscribe_events = false)
    : app_(std::move(app)),
      service_id_(service_id),
      instance_id_(instance_id),
      subscribe_events_(subscribe_events),
      is_registered_(false),
      is_available_(false),
      running_(true),
      blocked_(false) {
        // Don't start sender_thread_ in constructor
    }

    bool init() {
        std::lock_guard<std::mutex> lk(mtx_);

        if (!app_->init()) {
            std::cerr << "[" << app_->get_name() << "] Couldn't initialize application. "
                      << "Check VSOMEIP_CONFIGURATION[_" << app_->get_name() << "] and JSON."
                      << std::endl;
            return false;
        }

        // Register state handler
        app_->register_state_handler(
            std::bind(&ServiceClient::on_state, this, std::placeholders::_1));

        // Register availability handler for the service
        app_->register_availability_handler(
            service_id_, instance_id_,
            std::bind(&ServiceClient::on_availability, this,
                     std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // Register message handler for responses
        app_->register_message_handler(
            service_id_, instance_id_, vsomeip::ANY_METHOD,
            std::bind(&ServiceClient::on_message, this, std::placeholders::_1));

        // Optional: register event handler if subscribing to events
        if (subscribe_events_) {
            app_->register_message_handler(
                service_id_, instance_id_, SAMPLE_EVENT_ID,
                std::bind(&ServiceClient::on_event, this, std::placeholders::_1));
        }

        std::cout << "[" << app_->get_name() << "] Initialized for service 0x"
                  << std::hex << service_id_ << "/0x" << instance_id_ << std::dec << std::endl;

        return true;
    }

    void start_async() {
        running_.store(true);
        
        // Start sender thread here, not in constructor
        sender_thread_ = std::thread([this]{ run(); });
        
        th_ = std::thread([this]{
            app_->start(); // blocks until stop()
        });
    }

    void stop() {
        if (!running_.load()) return; // Already stopped
        
        running_.store(false);

        // Unsubscribe and release service
        if (subscribe_events_ && is_available_) {
            app_->unsubscribe(service_id_, instance_id_, SAMPLE_EVENTGROUP_ID);
        }
        if (is_available_) {
            app_->release_service(service_id_, instance_id_);
        }

        // Signal sender thread to exit
        blocked_.store(true);
        cv_.notify_all();

        // Join sender thread before stopping app
        if (sender_thread_.joinable()) {
            sender_thread_.join();
        }

        // Now stop the app
        app_->clear_all_handler();
        app_->stop();
    }

    void join() {
        if (th_.joinable()) th_.join();
    }

private:
    void on_state(vsomeip::state_type_e st) {
        const bool reg = (st == vsomeip::state_type_e::ST_REGISTERED);
        std::cout << "[" << app_->get_name() << "] is " << (reg ? "registered" : "deregistered")
                  << std::endl;

        if (reg && !is_registered_) {
            is_registered_ = true;

            // Request the service
            std::cout << "[" << app_->get_name() << "] Requesting service 0x"
                      << std::hex << service_id_ << "/0x" << instance_id_ << std::dec << std::endl;
            app_->request_service(service_id_, instance_id_);

            blocked_.store(true);
            cv_.notify_all();
        } else if (!reg) {
            is_registered_ = false;
        }
    }

    void on_availability(vsomeip::service_t service, vsomeip::instance_t instance, bool available) {
        std::cout << "[" << app_->get_name() << "] Service 0x" << std::hex << service
                  << "/0x" << instance << " is "
                  << (available ? "AVAILABLE" : "NOT available") << std::dec << std::endl;

        if (service == service_id_ && instance == instance_id_) {
            is_available_ = available;

            if (available && subscribe_events_) {
                // Subscribe to eventgroup
                std::cout << "[" << app_->get_name() << "] Subscribing to eventgroup 0x"
                          << std::hex << SAMPLE_EVENTGROUP_ID << std::dec << std::endl;

                app_->subscribe(service_id_, instance_id_, SAMPLE_EVENTGROUP_ID);
            }

            if (available) {
                cv_.notify_all(); // Wake up sender thread
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& resp) {
        if (resp->get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
            std::cout << "[" << app_->get_name() << "] Received RESPONSE for method 0x"
                      << std::hex << resp->get_method()
                      << " from service 0x" << resp->get_service()
                      << "/0x" << resp->get_instance()
                      << " session 0x" << resp->get_session() << std::dec;

            auto payload = resp->get_payload();
            if (payload && payload->get_length() > 0) {
                std::cout << " payload length: " << payload->get_length();
                
                // Print first few bytes
                const vsomeip::byte_t* data = payload->get_data();
                std::cout << " [";
                for (size_t i = 0; i < std::min(size_t(8), size_t(payload->get_length())); ++i) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(data[i]) << " ";
                }
                std::cout << "...]" << std::dec;
            }
            std::cout << std::endl;
        }
    }

    void on_event(const std::shared_ptr<vsomeip::message>& notif) {
        std::cout << "[" << app_->get_name() << "] Received EVENT 0x"
                  << std::hex << notif->get_method()
                  << " from service 0x" << notif->get_service()
                  << "/0x" << notif->get_instance() << std::dec;

        auto payload = notif->get_payload();
        if (payload && payload->get_length() > 0) {
            std::cout << " payload: [";
            const vsomeip::byte_t* data = payload->get_data();
            for (size_t i = 0; i < payload->get_length(); ++i) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(data[i]) << " ";
            }
            std::cout << "]" << std::dec;
        }
        std::cout << std::endl;
    }

    void run() {
        // Wait until registered and service available
        std::unique_lock<std::mutex> lk(mtx_);
        while (!blocked_.load() && running_.load()) {
            cv_.wait(lk);
        }
        lk.unlock();

        // Send requests periodically
        while (running_.load()) {
            if (!is_available_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Send SAMPLE_METHOD_ID request
            send_request(SAMPLE_METHOD_ID, {0x01, 0x02, 0x03, 0x04});
            
            for (int i = 0; i < 50 && running_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!running_.load()) break;

            // Send GET request
            send_request(SAMPLE_GET_METHOD_ID, {});
            
            for (int i = 0; i < 50 && running_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!running_.load()) break;

            // Send SET request
            send_request(SAMPLE_SET_METHOD_ID, {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF});
            
            for (int i = 0; i < 50 && running_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void send_request(vsomeip::method_t method, const std::vector<vsomeip::byte_t>& data) {
        auto req = vsomeip::runtime::get()->create_request(false); // false = unreliable (UDP)
        req->set_service(service_id_);
        req->set_instance(instance_id_);
        req->set_method(method);

        if (!data.empty()) {
            auto payload = vsomeip::runtime::get()->create_payload();
            payload->set_data(data);
            req->set_payload(payload);
        }

        std::cout << "[" << app_->get_name() << "] Sending request method 0x"
                  << std::hex << method << " to service 0x" << service_id_
                  << "/0x" << instance_id_ << std::dec << std::endl;

        app_->send(req);
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    vsomeip::service_t  service_id_;
    vsomeip::instance_t instance_id_;
    bool subscribe_events_;

    bool is_registered_;
    bool is_available_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> blocked_{false};
    std::atomic<bool> running_{true};

    std::thread th_;           // app_->start() thread
    std::thread sender_thread_; // request sender thread
};

// Global state
static std::vector<std::shared_ptr<vsomeip::application>> g_apps;
static std::vector<std::unique_ptr<ServiceClient>> g_clients;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cerr << "\n[main] Signal caught, stopping clients..." << std::endl;
        for (auto& client : g_clients) {
            client->stop();
        }
    }
}

int main(int argc, char** argv) {
    bool subscribe_events = false;

    for (int i = 1; i < argc; ++i) {
        if (std::string("--subscribe-events") == argv[i]) {
            subscribe_events = true;
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    auto rt = vsomeip::runtime::get();

    // Create client apps
    auto app170 = rt->create_application("CLGW170");
    auto app180 = rt->create_application("CLGW180");
    
    if (!app170 || !app180) {
        std::cerr << "[main] Failed to create applications" << std::endl;
        return 1;
    }

    g_apps.push_back(app170);
    g_apps.push_back(app180);

    // Wrap client logic
    // CLGW170: targets service 0x1234/0x5678 (SRVGW170)
    // CLGW180: targets service 0x1235/0x5679 (SRVGW180)
    g_clients.push_back(std::unique_ptr<ServiceClient>(
        new ServiceClient(app170, SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, subscribe_events)));
    
    g_clients.push_back(std::unique_ptr<ServiceClient>(
        new ServiceClient(app180, SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID + 1, subscribe_events)));

    // Init all clients
    for (auto& client : g_clients) {
        if (!client->init()) {
            std::cerr << "[main] Init failed for client app" << std::endl;
            return 1;
        }
    }

    // Start client apps
    for (auto& client : g_clients) {
        client->start_async();
    }

    // Wait for all to finish
    for (auto& client : g_clients) {
        client->join();
    }

    return 0;
}