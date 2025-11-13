//EventStubImpl.cpp
#include "EventStubImpl.hpp"

EventStubImpl::EventStubImpl(/* args */)

{
        //std::cout << "[STUB] EventStubImpl created with initial temperature: " << temperature_ << std::endl;
}

EventStubImpl::~EventStubImpl()
{
}

// const int32_t& EventStubImpl::getTemperatureAttribute(const std::shared_ptr<CommonAPI::ClientId> _client){
// std::cout << "Temperature requested: " << temperature_ << std::endl;
//         return temperature_;
// }
// void EventStubImpl::setTemperatureAttr(int32_t _value)
// {
// std::cout << "Setting temperature to: " << _value << std::endl;
//         setTemperatureAttribute(_value);
//         //temperature_ = _value;
//         fireTemperatureAttributeChanged(temperature_);
// }