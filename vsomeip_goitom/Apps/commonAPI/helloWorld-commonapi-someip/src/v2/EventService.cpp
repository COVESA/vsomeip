
//WorldService.cpp
#include <iostream>
#include <thread>

#include <CommonAPI/CommonAPI.hpp>
#include "EventStubImpl.hpp"

using namespace std;

int main(int argc, char** argv)
{
    CommonAPI::Runtime::setProperty("LogContext", "E01S");
    CommonAPI::Runtime::setProperty("LogApplication", "E01S");
    CommonAPI::Runtime::setProperty("LibraryBase", "World");
    
    std::shared_ptr <CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();

    std::string domain = "local";
    std::string goodInstance = "commonapi.examples.event";
    std::string connection = "service-sample";

    std::shared_ptr<EventStubImpl> myGoodService = std::make_shared<EventStubImpl>();
    myGoodService ->setTemperatureAttribute(20);
    bool goodServiceRegistered = runtime->registerService(domain, goodInstance, myGoodService, connection);
    

    while (!goodServiceRegistered)
    {
        std::cout << "Register Service failed, trying again in 100 milliseconds..." <<std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        goodServiceRegistered = runtime->registerService(domain, goodInstance, myGoodService, connection);
    }
    
    std::cout << "Successfully Registered Service!" <<std::endl;

    while (true)
    {
        std::cout << "Waiting for calls ... (Abort with CTRL+C)" <<std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
  
    return 0;
}
