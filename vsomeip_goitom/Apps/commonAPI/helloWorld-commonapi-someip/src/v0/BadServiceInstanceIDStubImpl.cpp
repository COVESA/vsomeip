
//BadServiceInstanceIDStubImpl.cpp
#include "BadServiceInstanceIDStubImpl.hpp"

BadServiceInstanceIDStubImpl::BadServiceInstanceIDStubImpl(/* args */)
{
}

BadServiceInstanceIDStubImpl::~BadServiceInstanceIDStubImpl()
{
}

void BadServiceInstanceIDStubImpl::sayHello(const std::shared_ptr <CommonAPI::ClientId> _client,
                         std::string _name, sayHelloReply_t _reply)
{
    std::cout << "sayHello called!" << std::endl;

    std::stringstream messageStream;
   messageStream << "Hello " << _name << "!";
    std::cout << "sayHello('" << _name << "'): '" << messageStream.str() << "'\n";
    _reply(messageStream.str()); 
};
