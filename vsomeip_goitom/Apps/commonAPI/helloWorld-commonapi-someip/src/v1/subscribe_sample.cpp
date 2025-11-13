
//subscribe_sample.cpp
#include "subscribe_sample.hpp"

subscribe_sample::subscribe_sample(/* args */)
{
  
}

subscribe_sample::~subscribe_sample()
{
}

void subscribe_sample::setupSubscriptions(std::shared_ptr<EventsAndFieldsProxy<>> proxy_ )
{
    // Subscribe to temperature field changes
        proxy_->getTemperatureAttribute().getChangedEvent().subscribe(
                [&](const int32_t& value){
                 std::cout << "[FIELD CHANGE] Temperature changed to: " << value << std::endl;   
                }
            );

        //Subscribe to status field changes
        proxy_ ->getStatusAttribute().getChangedEvent().subscribe(
            [&](const std::string& value){
              std::cout << "[FIELD CHANGE] Status changed to: " << value << std::endl;  
            }
        );

        // Subscribe to temperature alert events
        proxy_ ->getTemperatureAlertEvent().subscribe(
            [&] (const int32_t& currentTemp, const std::string& alertLevel){
        std::cout << "[EVENT] Temperature Alert! Temp: " << currentTemp 
            << ", Level: " << alertLevel << std::endl;
            }
        );
    // Subscribe to status changed events
    proxy_ ->getStatusChangedEvent().subscribe(
        [&] (const std::string& newStatus, const uint64_t& timestamp){
    std::cout << "[EVENT] Status Changed! New Status: " << newStatus 
                         << ", Timestamp: " << timestamp << std::endl;
        }
    );

    std::cout << "All subscriptions set up successfully!" << std::endl;

}

void subscribe_sample::runInteractiveMode(std::shared_ptr<EventsAndFieldsProxy<>> proxy_ )
{
    std::cout << "=== Interactive Mode ===" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  1 - Get temperature" << std::endl;
    std::cout << "  2 - Set temperature" << std::endl;
    std::cout << "  3 - Get status" << std::endl;
    std::cout << "  4 - Set status" << std::endl;
    std::cout << "  5 - Get system info" << std::endl;
    std::cout << "  q - Quit" << std::endl;

    std::string input;
    while (true)
    {
        std::cout << "Enter command: "<< std::endl;;
        std::getline(std::cin, input);
        
        if (input == "q" || input == "quit")
         {
            break;
        }
        try
        {
        int command = std::stoi(input);
        handleCommand(command, proxy_);
        } catch (const std::exception& e)
         {
            std::cout << "Invalid command. Try again." << std::endl;
        }
    }
    
}

void  subscribe_sample::handleCommand(int command, std::shared_ptr<EventsAndFieldsProxy<>> proxy_)
{
    switch (command)
    {
    case 1:
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
        
        break;
    }

    case 2:
    {
        //Set temperature
        std::cout << "Enter new temperature: ";
        std::string tempStr;
        std::getline(std::cin, tempStr);
        try
        {
            int32_t newTemp = std::stoi(tempStr);
            CommonAPI::CallStatus callStatus;
            int32_t responseValue;
            proxy_->getTemperatureAttribute().setValue(newTemp, callStatus, responseValue);
            if (callStatus == CommonAPI::CallStatus::SUCCESS)
            {
                std::cout << "Temperature set to: " << responseValue << std::endl;
            } 
            else
            {
                std::cout << "Failed to set temperature" << std::endl;
            }
        }
        catch(const std::exception& e)
        {
            std::cout << "Invalid temperature value" << std::endl;
        }
            
        break;
    }

    case 3:
    {
        //Get Status
        CommonAPI::CallStatus callStatus;
        std::string status;
        proxy_ ->getStatusAttribute().getValue(callStatus, status);
        if (callStatus == CommonAPI::CallStatus::SUCCESS)
        {
            std::cout << "Current status: " << status << std::endl;
        } 
        else
        {
            std::cout << "Failed to get status" << std::endl;
        }
        break;
    }

    case 4:
    {
        //Set Status
        std::cout << "Enter new status: ";
        std::string newStatus;
        std::getline(std::cin, newStatus);
        CommonAPI::CallStatus callStatus;
        std::string responseValue;
        proxy_ ->getStatusAttribute().setValue(newStatus, callStatus, responseValue);
        if (callStatus == CommonAPI::CallStatus::SUCCESS)
        {
            std::cout << "Status set to: " << responseValue << std::endl;
        } 
        else
        {
            std::cout << "Failed to set status" << std::endl;
        }
        break;
    }
    
    case 5:
    {
        //Get system info
        CommonAPI::CallStatus callStatus;
        std::string version;
        uint32_t uptime;
        proxy_ ->getSystemInfo(callStatus, version,uptime);
        if (callStatus == CommonAPI::CallStatus::SUCCESS)
        {
            std::cout << "System Info - Version: " << version 
                             << ", Uptime: " << uptime << " seconds" << std::endl;
        } 
        else
        {
            std::cout << "Failed to get system info" << std::endl;
        }
        break;
    }

    default:
        std::cout << "Unknown command" << std::endl;
        break;
    }
}

int main()
{
    CommonAPI::Runtime::setProperty("LogContext", "E01C");
    CommonAPI::Runtime::setProperty("LogApplication", "E01C");
    CommonAPI::Runtime::setProperty("LibraryBase", "World");
    
    std::shared_ptr<CommonAPI::Runtime> runtime_ = CommonAPI::Runtime::get();;
    std::string domain  = "local";
    std::string goodService = "commonapi.examples.eventsandfields";
    std::string connection = "client-sample";

    std::shared_ptr<EventsAndFieldsProxy<>> proxy_ = runtime_ ->buildProxy<EventsAndFieldsProxy>(domain, goodService, connection);
    
    std::cout << "Waiting for service to become available..." << std::endl;
    while (!proxy_ ->isAvailable())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Service is available!" << std::endl;

    // Give some time for initial subscriptions to work
    
    std::this_thread::sleep_for(std::chrono::seconds(2));

    subscribe_sample subscribe;
    subscribe.setupSubscriptions(proxy_);
    subscribe.runInteractiveMode(proxy_ );
    std::cout << "Client shutting down..." << std::endl;
    
    return 0;
}

