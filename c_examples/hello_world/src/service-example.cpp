#include <vsomeip/vsomeip.hpp>
#include <iostream>

std::shared_ptr<vsomeip::application> app;
int main(){
    app = vsomeip::runtime::get()->create_application("World");
    app->init();
    app-->start();
}