#pragma once

#include <future>
#include <memory>
#include <string_view>

#include <vsomeip/application.hpp>

struct applet
{
protected:
    std::shared_ptr<vsomeip_v3::application> application;

    applet(std::string_view name);
    virtual ~applet();

private:
    std::future<void> async_start;

    virtual void on_state_registered();
    virtual void on_state_deregistered();
};
