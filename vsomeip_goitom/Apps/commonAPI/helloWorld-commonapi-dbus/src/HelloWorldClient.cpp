
//HelloWorldClient.cpp
#include <iostream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <CommonAPI/CommonAPI.hpp>

#include <v0/commonapi/examples/HelloWorldProxy.hpp>

using namespace v0::commonapi::examples;

int main()
{
    CommonAPI::Runtime::setProperty("LogContext", "E01C");
    CommonAPI::Runtime::setProperty("LogApplication", "E01C");
    CommonAPI::Runtime::setProperty("LibraryBase", "HelloWorld");
    
    std::shared_ptr <CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    std::string domain  = "local";
    std::string instance = "commonapi.examples.HelloWorld";
    std::string connection = "sample-connection";
    
    std::shared_ptr <HelloWorldProxy<>> myproxy = runtime->buildProxy <HelloWorldProxy>(domain,instance,connection);

    std::cout << "Checking availability !" <<std::endl;
    while (!myproxy ->isAvailable())    
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    
    std::cout <<"Available..." <<std::endl;
    
    const std::string name = "World";
    CommonAPI::CallStatus callStatus;
    std::string returnMessage;

    CommonAPI::CallInfo info(1000);
    info.sender_ = 1234;

    while (true)
    {
        myproxy ->sayHello(name, callStatus, returnMessage, &info);
        if (callStatus != CommonAPI::CallStatus::SUCCESS)
        {
            std::cerr <<"Remote call failed! \n";
            return -1;                    
        }
        info.timeout_ = info.timeout_ + 1000;

        std::cout <<"Got message: '" <<returnMessage << "'\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;            
    
    
}