#include "poller.h"

#include "config.h"
#include "register_map.h"

static_assert(device::POINT_COUNT <= 32, "PollResult holds at most 32 points");

static Modbus::ResultCode lastResult = Modbus::EX_SUCCESS;

static bool onTransaction(Modbus::ResultCode event, uint16_t, void*) {
  lastResult = event;
  return true;
}

bool Poller::begin() {
  blockCount_ = mbdec::planBlocks(device::POINTS, device::POINT_COUNT, MODBUS_MERGE_GAP, blocks_,
                                  sizeof blocks_ / sizeof blocks_[0]);
  return blockCount_ > 0;
}

bool Poller::readBlock(const mbdec::Block& b, uint8_t slaveId, uint16_t* dst) {
  for (int attempt = 0; attempt <= MODBUS_RETRIES; ++attempt) {
    lastResult = Modbus::EX_TIMEOUT;
    const bool queued = b.fn == mbdec::Function::Input
                            ? mb_.readIreg(slaveId, b.start, dst, b.count, onTransaction)
                            : mb_.readHreg(slaveId, b.start, dst, b.count, onTransaction);
    if (!queued) {
      mb_.task();
      delay(10);
      continue;
    }
    // The library enforces its own response timeout; just pump it.
    while (mb_.slave()) {
      mb_.task();
      delay(1);
    }
    if (lastResult == Modbus::EX_SUCCESS) return true;
    log_w("block %u@%u attempt %d failed: 0x%02X", b.count, b.start, attempt, lastResult);
    delay(50);  // let a slow slave recover before retrying
  }
  return false;
}

void Poller::poll(uint8_t slaveId, PollResult& out) {
  static uint16_t regs[16][mbdec::kMaxRegistersPerRead];
  bool ok[16] = {};
  out.blocksOk = out.blocksFailed = 0;

  for (size_t i = 0; i < blockCount_; ++i) {
    ok[i] = readBlock(blocks_[i], slaveId, regs[i]);
    ok[i] ? out.blocksOk++ : out.blocksFailed++;
  }

  for (size_t i = 0; i < device::POINT_COUNT; ++i) {
    const auto& p = device::POINTS[i];
    const int bi = mbdec::findBlock(p, blocks_, blockCount_);
    out.valid[i] = bi >= 0 && ok[bi];
    out.values[i] = out.valid[i] ? mbdec::decode(&regs[bi][p.address - blocks_[bi].start], p.type,
                                                 device::WORD_ORDER, p.scale)
                                 : 0.0;
  }
}
