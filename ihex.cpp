#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using std::uint8_t;
using std::size_t;

namespace {
	uint8_t hex_value(char c) {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		throw std::runtime_error(std::string("Not a valid hexadecimal digit: '") + c + "'.");
	}
	uint8_t hex_byte(std::string & s, size_t i = 0) {
		if (i + 1 >= s.size() || s[i+1] == '\n') throw std::runtime_error("Unexpected end of line.");
		return hex_value(s[i]) << 4 | hex_value(s[i+1]);
	}
}

std::vector<uint8_t> load_ihex(std::istream & in) {
	std::vector<uint8_t> memory;
	memory.reserve(1024);
	size_t memory_used = 0;
	size_t address_offset = 0;
	std::string line;
	while (std::getline(in, line)) {
		if (line[0] == '\n') continue;
		if (line[0] != ':') throw std::runtime_error{"Invalid file format."};
		size_t size = hex_byte(line, 1);
		size_t address = address_offset + (hex_byte(line, 3) << 8 | hex_byte(line, 5));
		uint8_t type = hex_byte(line, 7);
		if (type == 0x00) {
			size_t end = address + size;
			if (end > memory.size()) {
				size_t s = memory.capacity();
				while (s < end) s *= 2;
				memory.reserve(s);
				memory.resize(end);
			}
			for (size_t i = 0; i < size; ++i) {
				size_t a = address + i;
				uint8_t value = hex_byte(line, 9 + i * 2);
				memory[a] = value;
			}
		} else if (type == 0x01) {
			memory.shrink_to_fit();
			return memory;
		} else if (type == 0x02 && size == 2) {
			address_offset = hex_byte(line, 9) << 12 | hex_byte(line, 11) << 4;
		} else if (type == 0x04 && size == 2) {
			address_offset = hex_byte(line, 9) << 24 | hex_byte(line, 11) << 16;
		} else {
			std::stringstream s;
			s << "Unknown record type 0x" << std::hex << type << " (size " << std::dec << size << ").";
			throw std::runtime_error{s.str()};
		}
	}
	if (in.bad()) {
		throw std::runtime_error{"Unable to read from file."};
	} else {
		throw std::runtime_error{"Unexpected end of file."};
	}
}
