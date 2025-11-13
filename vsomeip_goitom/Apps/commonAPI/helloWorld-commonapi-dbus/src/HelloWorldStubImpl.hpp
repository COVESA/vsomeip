
// HelloWorldStubImpl.hpp
#ifndef HELLOWORLDSTUBIMPL_H_
#define HELLOWORLDSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v0/commonapi/examples/HelloWorldStubDefault.hpp>

class HelloWorldStubImpl: public v0_1::commonapi::examples::HelloWorldStubDefault
{
private:
    /* data */
public:
    HelloWorldStubImpl(/* args */);
    virtual ~HelloWorldStubImpl();

    virtual void sayHello(const std::shared_ptr <CommonAPI::ClientId> _client, std::string _name, sayHelloReply_t _return);

};
#endif /* HELLOWORLDSTUBIMPL_H_ */

// HelloWorldStubImpl::HelloWorldStubImpl(/* args */)
// {
// }

// HelloWorldStubImpl::~HelloWorldStubImpl()
// {
// }
