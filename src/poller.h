#pragma once

#include <ModbusRTU.h>
#include <modbus_decode.h>

struct PollResult {
  double values[32];
  bool valid[32];
  uint8_t blocksOk;
  uint8_t blocksFailed;
};

// Reads every point of the register map from one slave, batching contiguous
// registers into as few requests as possible. Blocking: worst case is
// blocks * (retries + 1) * MODBUS_TIMEOUT_MS.
class Poller {
 public:
  explicit Poller(ModbusRTU& mb) : mb_(mb) {}

  // Builds the read plan. Returns false if the map does not fit the buffers.
  bool begin();
  void poll(uint8_t slaveId, PollResult& out);
  size_t blockCount() const { return blockCount_; }

 private:
  bool readBlock(const mbdec::Block& b, uint8_t slaveId, uint16_t* dst);

  ModbusRTU& mb_;
  mbdec::Block blocks_[16];
  size_t blockCount_ = 0;
};
