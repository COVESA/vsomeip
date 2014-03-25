#ifndef BOOST_EXT_ASIO_MQ_BASIC_MESSAGE_QUEUE_SERVICE_HPP
#define BOOST_EXT_ASIO_MQ_BASIC_MESSAGE_QUEUE_SERVICE_HPP

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>
#include <boost_ext/asio/mq/message_queue_adapter.hpp>

namespace boost_ext {
namespace asio {
namespace mq {

template <typename MessageQueueImpl = message_queue_adapter>
class basic_message_queue_service
    : public boost::asio::io_service::service {

public:
    static boost::asio::io_service::id id;

    explicit basic_message_queue_service(boost::asio::io_service &io_service)
        : boost::asio::io_service::service(io_service),
          async_work_(new boost::asio::io_service::work(async_io_service_)),
          async_thread_(boost::bind(&boost::asio::io_service::run, &async_io_service_)) {
    }

    typedef boost::shared_ptr<MessageQueueImpl> implementation_type;

    void construct(implementation_type &impl) {
    	impl.reset(new MessageQueueImpl);
    }

    void destroy(implementation_type &impl) {
    	impl.reset();
    }

    const std::string & get_name(const implementation_type &impl) const {
     	return impl->get_name();
    }

    void create(implementation_type &impl, const std::string &name, std::size_t max_num_msgs, std::size_t max_msg_size) {
        boost::system::error_code ec;
        impl->create(name, max_num_msgs, max_msg_size, ec);
        boost::asio::detail::throw_error(ec);
    }

    template <typename Handler>
    class create_operation {
    public:
        create_operation(implementation_type &impl, boost::asio::io_service &io_service, const std::string &name, std::size_t max_num_msgs, std::size_t max_msg_size, Handler handler)
            : impl_(impl), io_service_(io_service), work_(io_service), name_(name), max_num_msgs_(max_num_msgs), max_msg_size_(max_msg_size), handler_(handler) {
        }

        void operator()() const {
            implementation_type impl = impl_.lock();
            if (impl) {
                boost::system::error_code ec;
                impl->create(name_, max_num_msgs_, max_msg_size_, ec);
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec));
            } else {
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, boost::asio::error::operation_aborted));
            }
        }

    private:
        boost::weak_ptr<MessageQueueImpl> impl_;
        boost::asio::io_service &io_service_;
        boost::asio::io_service::work work_;
        std::string name_;
        std::size_t max_num_msgs_;
        std::size_t max_msg_size_;
        Handler handler_;
    };

    template <typename Handler>
    void async_create(implementation_type &impl, const std::string &name, std::size_t max_num_msgs, std::size_t max_msg_size, Handler handler) {
        this->async_io_service_.post(create_operation<Handler>(impl, this->get_io_service(), name, max_num_msgs, max_msg_size, handler));
    }

    void open(implementation_type &impl, const std::string &name) {
        boost::system::error_code ec;
        impl->open(name, ec);
        boost::asio::detail::throw_error(ec);
    }

    template <typename Handler>
    class open_operation {
    public:
        open_operation(implementation_type &impl, boost::asio::io_service &io_service, const std::string &name, Handler handler)
            : impl_(impl), io_service_(io_service), work_(io_service), name_(name), handler_(handler) {
        }

        void operator()() const {
            implementation_type impl = impl_.lock();
            if (impl) {
                boost::system::error_code ec;
                impl->open(name_, ec);
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec));
            } else {
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, boost::asio::error::operation_aborted));
            }
        }

    private:
        boost::weak_ptr<MessageQueueImpl> impl_;
        boost::asio::io_service &io_service_;
        boost::asio::io_service::work work_;
        std::string name_;
        Handler handler_;
    };

    template <typename Handler>
    void async_open(implementation_type &impl, const std::string &name, Handler handler) {
        this->async_io_service_.post(open_operation<Handler>(impl, this->get_io_service(), name, handler));
    }

    void close(implementation_type &impl) {
        boost::system::error_code ec;
        impl->close(ec);
        boost::asio::detail::throw_error(ec);
    }

    template <typename Handler>
    class close_operation {
    public:
        close_operation(implementation_type &impl, boost::asio::io_service &io_service, Handler handler)
            : impl_(impl), io_service_(io_service), work_(io_service), handler_(handler) {
        }

        void operator()() const {
            implementation_type impl = impl_.lock();
            if (impl) {
                boost::system::error_code ec;
                impl->close(ec);
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec));
            } else {
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, boost::asio::error::operation_aborted));
            }
        }

    private:
        boost::weak_ptr<MessageQueueImpl> impl_;
        boost::asio::io_service &io_service_;
        boost::asio::io_service::work work_;
        Handler handler_;
    };

	template <typename Handler>
	void async_close(implementation_type &impl, Handler handler) {
		this->async_io_service_.post(close_operation<Handler>(impl, this->get_io_service(), handler));
	}

	void unlink(implementation_type &impl) {
		boost::system::error_code ec;
		impl->unlink(ec);
		boost::asio::detail::throw_error(ec);
	}

    template <typename Handler>
	class unlink_operation {
	public:
    	unlink_operation(implementation_type &impl, boost::asio::io_service &io_service, Handler handler)
			: impl_(impl), io_service_(io_service), work_(io_service), handler_(handler) {
		}

		void operator()() const {
			implementation_type impl = impl_.lock();
			if (impl) {
				boost::system::error_code ec;
				impl->unlink(ec);
				this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec));
			} else {
				this->io_service_.post(boost::asio::detail::bind_handler(handler_, boost::asio::error::operation_aborted));
			}
		}

	private:
		boost::weak_ptr<MessageQueueImpl> impl_;
		boost::asio::io_service &io_service_;
		boost::asio::io_service::work work_;
		Handler handler_;
	};

    template <typename Handler>
    void async_unlink(implementation_type &impl, Handler handler) {
        this->async_io_service_.post(unlink_operation<Handler>(impl, this->get_io_service(), handler));
    }

    void send(implementation_type &impl, const void *buffer, std::size_t buffer_size, unsigned int priority) {
        boost::system::error_code ec;
        impl->send(buffer, buffer_size, priority, ec);
        boost::asio::detail::throw_error(ec);
    }

    template <typename Handler>
    class send_operation {
    public:
        send_operation(implementation_type &impl, boost::asio::io_service &io_service, const void *buffer, std::size_t buffer_size, unsigned int priority, Handler handler)
            : impl_(impl), io_service_(io_service), work_(io_service), buffer_(buffer), buffer_size_(buffer_size), priority_(priority), handler_(handler) {
        }

        void operator()() const {
            implementation_type impl = impl_.lock();
            if (impl) {
                boost::system::error_code ec;
                impl->send(buffer_, buffer_size_, priority_, ec);
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec));
            } else {
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, boost::asio::error::operation_aborted));
            }
        }

    private:
        boost::weak_ptr<MessageQueueImpl> impl_;
        boost::asio::io_service &io_service_;
        boost::asio::io_service::work work_;
        const void *buffer_;
        std::size_t buffer_size_;
        unsigned int priority_;
        Handler handler_;
    };

    template <typename Handler>
    void async_send(implementation_type &impl, const void *buffer, std::size_t buffer_size, unsigned int priority, Handler handler) {
        this->async_io_service_.post(send_operation<Handler>(impl, this->get_io_service(), buffer, buffer_size, priority, handler));
    }

    void receive(implementation_type &impl, void *buffer, std::size_t buffer_size, std::size_t &received_size, unsigned int &priority) {
        boost::system::error_code ec;
        impl->receive(buffer, buffer_size, priority, ec);
        boost::asio::detail::throw_error(ec);
    }

    template <typename Handler>
    class receive_operation {
    public:
        receive_operation(implementation_type &impl, boost::asio::io_service &io_service, void *buffer, std::size_t buffer_size, Handler handler)
            : impl_(impl), io_service_(io_service), work_(io_service), buffer_(buffer), buffer_size_(buffer_size), received_size_(0), priority_(0), handler_(handler) {
        }

        void operator()() {
            implementation_type impl = impl_.lock();
            if (impl) {
                boost::system::error_code ec;
                impl->receive(buffer_, buffer_size_, received_size_, priority_, ec);
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, ec, received_size_, priority_));
            } else {
                this->io_service_.post(boost::asio::detail::bind_handler(handler_, boost::asio::error::operation_aborted, received_size_, priority_));
            }
        }

    private:
        boost::weak_ptr<MessageQueueImpl> impl_;
        boost::asio::io_service &io_service_;
        boost::asio::io_service::work work_;
        void *buffer_;
        std::size_t buffer_size_;
        std::size_t received_size_;
        unsigned int priority_;
        Handler handler_;
    };

    template <typename Handler>
    void async_receive(implementation_type &impl, void *buffer, std::size_t buffer_size, Handler handler) {
        this->async_io_service_.post(receive_operation<Handler>(impl, this->get_io_service(), buffer, buffer_size, handler));
    }

private:
    void shutdown_service() {
        async_work_.reset();
        async_io_service_.stop();
        async_thread_.join();
    }

    boost::asio::io_service async_io_service_;
    boost::scoped_ptr<boost::asio::io_service::work> async_work_;
    boost::thread async_thread_;
};

template <typename MessageQueueImpl>
boost::asio::io_service::id basic_message_queue_service<MessageQueueImpl>::id;

} // mq
} // asio
} // boost_ext

#endif // BOOST_EXT_ASIO_MQ_BASIC_MESSAGE_QUEUE_SERVICE_HPP
