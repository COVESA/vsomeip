//
// message_base_impl.cpp
//
// Date: 	Nov 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <vsomeip/impl/byteorder_impl.hpp>
#include <vsomeip/impl/constants_impl.hpp>
#include <vsomeip/impl/message_impl.hpp>

namespace vsomeip {

message_base_impl::message_base_impl() {
	header_.set_owner(this);
}

message_base_impl::~message_base_impl() {
};

// header interface
message_id message_base_impl::get_message_id() const {
	return VSOMEIP_WORDS_TO_LONG(header_.service_id_, header_.method_id_);
}

void message_base_impl::set_message_id(message_id _id) {
	header_.service_id_ = VSOMEIP_LONG_WORD0(_id);
	header_.method_id_ = VSOMEIP_LONG_WORD1(_id);
}

service_id message_base_impl::get_service_id() const {
	return header_.service_id_;
}

void message_base_impl::set_service_id(service_id _id) {
	header_.service_id_ = _id;
}

method_id message_base_impl::get_method_id() const {
	return header_.method_id_;
}

void message_base_impl::set_method_id(method_id _id) {
	header_.method_id_ = _id;
}

request_id message_base_impl::get_request_id() const {
	return VSOMEIP_WORDS_TO_LONG(header_.client_id_, header_.session_id_);
}

void message_base_impl::set_request_id(message_id _id) {
	header_.client_id_ = VSOMEIP_LONG_WORD0(_id);
	header_.session_id_ = VSOMEIP_LONG_WORD1(_id);
}

client_id message_base_impl::get_client_id() const {
	return header_.client_id_;
}

void message_base_impl::set_client_id(client_id _id) {
	header_.client_id_ = _id;
}

session_id message_base_impl::get_session_id() const {
	return header_.session_id_;
}

void message_base_impl::set_session_id(session_id _id) {
	header_.session_id_ = _id;
}

protocol_version message_base_impl::get_protocol_version() const {
	return header_.protocol_version_;
}

void message_base_impl::set_protocol_version(protocol_version _id) {
	header_.protocol_version_ = _id;
}

interface_version message_base_impl::get_interface_version() const {
	return header_.interface_version_;
}

void message_base_impl::set_interface_version(protocol_version _id) {
	header_.interface_version_ = _id;
}

message_type message_base_impl::get_message_type() const {
	return header_.message_type_;
}

void message_base_impl::set_message_type(message_type _type) {
	header_.message_type_ = _type;
}

return_code message_base_impl::get_return_code() const {
	return header_.return_code_;
}

void message_base_impl::set_return_code(return_code _code) {
	header_.return_code_ = _code;
}

} // namespace vsomeip
