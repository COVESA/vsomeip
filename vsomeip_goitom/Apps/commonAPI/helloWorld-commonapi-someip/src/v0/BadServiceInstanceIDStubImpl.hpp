
// BadServiceInstanceIDStubImpl.hpp

#ifndef BADSERVICEINSTANCEIDSTUBIMPL_H_
#define BADSERVICEINSTANCEIDSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v0/commonapi/examples/BadServiceInstanceIDStubDefault.hpp>

class BadServiceInstanceIDStubImpl: public v0_1::commonapi::examples::BadServiceInstanceIDStubDefault
{
private:
    /* data */
public:
    BadServiceInstanceIDStubImpl(/* args */);
    virtual ~BadServiceInstanceIDStubImpl();
    virtual void sayHello(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _name, sayHelloReply_t _reply);

};

// BadServiceInstanceIDStubImpl::BadServiceInstanceIDStubImpl(/* args */)
// {
// }

// BadServiceInstanceIDStubImpl::~BadServiceInstanceIDStubImpl()
// {
// }


#endif /* BADSERVICEINSTANCEIDSTUBIMPL_H_*/
