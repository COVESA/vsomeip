
//EventsAndFieldsStubImpl.hpp
#include <iostream>
#include <thread>>
#include <chrono>>

#ifndef EVENTSANDFIELDSSTUBIMPL_H_
#define EVENTSANDFIELDSSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v1/commonapi/examples/EventsAndFieldsStubDefault.hpp>

class EventsAndFieldsStubImpl: public v1_0::commonapi::examples::EventsAndFieldsStubDefault
{
private:
    //int32_t temperature_;
    //std::string status_;
    std::thread updateThread_;
    std::atomic<bool> running_;
    //virtual void periodicUpdate(const std::shared_ptr<CommonAPI::ClientId> _client);
        // Method to fire events from server logic
    void checkAndFireAlerts(int32_t temperature);
public:
    EventsAndFieldsStubImpl(/* args */);
   virtual ~EventsAndFieldsStubImpl();
        // Provides getter access to the attribute temperature
    virtual const int32_t &getTemperatureAttribute(const std::shared_ptr<CommonAPI::ClientId> _client)override;
            /// Provides getter access to the attribute status
    virtual const std::string &getStatusAttribute(const std::shared_ptr<CommonAPI::ClientId> _client)override;
        // This is the method that will be called on remote calls on the method getSystemInfo.
    virtual void getSystemInfo(const std::shared_ptr<CommonAPI::ClientId> _client, getSystemInfoReply_t _reply);
protected:
    virtual bool trySetTemperatureAttribute(int32_t _value) override;
};

#endif /*EVENTSANDFIELDSSTUBIMPL_H_*/


