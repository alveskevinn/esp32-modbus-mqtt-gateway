// Pure C++ helpers for turning raw Modbus registers into engineering values.
// No Arduino dependency: compiled and unit-tested on the host (`pio test -e native`).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mbdec {

enum class Function : uint8_t {
  Holding = 3,  // FC03 — read holding registers
  Input = 4,    // FC04 — read input registers
};

enum class RegType : uint8_t { U16, S16, U32, S32, F32 };

// 32-bit values span two registers. Vendors disagree on which comes first:
//   ABCD = high word first (Modbus "big endian", most energy meters)
//   CDAB = low word first  (common on inverters and some PLCs)
enum class WordOrder : uint8_t { ABCD, CDAB };

struct Point {
  const char* name;  // JSON key, e.g. "voltage"
  Function fn;
  uint16_t address;  // 0-based register address (protocol address, not 3xxxx/4xxxx)
  RegType type;
  double scale;      // engineering value = raw * scale
  const char* unit;  // informational, published in the discovery payload
};

// A contiguous range of registers fetched with a single request.
struct Block {
  Function fn;
  uint16_t start;
  uint16_t count;
};

// Modbus spec: at most 125 registers per FC03/FC04 response.
constexpr uint16_t kMaxRegistersPerRead = 125;

uint8_t registerCount(RegType type);

// Decodes `registerCount(type)` registers starting at `regs`.
double decode(const uint16_t* regs, RegType type, WordOrder order, double scale);

// Groups points into as few read requests as possible.
// Points with the same function whose ranges are at most `maxGap` registers
// apart are merged, as long as the block stays within kMaxRegistersPerRead.
// Points must be sorted by (fn, address). Returns the number of blocks written,
// or 0 if `out` is too small.
size_t planBlocks(const Point* points, size_t pointCount, uint16_t maxGap, Block* out,
                  size_t outCapacity);

// Index of the block that fully contains `p`, or -1.
int findBlock(const Point& p, const Block* blocks, size_t blockCount);

}  // namespace mbdec
