#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <charconv>
#include <stdexcept>
#include <system_error>
//#include <iostream> // DEBUG

template<typename T>
std::string bytes_to_hex_string(const T& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : bytes) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

template<typename T>
size_t hex_string_to_bytes(const std::string& hex, T &out) {
    if (hex.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have an even number of characters");
    }

    for (size_t i = 0; i < hex.length(); i += 2) {
        uint8_t byte;
        const char* first = hex.data() + i;
        const char* last = hex.data() + i + 2;
        auto result = std::from_chars(first, last, byte, 16);

        if (result.ptr == first) {
             throw std::invalid_argument("Invalid hex character encountered at beginning of pair at position " + std::to_string(i));
        } else if (result.ec == std::errc::invalid_argument) {
            throw std::invalid_argument("Invalid hex character reported by from_chars at position " + std::to_string(i));
        } else if (result.ec != std::errc()) {
            throw std::runtime_error("Unexpected error during hex conversion with error code " + std::to_string(static_cast<int>(result.ec)));
        } else if (result.ptr != last) {
            throw std::invalid_argument("Incomplete or invalid hex pair encountered at position " + std::to_string(i));
        }

        // If all checks pass, conversion of two chars was successful
        out[i/2] = byte;
    }
    return hex.length() / 2;
}
