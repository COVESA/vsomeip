// Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_DIAGNOSIS_PLUGIN_MGU_HPP
#define VSOMEIP_DIAGNOSIS_PLUGIN_MGU_HPP

#include <mutex>

#include <vsomeip/application.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/plugins/application_plugin.hpp>

namespace vsomeip {
namespace mgu {

class diagnosis_plugin_mgu
        : public application_plugin,
          public plugin_impl<diagnosis_plugin_mgu>,
          public std::enable_shared_from_this<diagnosis_plugin_mgu> {
public:
        diagnosis_plugin_mgu();
        VSOMEIP_EXPORT void on_application_state_change(
                            const std::string _application_name,
                            const application_plugin_state_e _app_state);
private:
        void on_message(const std::shared_ptr<vsomeip::message> &_request);
        void on_state(vsomeip::state_type_e _state);

        std::string application_name_;
        std::mutex application_name_mutex_;

        static const uint32_t error_ok_;
        static const uint32_t error_out_of_range_;
        static const uint32_t error_communication_error_;
        static const uint8_t communication_type_mask_;
        static const uint8_t expected_payload_length_;
        static const uint8_t communication_type_payload_offset_;
        static const vsomeip::service_t diag_job_service_id_;
        static const vsomeip::instance_t diag_job_instance_id_;
        static const vsomeip::major_version_t diag_job_major_;
        static const vsomeip::minor_version_t diag_job_minor_;
        static const vsomeip::method_t diag_job_rx_on_tx_on_;
        static const vsomeip::method_t diag_job_rx_on_tx_off_;
};

} // namespace mgu
} // namespace vsomeip

#endif // VSOMEIP_DIAGNOSIS_PLUGIN_MGU_HPP
