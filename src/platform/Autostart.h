#pragma once

namespace aia {

// "Start with Windows" via HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
bool IsAutostartEnabled();
bool SetAutostartEnabled(bool enabled);

}  // namespace aia
