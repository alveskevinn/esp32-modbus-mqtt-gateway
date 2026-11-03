#include "modbus_decode.h"

#include <string.h>

namespace mbdec {

uint8_t registerCount(RegType type) {
  switch (type) {
    case RegType::U16:
    case RegType::S16:
      return 1;
    case RegType::U32:
    case RegType::S32:
    case RegType::F32:
      return 2;
  }
  return 1;
}

static uint32_t combine(const uint16_t* regs, WordOrder order) {
  const uint32_t first = regs[0];
  const uint32_t second = regs[1];
  return order == WordOrder::ABCD ? (first << 16) | second : (second << 16) | first;
}

double decode(const uint16_t* regs, RegType type, WordOrder order, double scale) {
  double raw = 0.0;
  switch (type) {
    case RegType::U16:
      raw = regs[0];
      break;
    case RegType::S16:
      raw = static_cast<int16_t>(regs[0]);
      break;
    case RegType::U32:
      raw = combine(regs, order);
      break;
    case RegType::S32:
      raw = static_cast<int32_t>(combine(regs, order));
      break;
    case RegType::F32: {
      const uint32_t bits = combine(regs, order);
      float f;
      memcpy(&f, &bits, sizeof f);  // well-defined type punning
      raw = f;
      break;
    }
  }
  return raw * scale;
}

size_t planBlocks(const Point* points, size_t pointCount, uint16_t maxGap, Block* out,
                  size_t outCapacity) {
  size_t n = 0;
  for (size_t i = 0; i < pointCount; ++i) {
    const Point& p = points[i];
    const uint16_t end = p.address + registerCount(p.type);  // exclusive

    if (n > 0) {
      Block& last = out[n - 1];
      const uint16_t lastEnd = last.start + last.count;
      const bool sameFn = last.fn == p.fn;
      const bool closeEnough = p.address <= lastEnd + maxGap;
      const bool fits = end - last.start <= kMaxRegistersPerRead;
      if (sameFn && closeEnough && fits) {
        if (end > lastEnd) last.count = end - last.start;
        continue;
      }
    }

    if (n == outCapacity) return 0;
    out[n++] = Block{p.fn, p.address, static_cast<uint16_t>(end - p.address)};
  }
  return n;
}

int findBlock(const Point& p, const Block* blocks, size_t blockCount) {
  const uint16_t end = p.address + registerCount(p.type);
  for (size_t i = 0; i < blockCount; ++i) {
    const Block& b = blocks[i];
    if (b.fn == p.fn && p.address >= b.start && end <= b.start + b.count) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}  // namespace mbdec
