//
// vsomeip.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_HPP
#define VSOMEIP_HPP

#include <vsomeip/config.hpp>

/// Convenience header to facilitate usage
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/receiver.hpp>
#include <vsomeip/service.hpp>
#include <vsomeip/client.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>

#ifdef USE_VSOMEIP_STATISTICS
#include <vsomeip/statistics.hpp>
#endif

#endif /* VSOMEIP_HPP */
