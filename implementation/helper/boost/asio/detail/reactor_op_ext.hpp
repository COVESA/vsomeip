//
// detail/reactor_op_ext.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_boost or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_REACTOR_OP_EXT_HPP
#define BOOST_ASIO_DETAIL_REACTOR_OP_EXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/reactor_op.hpp>

namespace boost {
namespace asio {
namespace detail {

class reactor_op_ext
  : public reactor_op
{
public:
  // The destination address
  boost::asio::ip::address da_;

  reactor_op_ext(perform_func_type perform_func, func_type complete_func)
    : reactor_op(perform_func, complete_func)
  {
  }
};

} // namespace detail
} // namespace asio
} // namespace boost

#endif // BOOST_ASIO_DETAIL_REACTOR_OP_EXT_HPP
