// Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/plugin.hpp>
#include <byteswap.h>

#include "../include/diagnosis_plugin_mgu.hpp"

VSOMEIP_PLUGIN(vsomeip::mgu::diagnosis_plugin_mgu)

namespace vsomeip {
namespace mgu {

const uint32_t diagnosis_plugin_mgu::error_ok_ = 256;
const uint32_t diagnosis_plugin_mgu::error_out_of_range_ = 257;
const uint32_t diagnosis_plugin_mgu::error_communication_error_ = 576;
const uint8_t diagnosis_plugin_mgu::communication_type_mask_ = 2;
const uint8_t diagnosis_plugin_mgu::expected_payload_length_ = sizeof(uint32_t) + sizeof(uint8_t);
const uint8_t diagnosis_plugin_mgu::communication_type_payload_offset_ = sizeof(uint32_t);
const vsomeip::service_t diagnosis_plugin_mgu::diag_job_service_id_ = 0xFE9F;
const vsomeip::instance_t diagnosis_plugin_mgu::diag_job_instance_id_ = 0x80;
const vsomeip::major_version_t diagnosis_plugin_mgu::diag_job_major_ = 0x0;
const vsomeip::minor_version_t diagnosis_plugin_mgu::diag_job_minor_ = 0x1;
const vsomeip::method_t diagnosis_plugin_mgu::diag_job_rx_on_tx_on_ = 0x1;
const vsomeip::method_t diagnosis_plugin_mgu::diag_job_rx_on_tx_off_ = 0x2;

diagnosis_plugin_mgu::diagnosis_plugin_mgu()
    : plugin_impl("diagnosis job plug-in mgu",
                  VSOMEIP_APPLICATION_PLUGIN_VERSION,
                  vsomeip::plugin_type_e::APPLICATION_PLUGIN) {
}

void diagnosis_plugin_mgu::on_application_state_change(const std::string _application_name,
                            const vsomeip::application_plugin_state_e _app_state) {

    std::shared_ptr<vsomeip::application> application = nullptr;
    {
        std::lock_guard<std::mutex> lock(application_name_mutex_);
        application_name_ = _application_name;
        application = runtime::get()->get_application(application_name_);
    }
    if (application) {
        switch (_app_state) {
            case vsomeip::application_plugin_state_e::STATE_INITIALIZED:
            if (application->is_routing()) {
                application->register_state_handler(
                        std::bind(&diagnosis_plugin_mgu::on_state,
                            this, std::placeholders::_1));
                application->register_message_handler(diag_job_service_id_,
                        diag_job_instance_id_, vsomeip::ANY_METHOD,
                        std::bind(&diagnosis_plugin_mgu::on_message,
                            this, std::placeholders::_1));
            }
            break;

            case vsomeip::application_plugin_state_e::STATE_STARTED:
            break;
            case vsomeip::application_plugin_state_e::STATE_STOPPED:
            if (application->is_routing()) {
                application->stop_offer_service(
                        diag_job_service_id_, diag_job_instance_id_, diag_job_major_, diag_job_minor_);
                application->unregister_message_handler(
                        diag_job_service_id_, diag_job_instance_id_, vsomeip::ANY_METHOD);
                application->unregister_state_handler();
            }
            break;
        }
    }
}

void diagnosis_plugin_mgu::on_message(const std::shared_ptr<vsomeip::message> &_request) {
    std::shared_ptr<vsomeip::application> application = nullptr;
    {
        std::lock_guard<std::mutex> lock(application_name_mutex_);
        application = runtime::get()->get_application(application_name_);
    }
    if (application && application->is_routing()) {
        vsomeip::routing_state_e state = vsomeip::routing_state_e::RS_UNKNOWN;
        switch (_request->get_method()) {
            case diag_job_rx_on_tx_on_:
                state = vsomeip::routing_state_e::RS_RUNNING;
                break;
            case diag_job_rx_on_tx_off_:
                state = vsomeip::routing_state_e::RS_DIAGNOSIS;
                break;
            default:
                break;
        }
        // Only reply if one of the 2 well known message are called!
        if (state != vsomeip::routing_state_e::RS_UNKNOWN) {
            uint32_t error;
            auto payload = _request->get_payload();
            // Check for correct data length
            if (payload->get_length() == expected_payload_length_) {
                // Check if bit 1 is set
                if (payload->get_data()[communication_type_payload_offset_]
                                    & communication_type_mask_) {
                    application->set_routing_state(state);
                    error = bswap_32(error_ok_);
                } else {
                    error = bswap_32(error_out_of_range_);
                }
            } else {
                error = bswap_32(error_communication_error_);
            }

            auto its_payload = vsomeip::runtime::get()->create_payload();
            std::vector<byte_t> its_payload_data;
            // Set error return type
            for (uint8_t i = 0; i < sizeof(error); ++i) {
                its_payload_data.push_back(reinterpret_cast<const byte_t*>(&error)[i]);
            }

            // Copy handle from request-payload!
            for (uint8_t i = 0; i < sizeof(uint32_t); ++i) {
                its_payload_data.push_back(payload->get_data()[i]);
            }

            // Create & send response
            its_payload->set_data(its_payload_data);
            std::shared_ptr<vsomeip::message> its_response =
                    vsomeip::runtime::get()->create_response(_request);
            its_response->set_payload(its_payload);
            application->send(its_response, true);
        }
    }
}

void diagnosis_plugin_mgu::on_state(vsomeip::state_type_e _state) {
    std::shared_ptr<vsomeip::application> application = nullptr;
    {
        std::lock_guard<std::mutex> lock(application_name_mutex_);
        application = runtime::get()->get_application(application_name_);
    }
    if (application && _state == vsomeip::state_type_e::ST_REGISTERED) {
        application->offer_service(diag_job_service_id_, diag_job_instance_id_,
                diag_job_major_, diag_job_minor_);
    }
}

} // namespace mgu
} // namespace vsomeip
