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

class ServiceApp {
public:
    ServiceApp(std::shared_ptr<vsomeip::application> app,
               vsomeip::service_t service_id,
               vsomeip::instance_t instance_id,
               vsomeip::method_t method_id,
               bool offer_events = false,
               bool use_static_routing = true,
               bool toggle_offers = false)
    : app_(std::move(app)),
      service_id_(service_id),
      instance_id_(instance_id),
      method_id_(method_id),
      offer_events_(offer_events),
      is_registered_(false),
      use_static_routing_(use_static_routing),
      toggle_offers_(toggle_offers),
      blocked_(false),
      running_(true),
      stopped_(false) {}

    ~ServiceApp() {
        stop();
    }

    bool init() {
        if (!app_->init()) {
            std::cerr << "[" << app_->get_name() << "] Couldn't initialize application." << std::endl;
            return false;
        }

        app_->register_state_handler(
            std::bind(&ServiceApp::on_state, this, std::placeholders::_1));

        app_->register_message_handler(
            service_id_, instance_id_, method_id_,
            std::bind(&ServiceApp::on_message, this, std::placeholders::_1));

        app_->register_message_handler(
            service_id_, instance_id_, SAMPLE_GET_METHOD_ID,
            std::bind(&ServiceApp::on_get, this, std::placeholders::_1));

        app_->register_message_handler(
            service_id_, instance_id_, SAMPLE_SET_METHOD_ID,
            std::bind(&ServiceApp::on_set, this, std::placeholders::_1));

        return true;
    }

    void start_async() {
        running_.store(true);
        stopped_.store(false);
        
        offer_thread_ = std::thread([this]{ run(); });
        
        th_ = std::thread([this]{
            try {
                app_->start();
            } catch (const std::exception& e) {
                std::cerr << "[" << app_->get_name() << "] Exception in start: " << e.what() << std::endl;
            }
        });
    }

    void stop() {
        if (stopped_.load()) return;
        stopped_.store(true);
        running_.store(false);
        
        stop_offer();
        
        blocked_.store(true);
        cv_.notify_all();
        
        if (offer_thread_.joinable()) {
            offer_thread_.join();
        }
        
        try {
            app_->clear_all_handler();
            app_->stop();
        } catch (...) {
            // Ignore exceptions during shutdown
        }
    }

    void join() {
        if (th_.joinable()) {
            th_.join();
        }
    }

    void offer() {
        std::cout << "[" << app_->get_name() << "] Offering service 0x"
                  << std::hex << service_id_ << "/0x" << instance_id_ << std::dec << std::endl;
        
        app_->offer_service(service_id_, instance_id_);

        if (offer_events_) {
            std::cout << "[" << app_->get_name() << "] Offering event 0x"
                      << std::hex << SAMPLE_EVENT_ID << " in eventgroup 0x"
                      << SAMPLE_EVENTGROUP_ID << std::dec << std::endl;

            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(SAMPLE_EVENTGROUP_ID);
            app_->offer_event(service_id_, instance_id_, SAMPLE_EVENT_ID,
                             its_eventgroups, vsomeip::event_type_e::ET_EVENT,
                             std::chrono::milliseconds::zero(),
                             false, true, nullptr, vsomeip::reliability_type_e::RT_UNRELIABLE);
        }
    }

    void stop_offer() {
        std::cout << "[" << app_->get_name() << "] Stopping offer 0x"
                  << std::hex << service_id_ << "/0x" << instance_id_ << std::dec << std::endl;
        
        if (offer_events_) {
            app_->stop_offer_event(service_id_, instance_id_, SAMPLE_EVENT_ID);
        }
        app_->stop_offer_service(service_id_, instance_id_);
    }

private:
    void on_state(vsomeip::state_type_e st) {
        const bool reg = (st == vsomeip::state_type_e::ST_REGISTERED);
        std::cout << "[" << app_->get_name() << "] is " << (reg ? "registered" : "deregistered")
                  << std::endl;

        if (reg && !is_registered_) {
            is_registered_ = true;
            blocked_.store(true);
            cv_.notify_all();

            if (use_static_routing_) {
                offer();
            }
        } else if (!reg) {
            is_registered_ = false;
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& req) {
        std::cout << "[" << app_->get_name() << "] Received method 0x" << std::hex
                  << req->get_method() << " from client 0x" << req->get_client()
                  << " session 0x" << req->get_session() << std::dec << std::endl;

        auto resp = vsomeip::runtime::get()->create_response(req);
        auto payload = vsomeip::runtime::get()->create_payload();

        std::vector<vsomeip::byte_t> data;
        data.reserve(120);
        for (std::size_t i = 0; i < 120; ++i)
            data.push_back(static_cast<vsomeip::byte_t>(i % 256));

        payload->set_data(data);
        resp->set_payload(payload);

        app_->send(resp);
    }

    void on_get(const std::shared_ptr<vsomeip::message>& req) {
        std::cout << "[" << app_->get_name() << "] Received GET method 0x" << std::hex
                  << req->get_method() << " from client 0x" << req->get_client()
                  << " session 0x" << req->get_session() << std::dec << std::endl;

        auto resp = vsomeip::runtime::get()->create_response(req);
        auto payload = vsomeip::runtime::get()->create_payload();

        std::vector<vsomeip::byte_t> data = {0x01, 0x02, 0x03, 0x04};
        payload->set_data(data);
        resp->set_payload(payload);

        app_->send(resp);
    }

    void on_set(const std::shared_ptr<vsomeip::message>& req) {
        std::cout << "[" << app_->get_name() << "] Received SET method 0x" << std::hex
                  << req->get_method() << " from client 0x" << req->get_client()
                  << " session 0x" << req->get_session() << std::dec << std::endl;

        auto payload = req->get_payload();
        if (payload && payload->get_length() > 0) {
            std::cout << "[" << app_->get_name() << "] SET payload length: "
                      << payload->get_length() << std::endl;
        }

        auto resp = vsomeip::runtime::get()->create_response(req);
        app_->send(resp);
    }

    void run() {
        std::unique_lock<std::mutex> lk(mtx_);
        while (!blocked_.load() && running_.load()) {
            cv_.wait(lk);
        }
        lk.unlock();

        if (!toggle_offers_ || !running_.load()) return;

        bool is_offer = true;
        while (running_.load()) {
            for (int i = 0; i < 100 && running_.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (!running_.load()) break;
            
            if (is_offer) stop_offer();
            else offer();
            is_offer = !is_offer;
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    vsomeip::service_t  service_id_;
    vsomeip::instance_t instance_id_;
    vsomeip::method_t   method_id_;
    bool offer_events_;
    bool is_registered_;
    bool use_static_routing_;
    bool toggle_offers_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> blocked_{false};
    std::atomic<bool> running_{true};
    std::atomic<bool> stopped_{false};

    std::thread th_;
    std::thread offer_thread_;
};

static std::vector<std::shared_ptr<vsomeip::application>> g_apps;
static std::vector<std::unique_ptr<ServiceApp>> g_services;
static std::atomic<bool> g_shutdown{false};

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cerr << "\n[main] Signal caught, stopping..." << std::endl;
        g_shutdown.store(true);
    }
}

int main(int argc, char** argv) {
    bool toggle_offers = false;
    bool offer_events = false;

    for (int i = 1; i < argc; ++i) {
        if (std::string("--toggle-offers") == argv[i]) toggle_offers = true;
        if (std::string("--offer-events") == argv[i]) offer_events = true;
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    auto rt = vsomeip::runtime::get();

    auto app170 = rt->create_application("SRVGW170");
    auto app180 = rt->create_application("SRVGW180");
    
    if (!app170 || !app180) {
        std::cerr << "[main] Failed to create applications" << std::endl;
        return 1;
    }

    g_apps.push_back(app170);
    g_apps.push_back(app180);

    g_services.push_back(std::unique_ptr<ServiceApp>(
        new ServiceApp(app170, SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
                      offer_events, true, toggle_offers)));
    
    g_services.push_back(std::unique_ptr<ServiceApp>(
        new ServiceApp(app180, SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID + 1, SAMPLE_METHOD_ID,
                      offer_events, true, toggle_offers)));

    for (auto& svc : g_services) {
        if (!svc->init()) {
            std::cerr << "[main] Init failed for service" << std::endl;
            return 1;
        }
    }

    for (auto& svc : g_services) {
        svc->start_async();
    }

    // Wait for shutdown signal
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop in reverse order
    for (auto it = g_services.rbegin(); it != g_services.rend(); ++it) {
        (*it)->stop();
    }

    for (auto& svc : g_services) {
        svc->join();
    }

    // Small delay before final cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}