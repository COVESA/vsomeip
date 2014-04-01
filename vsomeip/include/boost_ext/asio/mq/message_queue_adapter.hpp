#ifndef BOOST_EXT_ASIO_MQ_MESSAGE_QUEUE_ADAPTER_HPP
#define BOOST_EXT_ASIO_MQ_MESSAGE_QUEUE_ADAPTER_HPP

#include <cstddef>
#include <string>

#include <mqueue.h>

#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>

namespace boost_ext {
namespace asio {
namespace mq {

#if defined(LINUX)
const mqd_t INVALID = -1;
#elif defined(FREEBSD)
const mqd_t INVALID(reinterpret_cast< mqd_t >(-1));
#else
#error "OS undefined (only Linux and FreeBSD are currently supported)"
#endif

class message_queue_adapter {
public:
	message_queue_adapter() : id_(INVALID) {
		timeout_.tv_sec = 0;
		timeout_.tv_nsec = 100000000;
    }

    ~message_queue_adapter() {
    }

    void create(const std::string &name, std::size_t max_num_msgs, std::size_t max_msg_size, boost::system::error_code &ec) {
        struct mq_attr config;
    	config.mq_maxmsg = max_num_msgs;
    	config.mq_msgsize = max_msg_size;
    	name_ = name;

       	id_ = mq_open(name.c_str(), O_CREAT|O_RDWR, S_IRWXU|S_IRWXG, &config);
       	if (id_ != INVALID) {
       		ec = boost::system::error_code();
       	} else {
            ec = boost::asio::error::operation_aborted;
        }
    }

    void open(const std::string &name, boost::system::error_code &ec) {
    	name_ = name;
        id_ = mq_open(name.c_str(), O_WRONLY);
        if (id_ != INVALID) {
            ec = boost::system::error_code();
        } else {
            ec = boost::asio::error::operation_aborted;
        }
    }

    void close(boost::system::error_code &ec) {
        int e = mq_close(id_);
        if (e > 0) {
    		ec = boost::system::error_code();
        } else {
            ec = boost::asio::error::operation_aborted;
        }
    }

    void unlink(boost::system::error_code &ec) {
        int e = mq_unlink(name_.c_str());
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
            struct mq_attr config;
            e = mq_getattr(id_, &config);
            std::cout << "Queue ("
            		  << name_
            		  << ") state ["
            		  << config.mq_maxmsg
            		  << ", "
            		  << config.mq_msgsize
            		  << ", "
            		  << config.mq_curmsgs
            		  << ", "
            		  << config.mq_flags
            		  << "]" << std::endl;
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

    const std::string & get_name() const {
    	return name_;
    }

private:
    struct timespec timeout_;
    mqd_t id_;
    std::string name_;
};

} // namespace mq
} // namespace asio
} // namespace boost_ext

#endif // BOOST_EXT_ASIO_MQ_MESSAGE_QUEUE_ADAPTER_HPP
