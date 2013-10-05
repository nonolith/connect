#include <boost/thread.hpp>

int server_main(int argc, char* argv[]);
int ui_main(int argc, char* argv[]);

int main(int argc, char* argv[]){
	boost::thread* server_thread = new boost::thread(boost::bind(server_main, argc, argv));
	ui_main(argc, argv);
}