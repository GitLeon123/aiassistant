#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace aia {

// Incremental Server-Sent Events parser (WHATWG event-stream format).
// Feed arbitrary byte chunks; complete events are dispatched to the callback.
class SseParser {
 public:
  using Handler = std::function<void(const std::string& event, const std::string& data)>;
  explicit SseParser(Handler handler) : handler_(std::move(handler)) {}

  void Feed(std::string_view chunk);
  // Dispatches a trailing event that was not terminated by a blank line.
  void Finish();

 private:
  void ProcessLine(std::string_view line);
  void Dispatch();

  Handler handler_;
  std::string buffer_;
  std::string event_;
  std::string data_;
  bool hasData_ = false;
  bool lastWasCR_ = false;
};

// Splits a byte stream into newline-terminated lines (JSON Lines from CLI tools).
class LineSplitter {
 public:
  using Handler = std::function<void(std::string_view line)>;
  explicit LineSplitter(Handler handler) : handler_(std::move(handler)) {}
  void Feed(std::string_view chunk);
  void Finish();

 private:
  Handler handler_;
  std::string buffer_;
};

}  // namespace aia
