#pragma once

#include <cstdint>

namespace cfg {
constexpr uint16_t EC_PORT_CMD  = 0x66;
constexpr uint16_t EC_PORT_DATA = 0x62;

constexpr uint8_t EC_CMD_READ  = 0x80;
constexpr uint8_t EC_CMD_WRITE = 0x81;

constexpr uint8_t EC_STATUS_OBF = 0x01; // output buffer full
constexpr uint8_t EC_STATUS_IBF = 0x02; // input buffer full

// MSI Full Blast: EC register 0x98, bit 7.
constexpr uint8_t FULLBLAST_REG = 0x98;
constexpr uint8_t FULLBLAST_BIT = 0x80;

// Keyboard hook: Right Ctrl = scancode 0x11 + LLKHF_EXTENDED.
constexpr uint16_t TARGET_SCANCODE = 0x11;
constexpr bool     TARGET_EXTENDED = true;
} // namespace cfg
