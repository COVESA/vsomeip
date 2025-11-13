
// WorldStubImpl.hpp
#ifndef EVENTSTUBIMPL_H_
#define EVENTSTUBIMPL_H_

#include <CommonAPI/CommonAPI.hpp>
#include <v1/commonapi/examples/EventStubDefault.hpp>

class EventStubImpl: public v1_0::commonapi::examples::EventStubDefault
{
private:
    /* data */
    std::int32_t temperature_;
public:
    EventStubImpl(/* args */);
    virtual ~EventStubImpl();
    // virtual const int32_t &getTemperatureAttribute(const std::shared_ptr<CommonAPI::ClientId> _client) override;
    // virtual void setTemperatureAttr(int32_t _value);
};

// EventStubImpl::EventStubImpl(/* args */)
// {
// }

// EventStubImpl::~EventStubImpl()
// {
// }

#endif