
#include <iostream>
#include <thread>
#include <chrono>
#include <CommonAPI/CommonAPI.hpp>

#include "v1/commonapi/examples/EventsAndFieldsProxy.hpp"

using namespace v1::commonapi::examples;

class subscribe_sample
{
private:
    void handleCommand(int command, std::shared_ptr<EventsAndFieldsProxy<>> proxy_ );
public:
    subscribe_sample(/* args */);
    virtual ~subscribe_sample();
    void setupSubscriptions(std::shared_ptr<EventsAndFieldsProxy<>> proxy_ );
    void runInteractiveMode(std::shared_ptr<EventsAndFieldsProxy<>> proxy_ );
};

