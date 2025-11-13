// BadServiceServiceIDStubImpl.hpp
#ifndef BADSERVICESERVICEIDSTUBIMPL_H_
#define BADSERVICESERVICEIDSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v0/commonapi/examples/BadServiceServiceIDStubDefault.hpp>

class BadServiceServiceIDStubImpl: public v0_1::commonapi::examples::BadServiceServiceIDStubDefault
{
private:
    /* data */
public:
    BadServiceServiceIDStubImpl(/* args */);
    virtual ~BadServiceServiceIDStubImpl();
    virtual void sayHello(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _name, sayHelloReply_t _reply);

};

// BadServiceServiceIDStubImpl::BadServiceServiceIDStubImpl(/* args */)
// {
// }

// BadServiceServiceIDStubImpl::~BadServiceServiceIDStubImpl()
// {
// }


#endif /* BADSERVICESERVICEIDSTUBIMPL_H_*/
