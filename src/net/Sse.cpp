#include "net/Sse.h"

namespace aia {

void SseParser::Feed(std::string_view chunk) {
  for (char c : chunk) {
    if (c == '\n') {
      if (lastWasCR_) {  // "\r\n": the line was already processed at '\r'
        lastWasCR_ = false;
        continue;
      }
      ProcessLine(buffer_);
      buffer_.clear();
    } else if (c == '\r') {
      ProcessLine(buffer_);
      buffer_.clear();
      lastWasCR_ = true;
      continue;
    } else {
      buffer_.push_back(c);
    }
    lastWasCR_ = false;
  }
}

void SseParser::Finish() {
  if (!buffer_.empty()) {
    ProcessLine(buffer_);
    buffer_.clear();
  }
  Dispatch();
}

void SseParser::ProcessLine(std::string_view line) {
  if (line.empty()) {
    Dispatch();
    return;
  }
  if (line[0] == ':') return;  // comment / keep-alive
  std::string_view field = line;
  std::string_view value;
  if (size_t colon = line.find(':'); colon != std::string_view::npos) {
    field = line.substr(0, colon);
    value = line.substr(colon + 1);
    if (!value.empty() && value[0] == ' ') value.remove_prefix(1);
  }
  if (field == "data") {
    if (hasData_) data_.push_back('\n');
    data_.append(value);
    hasData_ = true;
  } else if (field == "event") {
    event_.assign(value);
  }
  // "id" and "retry" are irrelevant for one-shot streams.
}

void SseParser::Dispatch() {
  if (hasData_) handler_(event_.empty() ? std::string("message") : event_, data_);
  event_.clear();
  data_.clear();
  hasData_ = false;
}

void LineSplitter::Feed(std::string_view chunk) {
  buffer_.append(chunk);
  size_t start = 0;
  for (;;) {
    size_t nl = buffer_.find('\n', start);
    if (nl == std::string::npos) break;
    std::string_view line(buffer_.data() + start, nl - start);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (!line.empty()) handler_(line);
    start = nl + 1;
  }
  buffer_.erase(0, start);
}

void LineSplitter::Finish() {
  if (!buffer_.empty()) {
    std::string rest;
    rest.swap(buffer_);
    if (!rest.empty() && rest.back() == '\r') rest.pop_back();
    if (!rest.empty()) handler_(rest);
  }
}

}  // namespace aia
