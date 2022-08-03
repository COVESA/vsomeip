//
// detail/reactive_socket_recvmsg_op_ext.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2016-2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_REACTIVE_SOCKET_RECVMSG_OP_EXT_HPP
#define BOOST_ASIO_DETAIL_REACTIVE_SOCKET_RECVMSG_OP_EXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/buffer_sequence_adapter.hpp>
#include <boost/asio/detail/fenced_block.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_invoke_helpers.hpp>
#include <boost/asio/detail/handler_work.hpp>
#include <boost/asio/detail/memory.hpp>
#include <boost/asio/detail/reactor_op_ext.hpp>
#include <boost/asio/detail/socket_ops_ext.hpp>
#include <boost/asio/socket_base.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

template <typename MutableBufferSequence>
class reactive_socket_recvmsg_op_base_ext : public reactor_op_ext
{
public:
  reactive_socket_recvmsg_op_base_ext(const boost::system::error_code& success_ec,
	  socket_type socket, const MutableBufferSequence& buffers, socket_base::message_flags in_flags,
      socket_base::message_flags& out_flags, func_type complete_func)
    : reactor_op_ext(&reactive_socket_recvmsg_op_base_ext::do_perform, complete_func),
      socket_(socket),
      buffers_(buffers),
      in_flags_(in_flags),
      out_flags_(out_flags)
  {
  }

  static status do_perform(reactor_op* base)
  {
    reactive_socket_recvmsg_op_base_ext* o(
        static_cast<reactive_socket_recvmsg_op_base_ext*>(base));

    buffer_sequence_adapter<boost::asio::mutable_buffer,
        MutableBufferSequence> bufs(o->buffers_);

    status result = socket_ops::non_blocking_recvmsg(o->socket_,
        bufs.buffers(), bufs.count(),
        o->in_flags_, o->out_flags_,
        o->ec_, o->bytes_transferred_) ? done : not_done;

    BOOST_ASIO_HANDLER_REACTOR_OPERATION((*o, "non_blocking_recvmsg",
          o->ec_, o->bytes_transferred_));

    return result;
  }

private:
  socket_type socket_;
  MutableBufferSequence buffers_;
  socket_base::message_flags in_flags_;
  socket_base::message_flags& out_flags_;
};

template <typename MutableBufferSequence, typename Handler, typename IoExecutor>
class reactive_socket_recvmsg_op_ext :
  public reactive_socket_recvmsg_op_base_ext<MutableBufferSequence>
{
public:
  BOOST_ASIO_DEFINE_HANDLER_PTR(reactive_socket_recvmsg_op_ext);

  reactive_socket_recvmsg_op_ext(const boost::system::error_code& success_ec,
      socket_type socket, const MutableBufferSequence& buffers, socket_base::message_flags in_flags,
      socket_base::message_flags& out_flags, Handler& handler,
      const IoExecutor& io_ex)
    : reactive_socket_recvmsg_op_base_ext<MutableBufferSequence>(socket, buffers,
        in_flags, out_flags, &reactive_socket_recvmsg_op_ext::do_complete),
      handler_(BOOST_ASIO_MOVE_CAST(Handler)(handler)),
      work_(handler_, io_ex)
  {
  }

  static void do_complete(void* owner, operation* base,
      const boost::system::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    // Take ownership of the handler object.
    reactive_socket_recvmsg_op_ext* o(
        static_cast<reactive_socket_recvmsg_op_ext*>(base));
    ptr p = { boost::asio::detail::addressof(o->handler_), o, o };

    BOOST_ASIO_HANDLER_COMPLETION((*o));

    // Take ownership of the operation's outstanding work.
    handler_work<Handler, IoExecutor> w(
        BOOST_ASIO_MOVE_CAST2(handler_work<Handler, IoExecutor>)(
            o->work_));

    // Make a copy of the handler so that the memory can be deallocated before
    // the upcall is made. Even if we're not about to make an upcall, a
    // sub-object of the handler may be the true owner of the memory associated
    // with the handler. Consequently, a local copy of the handler is required
    // to ensure that any owning sub-object remains valid until after we have
    // deallocated the memory here.
    detail::binder3<Handler, boost::system::error_code, std::size_t, boost::asio::ip::address>
      handler(o->handler_, o->ec_, o->bytes_transferred_, o->da_);
    p.h = boost::asio::detail::addressof(handler.handler_);
    p.reset();

    // Make the upcall if required.
    if (owner)
    {
      fenced_block b(fenced_block::half);
      BOOST_ASIO_HANDLER_INVOCATION_BEGIN((handler.arg1_, handler.arg2_, handler.arg3_));
      w.complete(handler, handler.handler_);
      BOOST_ASIO_HANDLER_INVOCATION_END;
    }
  }

private:
  Handler handler_;
  handler_work<Handler, IoExecutor> work_;
};

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_REACTIVE_SOCKET_RECVMSG_OP_EXT_HPP
