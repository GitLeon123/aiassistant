#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace aia {

using json = nlohmann::json;

std::wstring Widen(std::string_view utf8);
std::string Narrow(std::wstring_view wide);

std::string Base64Encode(const uint8_t* data, size_t size);
inline std::string Base64Encode(const std::vector<uint8_t>& data) {
  return Base64Encode(data.data(), data.size());
}
std::vector<uint8_t> Base64Decode(std::string_view text);

std::string ToLower(std::string_view s);
bool StartsWith(std::string_view s, std::string_view prefix);
bool Contains(std::string_view s, std::string_view needle);
std::string TrimRight(std::string_view s, std::string_view chars = "/ ");

// Returns a JSON value at a "/"-separated path, or the fallback if any segment is missing.
template <typename T>
T JsonGet(const json& j, std::string_view path, T fallback) {
  const json* cur = &j;
  size_t start = 0;
  while (start <= path.size()) {
    size_t end = path.find('/', start);
    if (end == std::string_view::npos) end = path.size();
    std::string key(path.substr(start, end - start));
    if (!cur->is_object()) return fallback;
    auto it = cur->find(key);
    if (it == cur->end()) return fallback;
    cur = &*it;
    start = end + 1;
  }
  try {
    if (cur->is_null()) return fallback;
    return cur->get<T>();
  } catch (...) {
    return fallback;
  }
}

// Cooperative cancellation shared between the UI thread and a worker.
// Workers register a callback (close an HTTP handle, terminate a job) that
// Cancel() invokes so blocking calls return promptly.
class CancelToken {
 public:
  void Cancel() {
    std::function<void()> cb;
    {
      std::lock_guard lock(mutex_);
      if (cancelled_.exchange(true)) return;
      cb = onCancel_;
    }
    if (cb) cb();
  }
  bool IsCancelled() const { return cancelled_.load(); }
  // Returns false (and does not register) if already cancelled.
  bool SetOnCancel(std::function<void()> cb) {
    std::lock_guard lock(mutex_);
    if (cancelled_) return false;
    onCancel_ = std::move(cb);
    return true;
  }
  void ClearOnCancel() {
    std::lock_guard lock(mutex_);
    onCancel_ = nullptr;
  }

 private:
  std::atomic<bool> cancelled_{false};
  std::mutex mutex_;
  std::function<void()> onCancel_;
};

}  // namespace aia
