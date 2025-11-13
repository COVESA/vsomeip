
//HelloWorldStubImpl.cpp

#include "HelloWorldStubImpl.hpp"

HelloWorldStubImpl::HelloWorldStubImpl()
{
}

HelloWorldStubImpl::~HelloWorldStubImpl()
{
}

void HelloWorldStubImpl::sayHello(const std::shared_ptr <CommonAPI::ClientId> _client,
                        std::string _name, sayHelloReply_t _reply)
{
    std::cout << "sayHello called!" << std::endl;

    
    std::stringstream messageStream;
    messageStream << "Hello " << _name << "!";
    std::cout << "sayHello('" << _name << "'): '" << messageStream.str() << "'\n";
    _reply(messageStream.str());
    
   // _reply("Hello from service!");
};

