#ifndef PALA_HAL_FILE_STREAM_H
#define PALA_HAL_FILE_STREAM_H

#include "src/state.h"
#include "src/pure/stream.h"

// Arduino `File` adapter for the pure IReadStream interface.
class FileReadStream : public IReadStream {
public:
  explicit FileReadStream(File& f) : f_(f) {}

  int read() override {
    if (!f_ || !f_.available()) return -1;
    return f_.read();
  }
  bool seek(uint32_t pos) override { return f_.seek(pos); }
  uint32_t position() override { return (uint32_t)f_.position(); }
  size_t size() override { return f_.size(); }
  bool available() override { return f_.available() > 0; }

private:
  File& f_;
};

#endif  // PALA_HAL_FILE_STREAM_H
