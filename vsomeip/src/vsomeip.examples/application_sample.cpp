#include <chrono>

#include <boost/thread.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip/factory.hpp>

#define SAMPLE_MULTITHREAD

vsomeip::factory * the_factory = vsomeip::factory::get_instance();


void run() {
	vsomeip::application * the_application = the_factory->create_application();

	the_application->init();
	the_application->start();

	while (1) {
		the_application->run();
	}
}


int main(int argc, char **argv) {
	boost::thread t00(run);
#ifdef SAMPLE_MULTITHREAD
	boost::thread t01(run);
	boost::thread t02(run);
	boost::thread t03(run);
	boost::thread t04(run);
	boost::thread t05(run);
	boost::thread t06(run);
	boost::thread t07(run);
	boost::thread t08(run);
	boost::thread t09(run);
	boost::thread t10(run);
	boost::thread t11(run);
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




