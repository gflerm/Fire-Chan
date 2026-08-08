#pragma once

#include <Arduino.h>

namespace firechan {

class StreamSink {
 public:
  virtual ~StreamSink() = default;

  virtual bool beginStream(uint32_t expectedBytes) = 0;
  virtual bool streamWrite(const uint8_t* data, size_t length) = 0;
  virtual void endStream() = 0;
  virtual void abortStream() = 0;
};

}  // namespace firechan
