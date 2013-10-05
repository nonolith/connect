#include <boost/thread.hpp>

int server_main(int argc, char* argv[]);
void server_shutdown();
int ui_main(int argc, char* argv[]);

int main(int argc, char* argv[]){
	boost::thread server_thread = boost::thread(boost::bind(server_main, argc, argv));
	ui_main(argc, argv);
	server_shutdown();
	server_thread.join();
}