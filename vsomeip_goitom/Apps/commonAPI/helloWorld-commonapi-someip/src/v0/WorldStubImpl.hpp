
// WorldStubImpl.hpp
#ifndef WORLDSTUBIMPL_H_
#define WORLDSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v0/commonapi/examples/WorldStubDefault.hpp>

class WorldStubImpl: public v0_1::commonapi::examples::WorldStubDefault
{
private:
    /* data */
public:
    WorldStubImpl(/* args */);
    virtual ~WorldStubImpl();
    virtual void sayHello(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _name, sayHelloReply_t _reply);
    virtual void sayBye(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _name, sayByeReply_t _reply);
    
};

// WorldStubImpl::WorldStubImpl(/* args */)
// {
// }

// WorldStubImpl::~WorldStubImpl()
// {
// }

#endif /*  WORLDSTUBIMPL_H_ */
