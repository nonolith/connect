#include <iostream>
#include <vector>
#include <libusb-1.0/libusb.h>
#include <sys/timeb.h>
using namespace std;


#define CEE_VID 0x9999
#define CEE_PID 0xffff

#define EP_BULK_IN 0x81
#define N_TRANSFERS 8
#define TRANSFER_SIZE 2048

long millis(){
	struct timeb tp;
	ftime(&tp);
	return tp.millitm;
}

void in_transfer_callback(libusb_transfer *t);

class CEE_device{
	public: 
	CEE_device(libusb_device *dev, libusb_device_descriptor &desc){
		int r = libusb_open(dev, &handle);
		if (r != 0){
			cerr << "Could not open device"<<endl;
			return;
		}
		
		r = libusb_claim_interface(handle, 0);
		if (r != 0){
			cerr << "Could not claim interface"<<endl;
			return;
		}
	
		libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, serial, 32);
		cout << "Found a CEE: "<< serial << endl;
		
		streaming = 0;
	}
	
	~CEE_device(){
		stop_streaming();
		libusb_close(handle);
	}
	
	void start_streaming(){
		if (streaming) return;
		streaming = 1;
		for (int i=0; i<N_TRANSFERS; i++){
			in_transfers[i] = libusb_alloc_transfer(0);
			unsigned char *buf = (unsigned char *) malloc(TRANSFER_SIZE);
			libusb_fill_bulk_transfer(in_transfers[i], handle, EP_BULK_IN, buf, TRANSFER_SIZE, in_transfer_callback, this, 50);
			libusb_submit_transfer(in_transfers[i]);
		}
	}
	
	void stop_streaming(){
		if (!streaming) return;
		for (int i=0; i<N_TRANSFERS; i++){
			// zero user_data tells the callback to free on complete. this obj may be dead by then
			if (in_transfers[i]->user_data){
				in_transfers[i]->user_data=0;
				libusb_cancel_transfer(in_transfers[i]);
			}
		}
		streaming = 0;
	}
	
	void in_transfer_complete(libusb_transfer *t){
		if (t -> status == LIBUSB_TRANSFER_COMPLETED){
			libusb_submit_transfer(t); 
			cout <<  millis() << " " << t << " complete " << t->actual_length << endl;
		}else{
			cerr << "Transfer error" << endl;
			t -> user_data = 0;
			free(t -> buf);
			libusb_free_transfer(t);
			stop_streaming();
		}
	}

	libusb_device_handle *handle;
	unsigned char serial[32];
	libusb_transfer* in_transfers[N_TRANSFERS];
	int streaming;
};

void in_transfer_callback(libusb_transfer *t){
	if (t -> user_data){
		((CEE_device *)(t->user_data))->in_transfer_complete(t);
	}else{ // user_data was zeroed out when device was deleted
		free(t->buffer);
		libusb_free_transfer(t);
	}
}

vector <CEE_device*> devices;

void scan_bus(){
	libusb_device **devs;
	
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		cerr << "Error in get_device_list" << endl;
	}
	
	for (ssize_t i=0; i<cnt; i++){
		libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(devs[i], &desc);
		if (r<0){
			cerr << "Error in get_device_descriptor" << endl;
			continue;
		}
		if (desc.idVendor == CEE_VID && desc.idProduct == CEE_PID){
			devices.push_back(new CEE_device(devs[i], desc));
		}
	}
	
	libusb_free_device_list(devs, 1);
}


int main(){
	int r;
	
	r = libusb_init(NULL);
	if (r < 0){
		cerr << "Could not init libusb" << endl;
		return 1;
	}
	
	libusb_set_debug(NULL, 3);

	scan_bus();

	cout << devices.size() << " devices found" << endl;
	
	for (auto it=devices.begin() ; it < devices.end(); it++ ){
		(*it) -> start_streaming();
	}
	
	while(1) libusb_handle_events(NULL);
	
	for (auto it=devices.begin() ; it < devices.end(); it++ ){
		delete *it;
	}

	
	libusb_exit(NULL);
	return 0;
}
