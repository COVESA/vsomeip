
//WorldClient.cpp
#include <iostream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <CommonAPI/CommonAPI.hpp>

#include <v0/commonapi/examples/WorldProxy.hpp>
#include <v0/commonapi/examples/BadProxyServiceIDProxy.hpp>
#include <v0/commonapi/examples/BadProxyInstanceIDProxy.hpp>

using namespace v0::commonapi::examples;

int main()
{
    CommonAPI::Runtime::setProperty("LogContext", "E01C");
    CommonAPI::Runtime::setProperty("LogApplication", "E01C");
    CommonAPI::Runtime::setProperty("LibraryBase", "World");
     
    std::shared_ptr <CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    std::string domain  = "local";
    std::string goodInstance = "commonapi.examples.someip";
    std::string badProxyServiceId = "commonapi.examples.someip.badproxyserviceid";
    std::string badProxyInstanceId = "commonapi.examples.someip.badproxyinstanceid";
    std::string connection = "client-sample";
    
    std::shared_ptr <WorldProxy<>> myGoodProxy = runtime->buildProxy <WorldProxy>(domain,goodInstance,connection);
    std::shared_ptr <BadProxyServiceIDProxy<>> myBadProxyServiceId = runtime->buildProxy <BadProxyServiceIDProxy>(domain, badProxyServiceId, connection);
    std::shared_ptr <BadProxyInstanceIDProxy<>> myBadProxyInstanceId = runtime->buildProxy <BadProxyInstanceIDProxy>(domain, badProxyInstanceId, connection);

    std::cout << "Checking availability !" <<std::endl;

    std::cout<<(myBadProxyServiceId ->isAvailable()? "Available..." : "Not Available...") <<std::endl;
    std::cout<<(myBadProxyInstanceId ->isAvailable()? "Available..." : "Not Available...") <<std::endl;  
    
    while (!myGoodProxy ->isAvailable())    
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    
    std::cout <<"Available..." <<std::endl;
    
    const std::string name = "World";
    CommonAPI::CallStatus callStatus;
    std::string returnMessage;

    CommonAPI::CallInfo info(1000);
    info.sender_ = 1234;

    bool sayHello_call_ = true;
    while (true)
    {
        if (sayHello_call_)
        {
            myGoodProxy ->sayHello(name, callStatus, returnMessage, &info);
            if (callStatus != CommonAPI::CallStatus::SUCCESS)
            {
                std::cerr <<"Remote call failed! \n";
                return -1;                    
            }
            info.timeout_ = info.timeout_ + 1000;
        }
        else
        {
            myGoodProxy ->sayBye(name, callStatus, returnMessage, &info);
            if (callStatus != CommonAPI::CallStatus::SUCCESS)
            {
                std::cerr <<"Remote call failed! \n";
                return -1;                    
            }
            info.timeout_ = info.timeout_ + 1000;
        }   
        std::cout <<"Got message: '" <<returnMessage << "'\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        sayHello_call_ = !sayHello_call_;
    }
    return 0;
}