#ifndef BOOST_EXT_ASIO_MQ_BASIC_MESSAGE_QUEUE_HPP
#define BOOST_EXT_ASIO_MQ_BASIC_MESSAGE_QUEUE_HPP

#include <cstddef>
#include <string>
#include <boost/asio.hpp>

namespace boost_ext {
namespace asio {
namespace mq {

template <typename Service>
class basic_message_queue 
	: public boost::asio::basic_io_object<Service> {
public:
    explicit basic_message_queue(boost::asio::io_service &io_service)
        : boost::asio::basic_io_object<Service>(io_service) {
    }

    void create(const std::string &name, std::size_t max_num_msgs, std::size_t max_msg_size) {
        return this->service.create(this->implementation, name, max_num_msgs, max_msg_size);
    }

    template <typename Handler>
    void async_create(const std::string &name, std::size_t max_num_msgs, std::size_t max_msg_size, Handler handler) {
        return this->service.async_create(this->implementation, name, max_num_msgs, max_msg_size, handler);
    }

    void open(const std::string &name) {
        return this->service.open(this->implementation, name);
    }

    template <typename Handler>
    void async_open(const std::string &name, Handler handler) {
        this->service.async_open(this->implementation, name,  handler);
    } 

    void close() {
        return this->service.close(this->implementation);
    }

    template <typename Handler>
    void async_close(Handler handler) {
        this->service.async_close(this->implementation, handler);
    } 

    void unlink() {
        return this->service.unlink(this->implementation);
    }

    template <typename Handler>
    void async_unlink(Handler handler) {
        this->service.async_unlink(this->implementation, handler);
    }

    void send(const void *buffer, std::size_t buffer_size, unsigned int priority) {
        return this->service.send(this->implementation, buffer, buffer_size, priority);
    }

    template <typename Handler>
    void async_send(const void *buffer, std::size_t buffer_size, int priority, Handler handler) {
        return this->service.async_send(this->implementation, buffer, buffer_size, priority, handler);
    }

    void receive(void *buffer, std::size_t buffer_size, std::size_t &received_size, unsigned int &priority) {
        return this->service.receive(this->implementation, buffer, buffer_size, received_size, priority);
    }

    template <typename Handler>
    void async_receive(void *buffer, std::size_t buffer_size, Handler handler) {
        return this->service.async_receive(this->implementation, buffer, buffer_size, handler);
    }

    const std::string get_name() const {
    	return this->service.get_name(this->implementation);
    }
};

} // mq
} // asio
} // boost_ext

#endif // BOOST_EXT_ASIO_MQ_BASIC_MESSAGE_QUEUE

