#include "capture/SourceManager.h"
#include <iostream>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#endif

namespace Recorder::Capture {

    std::vector<Core::CaptureSourceDescriptor> SourceManager::EnumerateMonitors() {
        std::vector<Core::CaptureSourceDescriptor> monitors;

#if defined(_WIN32)
        struct MonitorEnumContext {
            std::vector<Core::CaptureSourceDescriptor>* list;
            int index = 0;
        } ctx{ &monitors, 0 };

        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT, LPARAM lParam) -> BOOL {
            auto* pCtx = reinterpret_cast<MonitorEnumContext*>(lParam);
            MONITORINFOEXW mi;
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoW(hMon, &mi)) {
                Core::CaptureSourceDescriptor desc;
                desc.type = Core::CaptureSourceType::Monitor;
                desc.nativeHandle = hMon;
                desc.id = std::to_wstring(pCtx->index);
                desc.title = std::wstring(L"Display ") + std::to_wstring(pCtx->index + 1) + L" (" + mi.szDevice + L")";
                desc.nativeWidth = mi.rcMonitor.right - mi.rcMonitor.left;
                desc.nativeHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
                desc.isPrimaryMonitor = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
                desc.region.left = mi.rcMonitor.left;
                desc.region.top = mi.rcMonitor.top;
                desc.region.right = mi.rcMonitor.right;
                desc.region.bottom = mi.rcMonitor.bottom;

                pCtx->list->push_back(desc);
                pCtx->index++;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
#endif

        return monitors;
    }

    std::vector<Core::CaptureSourceDescriptor> SourceManager::EnumerateWindows() {
        std::vector<Core::CaptureSourceDescriptor> windows;

#if defined(_WIN32)
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* list = reinterpret_cast<std::vector<Core::CaptureSourceDescriptor>*>(lParam);

            if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

            int len = GetWindowTextLengthW(hwnd);
            if (len <= 0) return TRUE;

            std::wstring title(len + 1, L'\0');
            GetWindowTextW(hwnd, &title[0], len + 1);
            title.resize(len);

            // Filter out system utility/shell windows
            if (title == L"Program Manager" || title == L"Settings" || title == L"Windows Shell Experience") {
                return TRUE;
            }

            RECT r;
            GetWindowRect(hwnd, &r);
            if ((r.right - r.left) <= 100 || (r.bottom - r.top) <= 100) return TRUE;

            DWORD processId = 0;
            GetWindowThreadProcessId(hwnd, &processId);

            std::wstring procName = L"Unknown";
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
            if (hProc) {
                wchar_t procPath[MAX_PATH];
                DWORD pathLen = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, procPath, &pathLen)) {
                    std::wstring fullPath(procPath);
                    size_t slash = fullPath.find_last_of(L"\\/");
                    if (slash != std::wstring::npos) {
                        procName = fullPath.substr(slash + 1);
                    }
                }
                CloseHandle(hProc);
            }

            Core::CaptureSourceDescriptor desc;
            desc.type = Core::CaptureSourceType::Window;
            desc.nativeHandle = hwnd;
            desc.id = std::to_wstring(reinterpret_cast<uintptr_t>(hwnd));
            desc.title = title;
            desc.processName = procName;
            desc.nativeWidth = r.right - r.left;
            desc.nativeHeight = r.bottom - r.top;
            desc.region.left = r.left;
            desc.region.top = r.top;
            desc.region.right = r.right;
            desc.region.bottom = r.bottom;

            list->push_back(desc);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&windows));
#endif

        return windows;
    }

    Core::CaptureSourceDescriptor SourceManager::GetActiveWindowSource() {
        Core::CaptureSourceDescriptor desc;
#if defined(_WIN32)
        HWND hwnd = GetForegroundWindow();
        if (hwnd) {
            int len = GetWindowTextLengthW(hwnd);
            std::wstring title(len + 1, L'\0');
            if (len > 0) {
                GetWindowTextW(hwnd, &title[0], len + 1);
                title.resize(len);
            }
            RECT r;
            GetWindowRect(hwnd, &r);

            desc.type = Core::CaptureSourceType::Window;
            desc.nativeHandle = hwnd;
            desc.id = std::to_wstring(reinterpret_cast<uintptr_t>(hwnd));
            desc.title = title;
            desc.nativeWidth = r.right - r.left;
            desc.nativeHeight = r.bottom - r.top;
        }
#endif
        return desc;
    }

    Core::CaptureSourceDescriptor SourceManager::CreateRegionSource(const Core::CaptureSourceDescriptor& baseSource, const Core::Rect& region) {
        Core::CaptureSourceDescriptor desc = baseSource;
        desc.type = Core::CaptureSourceType::Region;
        desc.region = region;
        desc.nativeWidth = region.Width();
        desc.nativeHeight = region.Height();
        desc.title = L"Region (" + std::to_wstring(desc.nativeWidth) + L"x" + std::to_wstring(desc.nativeHeight) + L")";
        return desc;
    }

    bool SourceManager::IsProcessRunning(const std::wstring& processName) {
#if defined(_WIN32)
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);

        bool found = false;
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                    found = true;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe));
        }

        CloseHandle(hSnapshot);
        return found;
#else
        return false;
#endif
    }

} // namespace Recorder::Capture
