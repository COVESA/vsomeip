#ifndef BOOST_EXT_ASIO_MQ_HPP
#define BOOST_EXT_ASIO_MQ_HPP

#include <boost_ext/asio/mq/basic_message_queue.hpp>
#include <boost_ext/asio/mq/basic_message_queue_service.hpp>

namespace boost_ext {
namespace asio {

typedef mq::basic_message_queue< mq::basic_message_queue_service<> > message_queue;

} // namespace asio
} // namespace boost

#endif // BOOST_ASIO_MQ_HPP
