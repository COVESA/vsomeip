//EventsAndFieldsStubImpl.cpp
#include "EventsAndFieldsStubImpl.hpp"

EventsAndFieldsStubImpl::EventsAndFieldsStubImpl(/* args */):
// temperature_(20),
// status_("NORMAL"),
running_{true}
{
}

EventsAndFieldsStubImpl::~EventsAndFieldsStubImpl()
{
    running_ = false;
    if (updateThread_.joinable())
    {
        updateThread_.join();
    }
}

// const int32_t& EventsAndFieldsStubImpl::getTemperatureAttribute(const std::shared_ptr<CommonAPI::ClientId> _client)
// {
//     std::cout << "Temperature requested: " << temperature_ << std::endl;
//     return temperature_;
// }

// const std::string& EventsAndFieldsStubImpl::getStatusAttribute(const std::shared_ptr<CommonAPI::ClientId> _client)
// {
//     std::cout << "Status requested: " << status_ << std::endl;
//     return status_;
// }
    const int32_t& EventsAndFieldsStubImpl::getTemperatureAttribute(const std::shared_ptr<CommonAPI::ClientId> _client)
    {
        auto& value = EventsAndFieldsStubDefault::getTemperatureAttribute();
        std::cout << "[SERVER] Returning stored temperature: " << value << std::endl;
        return value;
    }

    const std::string& EventsAndFieldsStubImpl::getStatusAttribute(const std::shared_ptr<CommonAPI::ClientId> _client)
    {
        auto& value = EventsAndFieldsStubDefault::getStatusAttribute();
        std::cout << "[SERVER] Returning stored Status: " << value << std::endl;
        return value;
    }

void EventsAndFieldsStubImpl::getSystemInfo(const std::shared_ptr<CommonAPI::ClientId> _client, getSystemInfoReply_t _reply)
{
        std::cout << "get SystemInfo called" << std::endl;
        std::string version = "1.0.0";
        std::string status = getStatusAttribute(_client);
        uint temperature = getTemperatureAttribute(_client);
        uint32_t uptime = 12345; // Mock uptime in seconds       

        _reply(version, uptime);   
}

// void EventsAndFieldsStubImpl::setTemperatureAttribute(const std::shared_ptr<CommonAPI::ClientId> _client, int32_t _value)
// {
//     std::cout << "Setting temperature to: " << _value << std::endl;
    
//     if (temperature_ != _value)
//     {
//         temperature_ = _value;
//         fireTemperatureAttributeChanged(temperature_);
//         // Check for alert conditions
//         if (temperature_ > 80)
//         {
//             fireTemperatureAlertEvent(temperature_, "HIGH");
//         } 
//         else if (temperature_ < 0)
//         {
//             fireTemperatureAlertEvent(temperature_, "LOW");
//         }
//     }
//     // Optionally, handle the _client parameter if you need per-client logic
// }

// void EventsAndFieldsStubImpl::setStatusAttribute(const std::shared_ptr<CommonAPI::ClientId> _client, std::string _value)
// {
//     std::cout << "Setting status to: " << _value << std::endl;
//     std::string oldStatus = status_;
//     status_ = _value;    
//     // Fire change notification
//     fireStatusAttributeChanged(status_);
    
//     // Fire status changed event with timestamp
//     uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
//     std::chrono::system_clock::now().time_since_epoch()).count();
    
//     fireStatusChangedEvent(status_, timestamp);  
// }

// void EventsAndFieldsStubImpl::periodicUpdate(const std::shared_ptr<CommonAPI::ClientId> _client)
// {
//     int counter = 0;
//     while (running_)
//     {
//         std::this_thread::sleep_for(std::chrono::seconds(5));
        
//         if (!running_)
//             break;
//         // Simulate temperature fluctuation
//         counter++;
//         int32_t newTemp = 20 + (counter % 10) * 5;
        
//         if (newTemp != temperature_)
//         {
//             std::cout << "Periodic temperature update: " << newTemp << std::endl;
//             setTemperatureAttribute(_client, newTemp);
//         }
//         // Occasionally change status
//         if (counter % 4 == 0)
//         {
//             std::string newStatus = (counter % 8 == 0) ? "MAINTENANCE" : "NORMAL";
//             if (newStatus != status_)
//             {
//                 setStatusAttribute(_client, newStatus);
//             }
//         }
//     }
// }

    bool EventsAndFieldsStubImpl::trySetTemperatureAttribute(int32_t _value)
    {
        std::cout << "[SERVER] Temperature being set to: " << _value << std::endl;
        // Call the base class implementation (this updates the value and fires attribute change)
        bool valueChanged= EventsAndFieldsStubDefault::trySetTemperatureAttribute(_value);
        // Fire temperature alert events based on the new value
        if (valueChanged)
            checkAndFireAlerts(_value);

        return valueChanged;  
    }

    void EventsAndFieldsStubImpl::checkAndFireAlerts(int32_t temperature)
    {
        if (temperature>80)
        {
           fireTemperatureAlertEvent(temperature, "HIGH");
           std::cout << "[SERVER] Fired TemperatureAlert event" << std::endl;
        }
        else if (temperature <5)
        {
            fireTemperatureAlertEvent(temperature, "LOW");
            std::cout << "[SERVER] Fired TemperatureAlert event" << std::endl;
        }
    }