#include <chrono>

#include <boost/thread.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip/factory.hpp>

#define SAMPLE_MULTITHREAD

vsomeip::factory * the_factory = vsomeip::factory::get_instance();

void run(const char *name) {
	vsomeip::application * the_application = the_factory->create_application(name);

	the_application->init();
	the_application->start();

	while (1) {
		the_application->run();
	}
}


int main(int argc, char **argv) {
	boost::thread t00(run, "alpha");
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




