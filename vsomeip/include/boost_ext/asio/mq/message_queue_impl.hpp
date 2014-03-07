#ifndef BOOST_EXT_ASIO_MQ_MESSAGE_QUEUE_IMPL
#define BOOST_EXT_ASIO_MQ_MESSAGE_QUEUE_IMPL

#include <cstddef>
#include <string>

#include <mqueue.h>

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

namespace boost_ext {
namespace asio {
namespace mq {

class message_queue_impl {
public:
    message_queue_impl() : id_(-1) {
    }

    ~message_queue_impl() {
    }

    void create(const std::string &name, std::size_t max_num_msgs, std::size_t max_msg_size, boost::system::error_code &ec) {
        struct mq_attr config;
    	config.mq_maxmsg = max_num_msgs;
    	config.mq_msgsize = max_msg_size;

       	id_ = mq_open(name.c_str(), O_CREAT|O_RDWR, S_IRWXU|S_IRWXG, &config);

       	if (id_ > -1) {
       		ec = boost::system::error_code();
       	} else {
            ec = boost::asio::error::operation_aborted;
        }
    }

    void open(const std::string &name, boost::system::error_code &ec) {
        id_ = mq_open(name.c_str(), O_WRONLY);

        if (id_ > -1) {
            ec = boost::system::error_code();
        } else {
            ec = boost::asio::error::operation_aborted;
        }
    }

    void close(const std::string &name, boost::system::error_code &ec) {
        int e = mq_close(id_);
        if (e > 0) {
    		ec = boost::system::error_code();
        } else {
            ec = boost::asio::error::operation_aborted;
        }
    }

    void send(const void *buffer, std::size_t buffer_size, unsigned int priority, boost::system::error_code &ec) {
    	int e = mq_send(id_, (const char*)buffer, buffer_size, priority);
    	if (e > -1) {
    	    ec = boost::system::error_code();
        } else {
            ec = boost::asio::error::operation_aborted;
        }
    }

    void receive(void *buffer, std::size_t buffer_size, std::size_t &received_size, unsigned int &priority, boost::system::error_code &ec) {
		ssize_t bytes = mq_receive(id_, (char *)buffer, buffer_size, &priority);
		if (bytes > -1) {
			received_size = static_cast<std::size_t>(bytes);
			ec = boost::system::error_code();
		} else {
			received_size = 0;
			ec = boost::asio::error::operation_aborted;
		}
	}

private:
    mqd_t id_;
};

} // namespace mq
} // namespace asio
} // namespace boost_ext

#endif // BOOST_EXT_ASIO_MQ_MESSAGE_QUEUE_IMPL
