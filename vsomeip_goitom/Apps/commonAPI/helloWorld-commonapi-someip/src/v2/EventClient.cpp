
//EventClient.cpp
#include <iostream>
#include <string>
#include <thread>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <CommonAPI/CommonAPI.hpp>

#include <v1/commonapi/examples/EventProxy.hpp>


using namespace v1::commonapi::examples;
std::shared_ptr <EventProxy<>> proxy_;

void setupSubscription()
{
// Subscribe to temperature field changes
    proxy_->getTemperatureAttribute().getChangedEvent().subscribe(
            [&](const int32_t& value){
                std::cout << "[FIELD CHANGE] Temperature changed to: " << value << std::endl;   
            }
        );

}

void getTemperature()
{

        //Get temperature
    CommonAPI::CallStatus callStatus;
    int32_t temperature;
    proxy_ ->getTemperatureAttribute().getValue(callStatus, temperature);
    if (callStatus == CommonAPI::CallStatus::SUCCESS)
    {
        std::cout << "Current temperature: " << temperature << std::endl;
    }
    else
    {
        std::cout << "Failed to get temperature" << std::endl;
    }
        
}
void setTemperature(int32_t& value)
{
    //Set temperature
    //std::cout << "Enter new temperature: ";
    //std::string tempStr;
    //std::getline(std::cin, tempStr);
    std::cout << "[CLIENT] Attempting to set temperature to: " << value << std::endl;
    try
    {
        //int32_t newTemp = std::stoi(tempStr);
        CommonAPI::CallStatus callStatus;
        int32_t responseValue;
        proxy_->getTemperatureAttribute().setValue(value, callStatus, responseValue);
        std::cout << "[CLIENT] Call status: " << (int)callStatus << std::endl;

        if (callStatus == CommonAPI::CallStatus::SUCCESS)
        {
            std::cout << " [CLIENT] Temperature set to: " << responseValue << std::endl;
        } 
        else
        {
            std::cout << "[CLIENT] Failed to set temperature" << std::endl;
        }
    }
    catch(const std::exception& e)
    {
        std::cout << "Invalid temperature value" << std::endl;
    }
}

int main()
{
    CommonAPI::Runtime::setProperty("LogContext", "E01C");
    CommonAPI::Runtime::setProperty("LogApplication", "E01C");
    CommonAPI::Runtime::setProperty("LibraryBase", "World");
     
    std::shared_ptr <CommonAPI::Runtime> runtime = CommonAPI::Runtime::get();
    std::string domain  = "local";
    std::string goodInstance = "commonapi.examples.event";
    std::string connection = "client-sample";
    
    proxy_ = runtime->buildProxy <EventProxy>(domain,goodInstance,connection);

    std::cout << "Checking availability !" <<std::endl;

    
    while (!proxy_ ->isAvailable())    
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    
    std::cout <<"Available..." <<std::endl;
    CommonAPI::CallInfo info(1000);

    setupSubscription();

    getTemperature();

    int32_t newTemp=45;
    setTemperature(newTemp);

    return 0;
}