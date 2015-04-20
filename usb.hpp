#include <utility>

class libusb_device_handle;

namespace usb {
	std::pair<libusb_device_handle *, bool> connect();
	void switch_mode(libusb_device_handle *);
	void program(libusb_device_handle *, uint8_t const *data, size_t size);
	void close(libusb_device_handle *);
}
