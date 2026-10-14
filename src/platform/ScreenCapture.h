#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace aia {

// A frozen 32-bit BGRX image of one monitor, in physical pixels. Kept in memory only.
struct ScreenImage {
  RECT bounds{};  // Virtual-screen coordinates of the captured area
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;  // Top-down, stride = width * 4
  bool Empty() const { return pixels.empty(); }
};

struct ForegroundInfo {
  HWND hwnd = nullptr;
  std::string title;
  std::string process;  // Executable file name, e.g. "Code.exe"
  RECT rect{};          // Visible frame bounds
};

ForegroundInfo QueryForegroundWindow();
ForegroundInfo QueryWindow(HWND hwnd);
int MonitorNumber(HMONITOR monitor);

// Captures the given monitor with GDI. Display affinity is best-effort, not a security guarantee.
ScreenImage CaptureMonitor(HMONITOR monitor);

// Encodes a sub-rectangle (virtual-screen coordinates, clamped to the image) scaled so neither side exceeds
// maxSide (0 = no limit) and additionally by scale (<= 1). Returns empty on failure.
enum class ImageFormat { Jpeg, Png };
std::vector<uint8_t> EncodeRegion(const ScreenImage& image, RECT region, int maxSide, double scale,
                                  ImageFormat format, float jpegQuality = 0.85f);

}  // namespace aia
