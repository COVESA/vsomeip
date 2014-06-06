#include <chrono>

#include <boost/thread.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/factory.hpp>

//#define SAMPLE_MULTITHREAD

vsomeip::factory * the_factory = vsomeip::factory::get_instance();

int options_count = 0;
char **options = 0;

vsomeip::application * the_application;
vsomeip::endpoint * an_endpoint = the_factory->get_endpoint("10.0.2.15", 30499, vsomeip::ip_protocol::UDP);
vsomeip::endpoint * another_endpoint = the_factory->get_endpoint("10.0.2.15", 30506, vsomeip::ip_protocol::TCP);

void on_off() {
	bool is_on = true;
	while (1) {
		usleep(8000000);
		if (is_on) {
			the_application->provide_service(0x2234, 0x4455, an_endpoint);
			the_application->provide_service(0x2234, 0x4455, another_endpoint);
		} else
			the_application->withdraw_service(0x2234, 0x4455);

		is_on = !is_on;
	}
}

void run(const char *name) {
	the_application = the_factory->create_application(name);

	the_application->init(options_count, options);

	the_application->provide_service(0x2233, 0x4455, an_endpoint);
	the_application->provide_service(0x2233, 0x4456, an_endpoint);
	the_application->provide_service(0x2234, 0x4455, an_endpoint);

	the_application->provide_service(0x2234, 0x4455, another_endpoint);
	the_application->provide_service(0x2235, 0x4460, another_endpoint);

	the_application->start();
}


int main(int argc, char **argv) {
	options_count = argc;
	options = argv;

	boost::thread t00(run, "alpha");
	boost::thread t00_a(on_off);
#ifdef SAMPLE_MULTITHREAD
	boost::thread t01(run, "bravo");
	boost::thread t02(run, "charly");
	boost::thread t03(run, "delta");
	boost::thread t04(run, "echo");
	boost::thread t05(run, "foxtrott");
	boost::thread t06(run, "golf");
	boost::thread t07(run, "hotel");
	boost::thread t08(run, "indian");
	boost::thread t09(run, "juliet");
	boost::thread t10(run, "kilo");
	boost::thread t11(run, "lima");
#endif

	t00.join();
#ifdef SAMPLE_MULTITHREAD
	t01.join();
	t02.join();
	t03.join();
	t04.join();
	t05.join();
	t06.join();
	t07.join();
	t08.join();
	t09.join();
	t10.join();
	t11.join();
#endif

	return 0;
}




