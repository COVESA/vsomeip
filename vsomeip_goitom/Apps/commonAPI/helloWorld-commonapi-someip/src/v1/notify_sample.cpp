
#include <iostream>
#include <thread>>
#include <chrono>>

#include <CommonAPI/CommonAPI.hpp>

#include "EventsAndFieldsStubImpl.hpp"

int main()
{
    std::cout << "Starting MyService..." << std::endl;
    CommonAPI::Runtime::setProperty("LogContext", "E01C");
    CommonAPI::Runtime::setProperty("LogApplication", "E01C");
    CommonAPI::Runtime::setProperty("LibraryBase", "World");
    
    std::shared_ptr<CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    std::shared_ptr<EventsAndFieldsStubImpl> myGoodService = std::make_shared<EventsAndFieldsStubImpl>();
    myGoodService -> setTemperatureAttribute(20);
    myGoodService -> setStatusAttribute("NORMAL");

    std::string domain = "local";
    std::string goodService = "commonapi.examples.eventsandfields";
    std::string connection = "service-sample";

    bool goodServiceRegistered = runtime->registerService(domain, goodService, myGoodService, connection);
    
    if (!goodServiceRegistered)
    {
      std::cout << "Register Service failed, trying again in 100 milliseconds..." <<std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        goodServiceRegistered = runtime->registerService(domain, goodService, myGoodService, connection);
    }
    
    std::cout << "Successfully Registered Service!" <<std::endl;

    // Keep the service running
    while (true)
    {
        std::cout << "Waiting for calls ... (Abort with CTRL+C)" <<std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(60));
    } 

    return 0;
}