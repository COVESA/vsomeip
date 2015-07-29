// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/vsomeip.hpp>

static vsomeip::service_t service_id = 0x1111;
static vsomeip::instance_t service_instance_id = 0x2222;
static vsomeip::method_t service_method_id = 0x3333;

class hello_world_service {
public:
    // Get the vSomeIP runtime and
    // create a application via the runtime, we could pass the application name
    // here otherwise the name supplied via the VSOMEIP_APPLICATION_NAME
    // environment variable is used
    hello_world_service() :
                    rtm_(vsomeip::runtime::get()),
                    app_(rtm_->create_application())
    {
    }

    void init()
    {
        // init the application
        app_->init();

        // register a message handler callback for messages sent to our service
        app_->register_message_handler(service_id, service_instance_id,
                service_method_id,
                std::bind(&hello_world_service::on_message_cbk, this,
                        std::placeholders::_1));

        // register an event handler to get called back after registration at the
        // runtime was successful
        app_->register_event_handler(
                std::bind(&hello_world_service::on_event_cbk, this,
                        std::placeholders::_1));
    }

    void start()
    {
        // start the application and wait for the on_event callback to be called
        // this method only returns when app_->stop() is called
        app_->start();
    }

    void stop()
    {
        // Stop offering the service
        app_->stop_offer_service(service_id, service_instance_id);
        // unregister the event handler
        app_->unregister_event_handler();
        // unregister the message handler
        app_->unregister_message_handler(service_id, service_instance_id,
                service_method_id);
        // shutdown the application
        app_->stop();
    }

    void on_event_cbk(vsomeip::event_type_e _event)
    {
        if(_event == vsomeip::event_type_e::ET_REGISTERED)
        {
            // we are registered at the runtime and can offer our service
            app_->offer_service(service_id, service_instance_id);
        }
    }

    void on_message_cbk(const std::shared_ptr<vsomeip::message> &_request)
    {
        // Create a response based upon the request
        std::shared_ptr<vsomeip::message> resp = rtm_->create_response(_request);

        // Construct string to send back
        std::string str("Hello ");
        str.append(
                reinterpret_cast<const char*>(_request->get_payload()->get_data()),
                0, _request->get_payload()->get_length());

        // Create a payload which will be sent back to the client
        std::shared_ptr<vsomeip::payload> resp_pl = rtm_->create_payload();
        std::vector<vsomeip::byte_t> pl_data(str.begin(), str.end());
        resp_pl->set_data(pl_data);
        resp->set_payload(resp_pl);

        // Send the response back
        app_->send(resp, true);
        // we're finished stop now
        stop();
    }

private:
    std::shared_ptr<vsomeip::runtime> rtm_;
    std::shared_ptr<vsomeip::application> app_;
};

int main(int argc, char **argv)
{
    hello_world_service hw_srv;
    hw_srv.init();
    hw_srv.start();
    return 0;
}
