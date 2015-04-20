#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <string>

#include <libusb-1.0/libusb.h>

#include "usb.hpp"

namespace {
	char const *error_name(int s) {
		switch (s) {
			case LIBUSB_ERROR_IO: return "Input/output error.";
			case LIBUSB_ERROR_INVALID_PARAM: return "Invalid parameter.";
			case LIBUSB_ERROR_ACCESS: return "Access denied (insufficient permissions)";
			case LIBUSB_ERROR_NO_DEVICE: return "No such device (it may have been disconnected)";
			case LIBUSB_ERROR_NOT_FOUND: return "Entity not found.";
			case LIBUSB_ERROR_BUSY: return "Resource busy.";
			case LIBUSB_ERROR_TIMEOUT: return "Operation timed out.";
			case LIBUSB_ERROR_OVERFLOW: return "Overflow.";
			case LIBUSB_ERROR_PIPE: return "Pipe error.";
			case LIBUSB_ERROR_INTERRUPTED: return "System call interrupted";
			case LIBUSB_ERROR_NO_MEM: return "Insufficient memory.";
			case LIBUSB_ERROR_NOT_SUPPORTED: return "Operation not supported on this platform.";
			default: return "Unknown error.";
		}
	}

	void throw_if_err(int s) {
		if (s < 0) throw std::runtime_error{std::string{"USB error: "} + error_name(s)};
	}

	void init_libusb() {
		struct InitExit {
			InitExit() {
				throw_if_err(libusb_init(nullptr));
			}
			~InitExit() {
				libusb_exit(nullptr);
			}
		};
		static InitExit init_exit;
	}

	void send_packet(libusb_device_handle *device, uint8_t const *data) {
		constexpr uint8_t request_type = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
		uint8_t response;
		throw_if_err(libusb_control_transfer(device, request_type, 9, 0x0300, 0, const_cast<uint8_t *>(data), 64, 5000));
	}

	void receive_packet(libusb_device_handle *device, uint8_t *data) {
		constexpr uint8_t request_type = LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
		uint8_t response;
		throw_if_err(libusb_control_transfer(device, request_type, 1, 0x0300, 0, data, 64, 5000));
	}

	void claim(libusb_device_handle *device) {
		libusb_detach_kernel_driver(device, 0);
		libusb_claim_interface(device, 0);
	}
}

namespace usb {
	void switch_mode(libusb_device_handle *device) {
		claim(device);
		uint8_t data[64] = {0x44};
		send_packet(device, data);
	}

	void program(libusb_device_handle *device, uint8_t const *data, size_t size) {
		claim(device);
		{
			uint8_t packet[64] = {0x33, 0, 0, 0, 0, static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size & 0xFF)};
			send_packet(device, packet);
		}
		size_t sent = 0;
		while (sent < size) {
			uint8_t packet[64];
			size_t s = std::min<size_t>(64, size - sent);
			std::copy_n(data + sent, s, packet);
			std::fill_n(packet + s, 64 - s, 0);
			send_packet(device, packet);
			sent += s;
		}
		{
			uint8_t packet[64] = {0x22, 0, 0, 0, 0, 0, 0x02};
			send_packet(device, packet);
		}
		{
			uint8_t packet[64];
			receive_packet(device, packet);
			uint16_t checksum = std::accumulate(data, data + size, 0);
			if ((packet[0] << 8 | packet[1]) != checksum) throw std::runtime_error{"Received invalid checksum."};
		}
	}

	std::pair<libusb_device_handle *, bool> connect() {
		init_libusb();
		libusb_device **list;
		throw_if_err(libusb_get_device_list(nullptr, &list));
		libusb_device *device = nullptr;
		bool need_switch = false;
		bool multiple_found = false;
		for (auto **i = list; *i; ++i) {
			libusb_device_descriptor desc{};
			if (libusb_get_device_descriptor(*i, &desc) < 0) continue;
			if (desc.idVendor == 0x0E6A) {
				if (desc.idProduct == 0x030C || desc.idProduct == 0x030B) {
					if (device) {
						multiple_found = true;
						break;
					}
					device = *i;
					need_switch = desc.idProduct == 0x030C;
				}
			}
		}
		if (!multiple_found && device) {
			libusb_device_handle *handle;
			throw_if_err(libusb_open(device, &handle));
			libusb_free_device_list(list, true);
			return {handle, need_switch};
		} else if (multiple_found) {
			libusb_free_device_list(list, true);
			throw std::runtime_error{"Multiple Truly Ergonomic Keyboards found. Please disconnect all but one."};
		} else {
			libusb_free_device_list(list, true);
			return {nullptr, false};
		}
	}

	void close(libusb_device_handle *dev) {
		libusb_attach_kernel_driver(dev, 0);
		libusb_close(dev);
	}
}
