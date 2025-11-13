
//WorldStubImpl.cpp

#include "WorldStubImpl.hpp"

WorldStubImpl::WorldStubImpl(/* args */)
{
}

WorldStubImpl::~WorldStubImpl()
{
}

void WorldStubImpl::sayHello(const std::shared_ptr <CommonAPI::ClientId> _client,
                         std::string _name, sayHelloReply_t _reply)
{
    std::cout << "sayHello called!" << std::endl;

    std::stringstream messageStream;
   messageStream << "Hello " << _name << "!";
    std::cout << "sayHello('" << _name << "'): '" << messageStream.str() << "'\n";
    _reply(messageStream.str()); 
};

void WorldStubImpl::sayBye(const std::shared_ptr <CommonAPI::ClientId> _client,
                         std::string _name, sayHelloReply_t _reply)
{
    std::cout << "sayBye called!" << std::endl;

    std::stringstream messageStream;
   messageStream << "Bye " << _name << "!";
    std::cout << "sayBye('" << _name << "'): '" << messageStream.str() << "'\n";
    _reply(messageStream.str()); 
};    




