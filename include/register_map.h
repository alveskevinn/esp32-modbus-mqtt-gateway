// Register map of the device being polled.
//
// The example below matches the common map used by single-phase DIN-rail
// energy meters (IEEE-754 float32, input registers, high word first).
// To poll an inverter or another meter, replace POINTS with its map —
// keep the array sorted by (function, address) so reads can be batched.
#pragma once

#include <modbus_decode.h>

namespace device {

using mbdec::Function;
using mbdec::Point;
using mbdec::RegType;
using mbdec::WordOrder;

constexpr WordOrder WORD_ORDER = WordOrder::ABCD;

// clang-format off
constexpr Point POINTS[] = {
    // name              fn               addr    type          scale unit
    {"voltage",          Function::Input, 0x0000, RegType::F32, 1.0,  "V"},
    {"current",          Function::Input, 0x0006, RegType::F32, 1.0,  "A"},
    {"active_power",     Function::Input, 0x000C, RegType::F32, 1.0,  "W"},
    {"apparent_power",   Function::Input, 0x0012, RegType::F32, 1.0,  "VA"},
    {"reactive_power",   Function::Input, 0x0018, RegType::F32, 1.0,  "var"},
    {"power_factor",     Function::Input, 0x001E, RegType::F32, 1.0,  ""},
    {"frequency",        Function::Input, 0x0046, RegType::F32, 1.0,  "Hz"},
    {"import_energy",    Function::Input, 0x0048, RegType::F32, 1.0,  "kWh"},
    {"export_energy",    Function::Input, 0x004A, RegType::F32, 1.0,  "kWh"},
};
// clang-format on

constexpr size_t POINT_COUNT = sizeof(POINTS) / sizeof(POINTS[0]);

}  // namespace device
