#include <windows.h>
#include <windowsx.h>
#include <wtsapi32.h>
#include <powrprof.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include "windows/assets.hpp"
#include "core/animator.hpp"
#include "core/playback_sample.hpp"
#include "core/exploration_package.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using namespace pet;
constexpr UINT TrayMessage = WM_APP + 1;
constexpr UINT ClipBase = 100, DirectionBase = 500;
constexpr UINT Loop = 600, Finite = 601, Stop = 602, Hide = 603, Top = 604;
constexpr UINT Scale1 = 610, Scale2 = 611, Scale3 = 612, Background = 620, Reload = 623, Quit = 624;
constexpr unsigned PhysicalSize = 200;
const GUID SessionDisplayGuid{0x2b84c20e, 0xad23, 0x4ddf, {0x93, 0xdb, 0x05, 0xff, 0xbd, 0x7e, 0xfc, 0xa5}};
int64_t nowUs() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
void winCheck(bool ok, const char* operation) {
    if (!ok) throw std::runtime_error(std::string(operation) + " OS=" + std::to_string(GetLastError()));
}
std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
    winCheck(n != 0, "UTF-8");
    std::wstring out(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), out.data(), n);
    return out;
}
struct Handle {
    HANDLE value = nullptr;
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct ComScope {
    ComScope() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) throw std::runtime_error("CoInitializeEx HRESULT=" + std::to_string(hr));
    }
    ~ComScope() { CoUninitialize(); }
};
struct Dib {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ old = nullptr;
    void* bits = nullptr;
    unsigned size;
    explicit Dib(unsigned n) : size(n) {
        dc = CreateCompatibleDC(nullptr);
        winCheck(dc != nullptr, "CreateCompatibleDC");
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = LONG(n); info.bmiHeader.biHeight = -LONG(n);
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap) { DeleteDC(dc); dc = nullptr; winCheck(false, "CreateDIBSection"); }
        old = SelectObject(dc, bitmap);
        if (!old || old == HGDI_ERROR) {
            DeleteObject(bitmap); DeleteDC(dc); dc = nullptr; bitmap = nullptr;
            winCheck(false, "SelectObject");
        }
    }
    ~Dib() { if (dc) { SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc); } }
    Dib(const Dib&) = delete;
};
std::filesystem::path executableDir() {
    std::wstring path(32768, L'\0');
    DWORD n = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    winCheck(n != 0 && n < path.size(), "GetModuleFileName");
    path.resize(n); return std::filesystem::path(path).parent_path();
}
std::filesystem::path appDir() {
    PWSTR raw = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw);
    if (FAILED(hr)) throw std::runtime_error("LocalAppData HRESULT=" + std::to_string(hr));
    std::filesystem::path result = std::filesystem::path(raw) / L"MysteryDungeonPet";
    CoTaskMemFree(raw); std::filesystem::create_directories(result); return result;
}
struct Options {
    std::filesystem::path package = executableDir() / L"assets/original/asset-0/asset.mdpa";
    std::filesystem::path metrics, report;
    std::filesystem::path exploration, snapshot;
    unsigned seed = 1;
    bool newRun = false;
    unsigned scale = 3, direction = 0;
    uint32_t clip = 0;
    bool loop = true, validate = false, scaleExplicit = false;
    unsigned duration = 0;
    std::wstring scenario = L"manual";
};
unsigned number(const std::wstring& s, unsigned max) {
    size_t used = 0;
    auto n = std::stoul(s, &used);
    if (used != s.size() || n > max || s.empty() || s[0] == L'-') throw std::runtime_error("invalid numeric option");
    return unsigned(n);
}
Options options() {
    Options o;
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    winCheck(args != nullptr, "CommandLineToArgv");
    std::unique_ptr<wchar_t*, decltype(&LocalFree)> owner(args, LocalFree);
    for (int i = 1; i < count; ++i) {
        std::wstring arg = args[i];
        auto value = [&]() -> std::wstring { if (++i == count) throw std::runtime_error("missing option value"); return args[i]; };
        if (arg == L"--package") o.package = value();
        else if (arg == L"--explore") o.exploration = value();
        else if (arg == L"--snapshot") o.snapshot = value();
        else if (arg == L"--seed") o.seed = number(value(), UINT32_MAX);
        else if (arg == L"--new-run") o.newRun = true;
        else if (arg == L"--metrics") o.metrics = value();
        else if (arg == L"--report") o.report = value();
        else if (arg == L"--clip") o.clip = number(value(), INT32_MAX);
        else if (arg == L"--direction") o.direction = number(value(), 7);
        else if (arg == L"--size") { if (number(value(), PhysicalSize) != PhysicalSize) throw std::runtime_error("physical window size is fixed at 200"); }
        else if (arg == L"--scale") { o.scale = number(value(), 8); o.scaleExplicit = true; }
        else if (arg == L"--duration") o.duration = number(value(), 86400);
        else if (arg == L"--scenario") o.scenario = value();
        else if (arg == L"--finite") o.loop = false;
        else if (arg == L"--validate-package") o.validate = true;
        else throw std::runtime_error("unknown command-line option");
    }
    if (!o.scale) throw std::runtime_error("scale must be 1..8");
    if (!o.exploration.empty() && (o.validate || o.scenario != L"manual"))
        throw std::runtime_error("exploration requires manual scenario");
    if (o.scenario != L"manual" && o.scenario != L"idle" && o.scenario != L"walk" && o.scenario != L"empty" && o.scenario != L"switch" && o.scenario != L"hide")
        throw std::runtime_error("scenario must be manual/idle/walk/empty/switch/hide");
    return o;
}
uint64_t fileTime(FILETIME t) { return (uint64_t(t.dwHighDateTime) << 32) | t.dwLowDateTime; }
uint64_t cpuTime() {
    FILETIME created{}, ended{}, kernel{}, user{};
    winCheck(GetProcessTimes(GetCurrentProcess(), &created, &ended, &kernel, &user) != 0, "GetProcessTimes");
    return fileTime(kernel) + fileTime(user);
}
class App {
    HINSTANCE instance_;
    HWND window_ = nullptr;
    Options options_;
    std::filesystem::path directory_;
    Assets assets_;
    std::optional<ExplorationPackage> explorationPackage_;
    std::unique_ptr<Explorer> explorer_;
    std::map<uint32_t, Assets> tiles_;
    std::array<uint32_t, kTiles> chunks_{};
    uint64_t chunkVersion_ = 0;
    int64_t exploreDeadline_ = 0;
    bool poseFinished_ = false;
    Surface itemMarker_{5, 5}, friendMarker_{5, 5}, stairsMarker_{7, 7};
    Animator animator_;
    Surface surface_, background_{32, 24};
    std::unique_ptr<Dib> dib_;
    Handle timer_;
    HPOWERNOTIFY power_ = nullptr;
    bool session_ = false, tray_ = false, hidden_ = false, locked_ = false, displayOff_ = false, suspended_ = false, paused_ = false;
    bool topmost_ = true, backgroundOn_ = false, dragging_ = false, dirty_ = true, submitted_ = false, running_ = true;
    POINT dragCursor_{}, dragWindow_{};
    uint32_t clip_;
    unsigned direction_, scale_;
    const unsigned size_ = PhysicalSize;
    bool loop_;
    size_t frame_ = 0;
    std::optional<int64_t> nextFrameDeadline_;
    bool finalSampled_ = false;
    uint64_t submissions_ = 0, decodes_ = 0, markers_ = 0, finishes_ = 0, skippedCycles_ = 0, switches_ = 0;
    uint64_t hiddenSubmissions_ = 0;
    int64_t start_ = 0, metricDeadline_ = 0, stopDeadline_ = 0, scenarioDeadline_ = 0, sessionRetry_ = 0;
    unsigned sessionAttempts_ = 0;
    int64_t previousMetric_ = 0;
    uint64_t previousCpu_ = 0;
    std::ofstream metrics_, events_;
    static LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            self = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            self->window_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, msg, w, l);
        try { return self->dispatch(msg, w, l); }
        catch (const std::exception& e) { self->log(std::string("error ") + e.what()); return 0; }
    }
    void log(const std::string& s) {
        if (events_) { events_ << nowUs() << " " << s << "\n"; events_.flush(); }
        OutputDebugStringW((wide(s) + L"\n").c_str());
    }
    bool blocked() const { return hidden_ || locked_ || displayOff_ || suspended_; }
    void synchronizePause(int64_t now) {
        bool p = blocked();
        if (p == paused_) return;
        paused_ = p;
        if (p) { animator_.pause(now); log("render_pause"); }
        else { animator_.resume(now); log("render_resume (not user activity)"); }
        if (!p && explorer_) exploreDeadline_ = now + 350000;
        // Hidden window keeps its last submitted pixels; do not resubmit them on restore.
    }
    void startClip(int64_t now) {
        frame_ = 0; nextFrameDeadline_.reset();
        if (options_.scenario == L"empty") animator_.stop();
        else {
            animator_.start(assets_.package, clip_, direction_, loop_, now);
            if (paused_) animator_.pause(now);
        }
        dirty_ = true;
        log("select clip=" + std::to_string(clip_) + " direction=" + std::to_string(direction_) + " loop=" + std::to_string(loop_));
    }
    PlaybackSample currentPlayback() const {
        Point scene{int32_t(size_ / (2 * scale_)), int32_t(size_ / (2 * scale_))};
        if (explorer_ && explorer_->state().phase != Phase::Home) {
            const int tile = explorer_->state().position;
            scene = {tile % kWidth * 24 + 12, tile / kWidth * 24 + 12};
        }
        return playbackSample(assets_.package, clip_, direction_, frame_, animator_.active(), scene, options_.scenario != L"empty");
    }
    void clamp(POINT desired) {
        RECT target{desired.x, desired.y, desired.x + LONG(size_), desired.y + LONG(size_)};
        MONITORINFO info{}; info.cbSize = sizeof(info);
        winCheck(GetMonitorInfoW(MonitorFromRect(&target, MONITOR_DEFAULTTONEAREST), &info) != 0, "monitor work area");
        LONG x = std::clamp(desired.x, info.rcWork.left, std::max(info.rcWork.left, info.rcWork.right - LONG(size_)));
        LONG y = std::clamp(desired.y, info.rcWork.top, std::max(info.rcWork.top, info.rcWork.bottom - LONG(size_)));
        winCheck(SetWindowPos(window_, nullptr, x, y, int(size_), int(size_), SWP_NOACTIVATE | SWP_NOZORDER) != 0, "window position");
    }
    void save() {
        if (explorer_) saveSnapshot(options_.snapshot, explorer_->state());
        RECT rect{};
        winCheck(GetWindowRect(window_, &rect) != 0, "save window position");
        std::ostringstream s;
        s << "MDPS 1 " << rect.left << ' ' << rect.top << ' ' << size_ << ' ' << scale_ << ' ' << (topmost_ ? 1 : 0) << '\n';
        std::string text = s.str();
        const auto temporary = directory_ / L"window.state.tmp", final = directory_ / L"window.state";
        Handle file;
        file.value = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        winCheck(file.value != INVALID_HANDLE_VALUE, "state CreateFile");
        DWORD written = 0;
        winCheck(WriteFile(file.value, text.data(), DWORD(text.size()), &written, nullptr) && written == text.size(), "state WriteFile");
        winCheck(FlushFileBuffers(file.value) != 0, "state flush");
        CloseHandle(file.value); file.value = nullptr;
        winCheck(MoveFileExW(temporary.c_str(), final.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0, "state replace");
    }
    POINT restoredPosition() {
        POINT position{100, 100};
        auto path = directory_ / L"window.state";
        if (!std::filesystem::exists(path)) return position;
        try {
            auto bytes = readFile(path, 256);
            std::istringstream f(std::string(bytes.begin(), bytes.end()));
            std::string magic, trailing;
            int version, x, y, size, scale, top;
            if (!(f >> magic >> version >> x >> y >> size >> scale >> top) || (f >> trailing) || magic != "MDPS" || version != 1 ||
                x < -1000000 || x > 1000000 || y < -1000000 || y > 1000000 || (size != 100 && size != 200) || scale < 1 || scale > 8 || (top != 0 && top != 1))
                throw std::runtime_error("invalid window state/version");
            // Version 1's 100px state remains readable, but cannot resize the fixed viewport.
            if (!options_.scaleExplicit && options_.scenario == L"manual" && !explorer_) scale_ = unsigned(scale);
            position = {x, y}; topmost_ = top != 0;
        } catch (const std::exception& e) { log(std::string("state ignored: ") + e.what()); }
        return position;
    }
    void show(bool hide) {
        hidden_ = hide;
        ShowWindow(window_, hide ? SW_HIDE : SW_SHOWNOACTIVATE);
        synchronizePause(nowUs());
        log(hide ? "hide" : "show");
    }
    void popup() {
        HMENU menu = CreatePopupMenu(), clips = CreatePopupMenu(), directions = CreatePopupMenu();
        if (!menu || !clips || !directions) {
            if (menu) DestroyMenu(menu);
            if (clips) DestroyMenu(clips);
            if (directions) DestroyMenu(directions);
            winCheck(false, "CreatePopupMenu");
        }
        for (size_t i = 0; i < assets_.package.clips.size(); ++i) {
            const auto& c = assets_.package.clips[i];
            AppendMenuW(clips, MF_STRING | (c.id == clip_ ? MF_CHECKED : 0), ClipBase + i, wide(c.name).c_str());
        }
        const wchar_t* names[] = {L"Down", L"DownRight", L"Right", L"UpRight", L"Up", L"UpLeft", L"Left", L"DownLeft"};
        for (unsigned i = 0; i < 8; ++i) AppendMenuW(directions, MF_STRING | (i == direction_ ? MF_CHECKED : 0), DirectionBase + i, names[i]);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(clips), L"Animation");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(directions), L"Direction");
        auto add = [&](UINT id, const wchar_t* text, bool checked = false) { AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), id, text); };
        add(Loop, L"Loop", loop_); add(Finite, L"Finite", !loop_); add(Stop, L"Stop (explicit Sleep end)");
        add(Hide, hidden_ ? L"Show" : L"Hide"); add(Top, L"Always on top", topmost_);
        add(Scale1, L"Scale 1", scale_ == 1); add(Scale2, L"Scale 2", scale_ == 2); add(Scale3, L"Scale 3", scale_ == 3);
        add(Background, L"Test background", backgroundOn_);
        add(Reload, L"Reload package"); add(Quit, L"Exit");
        POINT cursor{}; GetCursorPos(&cursor);
        // Foreground is needed for tray menus to dismiss correctly; window remains NOACTIVATE.
        SetForegroundWindow(window_);
        UINT cmd = UINT(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window_, nullptr));
        DestroyMenu(menu); PostMessageW(window_, WM_NULL, 0, 0);
        if (cmd) command(cmd);
    }
    void command(UINT id) {
        if (explorer_ && (id == Loop || id == Finite || id == Stop || id == Reload || id == Background ||
            (id >= ClipBase && id < ClipBase + assets_.package.clips.size()) ||
            (id >= DirectionBase && id < DirectionBase + 8))) return;
        int64_t now = nowUs();
        if (id >= ClipBase && id < ClipBase + assets_.package.clips.size()) {
            clip_ = assets_.package.clips[id - ClipBase].id; startClip(now);
        } else if (id >= DirectionBase && id < DirectionBase + 8) { direction_ = id - DirectionBase; startClip(now); }
        else switch (id) {
            case Loop: loop_ = true; startClip(now); break;
            case Finite: loop_ = false; startClip(now); break;
            case Stop: animator_.stop(); nextFrameDeadline_.reset(); log("explicit stop"); break;
            case Hide: show(!hidden_); break;
            case Top:
                topmost_ = !topmost_;
                winCheck(SetWindowPos(window_, topmost_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != 0, "topmost"); break;
            case Scale1: case Scale2: case Scale3: scale_ = id - Scale1 + 1; dirty_ = true; break;
            case Background: backgroundOn_ = !backgroundOn_; dirty_ = true; break;
            case Reload: {
                Assets candidate = Assets::load(options_.package, &assets_);
                candidate.package.frames(clip_, direction_); // Reject before replacing the last good state.
                animator_.stop(); assets_ = std::move(candidate); decodes_ += assets_.decoded ? 1 : 0;
                startClip(now); log("package reloaded assetKey=" + assets_.package.assetKey); break;
            }
            case Quit: PostMessageW(window_, WM_CLOSE, 0, 0); break;
            default: break;
        }
    }
    LRESULT dispatch(UINT msg, WPARAM w, LPARAM l) {
        switch (msg) {
            case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
            case WM_NCHITTEST: {
                POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)}; ScreenToClient(window_, &p);
                if (p.x < 0 || p.y < 0 || p.x >= LONG(size_) || p.y >= LONG(size_) || surface_.pixels[(size_t(p.y) * size_ + p.x) * 4 + 3] == 0) return HTTRANSPARENT;
                return HTCLIENT;
            }
            case WM_LBUTTONDOWN: {
                RECT rect{}; winCheck(GetWindowRect(window_, &rect) != 0, "drag rect");
                GetCursorPos(&dragCursor_); dragWindow_ = {rect.left, rect.top}; dragging_ = true; SetCapture(window_); return 0;
            }
            case WM_MOUSEMOVE:
                if (dragging_) { POINT p{}; GetCursorPos(&p); clamp({dragWindow_.x + p.x - dragCursor_.x, dragWindow_.y + p.y - dragCursor_.y}); } return 0;
            case WM_LBUTTONUP:
                if (dragging_) { dragging_ = false; ReleaseCapture(); save(); } return 0;
            case WM_CAPTURECHANGED: dragging_ = false; return 0;
            case WM_RBUTTONUP: popup(); return 0;
            case TrayMessage:
                if (l == WM_LBUTTONDBLCLK) show(false);
                if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) popup();
                return 0;
            case WM_DPICHANGED: {
                const RECT* suggested = reinterpret_cast<const RECT*>(l);
                clamp({suggested->left, suggested->top}); // Physical size/nearest integer scale do not change with DPI.
                log("DPI=" + std::to_string(HIWORD(w))); return 0;
            }
            case WM_DISPLAYCHANGE: {
                RECT r{}; winCheck(GetWindowRect(window_, &r) != 0, "display rect"); clamp({r.left, r.top}); return 0;
            }
            case WM_WTSSESSION_CHANGE:
                if (w == WTS_SESSION_LOCK) locked_ = true;
                else if (w == WTS_SESSION_UNLOCK) locked_ = false;
                else if (w == WTS_CONSOLE_DISCONNECT || w == WTS_REMOTE_DISCONNECT) locked_ = true;
                else if (w == WTS_CONSOLE_CONNECT || w == WTS_REMOTE_CONNECT) initialSessionState();
                else return 0;
                synchronizePause(nowUs()); log("session=" + std::to_string(w)); return 0;
            case WM_POWERBROADCAST:
                if (w == PBT_APMSUSPEND) {
                    suspended_ = true; displayOff_ = true; synchronizePause(nowUs());
                    try { save(); } catch (const std::exception& e) { log(std::string("suspend state save failed: ") + e.what()); }
                }
                else if (w == PBT_APMRESUMEAUTOMATIC || w == PBT_APMRESUMESUSPEND) suspended_ = false;
                else if (w == PBT_POWERSETTINGCHANGE) {
                    auto setting = reinterpret_cast<POWERBROADCAST_SETTING*>(l);
                    if (setting->PowerSetting == SessionDisplayGuid && setting->DataLength == sizeof(DWORD)) {
                        DWORD status; std::memcpy(&status, setting->Data, sizeof(status));
                        if (status > 2) { log("unknown display status"); return TRUE; }
                        displayOff_ = status == 0;
                    }
                }
                synchronizePause(nowUs()); log("power=" + std::to_string(w)); return TRUE;
            case WM_SHOWWINDOW: hidden_ = w == 0; synchronizePause(nowUs()); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: { PAINTSTRUCT p{}; BeginPaint(window_, &p); EndPaint(window_, &p); return 0; }
            case WM_CLOSE:
                finalSample(); // HWND와 DPI가 유효할 때 마지막 행을 기록한다.
                try { save(); } catch (const std::exception& e) { log(std::string("state save failed: ") + e.what()); }
                DestroyWindow(window_); return 0;
            case WM_DESTROY:
                if (!finalSampled_ && start_) finalSample();
                if (session_) { WTSUnRegisterSessionNotification(window_); session_ = false; }
                if (power_) { UnregisterPowerSettingNotification(power_); power_ = nullptr; }
                if (tray_) { NOTIFYICONDATAW icon{}; icon.cbSize = sizeof(icon); icon.hWnd = window_; icon.uID = 1; Shell_NotifyIconW(NIM_DELETE, &icon); tray_ = false; }
                running_ = false; PostQuitMessage(0); return 0;
            default: return DefWindowProcW(window_, msg, w, l);
        }
    }
    void finalSample() {
        if (finalSampled_) return;
        sample(nowUs(), true);
        finalSampled_ = true;
    }
    void sample(int64_t now, bool final = false) {
        PROCESS_MEMORY_COUNTERS_EX mem{}; mem.cb = sizeof(mem);
        winCheck(GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem), sizeof(mem)) != 0, "memory metrics");
        uint64_t cpu = cpuTime();
        const double wall = double(now - previousMetric_) / 1000000;
        const double oneCore = wall > 0 ? 100.0 * double(cpu - previousCpu_) / (wall * 10000000) : 0;
        DWORD cores = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (!cores) throw std::runtime_error("logical core count unavailable");
        DWORD handles = 0;
        winCheck(GetProcessHandleCount(GetCurrentProcess(), &handles) != 0, "handle count");
        RECT rect{};
        winCheck(GetWindowRect(window_, &rect) != 0, "metrics window rect");
        if (rect.right - rect.left != LONG(size_) || rect.bottom - rect.top != LONG(size_))
            throw std::runtime_error("metrics physical window size differs from fixed viewport");
        const auto dpi = GetDpiForWindow(window_);
        winCheck(dpi != 0, "metrics DPI");
        if (metrics_) {
            metrics_ << (now - start_) / 1000000.0 << ',' << oneCore << ',' << oneCore / cores << ',' << cores << ','
                << mem.WorkingSetSize << ',' << mem.PrivateUsage << ',' << mem.PeakWorkingSetSize << ',' << submissions_ << ',' << decodes_ << ','
                << markers_ << ',' << finishes_ << ',' << skippedCycles_ << ',' << handles << ',' << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) << ','
                << blocked() << ',' << hiddenSubmissions_ << ',' << dpi << ',' << size_ << ',' << scale_ << ','
                << rect.left << ',' << rect.top << ',' << switches_ << ',' << final << ',';
            currentPlayback().writeCsv(metrics_);
            metrics_ << '\n';
            metrics_.flush();
        }
        previousMetric_ = now; previousCpu_ = cpu;
    }
    void initialSessionState() {
        LPWSTR raw = nullptr; DWORD bytes = 0;
        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionInfoEx, &raw, &bytes)) {
            if (bytes >= sizeof(WTSINFOEXW)) {
                auto info = reinterpret_cast<WTSINFOEXW*>(raw);
                if (info->Level == 1) {
                    const auto flags = info->Data.WTSInfoExLevel1.SessionFlags;
                    locked_ = flags != WTS_SESSIONSTATE_UNLOCK;
                    log("initial session flags=" + std::to_string(flags));
                } else locked_ = true;
            } else locked_ = true;
            WTSFreeMemory(raw);
        } else {
            locked_ = true;
            log("initial session unknown; render paused OS=" + std::to_string(GetLastError()));
        }
    }
    void registerSession(int64_t now) {
        ++sessionAttempts_;
        session_ = WTSRegisterSessionNotification(window_, NOTIFY_FOR_THIS_SESSION) != 0;
        if (session_) { sessionRetry_ = 0; return; }
        log("WTSRegisterSessionNotification OS=" + std::to_string(GetLastError()));
        sessionRetry_ = sessionAttempts_ < 3 ? now + 5000000 : 0;
    }
    void advanceExploration(int64_t now) {
        if (!explorer_ || blocked() || now < exploreDeadline_ || explorer_->state().phase == Phase::Home) return;
        const int oldPosition = explorer_->state().position;
        auto candidate = *explorer_;
        const auto event = candidate.advance(poseFinished_);
        saveSnapshot(options_.snapshot, candidate.state());
        *explorer_ = std::move(candidate); // commit state before exposing inventory/clear event
        exploreDeadline_ = now + 350000;
        const auto phase = explorer_->state().phase;
        uint32_t nextClip = 7; bool nextLoop = true;
        if (phase == Phase::Pose) { nextClip = 17; nextLoop = false; }
        else if (event.kind == EventKind::Move) nextClip = 0;
        if (event.kind == EventKind::Move) {
            const auto pos = explorer_->state().position;
            const int dx = pos % kWidth - oldPosition % kWidth, dy = pos / kWidth - oldPosition / kWidth;
            const unsigned direction = dx > 0 ? 2 : dx < 0 ? 6 : dy < 0 ? 4 : 0;
            if (direction_ != direction) { direction_ = direction; startClip(now); }
        }
        if (clip_ != nextClip || loop_ != nextLoop) { clip_ = nextClip; loop_ = nextLoop; poseFinished_ = false; startClip(now); }
        if (!event.onceKey.empty()) log("explore onceKey=" + event.onceKey + " originalId=" + std::to_string(event.originalId));
        if (event.kind == EventKind::ReturnedHome) log("explore home completed=" + std::to_string(explorer_->state().completed));
        dirty_ = true;
    }
    void render(int64_t now) {
        synchronizePause(now);
        if (blocked()) return;
        advanceExploration(now);
        if (animator_.active()) {
            auto result = animator_.advance(now);
            dirty_ = dirty_ || frame_ != result.frame; frame_ = result.frame;
            markers_ += result.markers.size(); finishes_ += result.finishedEvent ? 1 : 0; skippedCycles_ += result.skippedCycles;
            nextFrameDeadline_ = result.nextDeadlineUs;
            if (result.finishedEvent) log("finite finished assetKey=" + assets_.package.assetKey);
            if (result.finishedEvent && explorer_ && explorer_->state().phase == Phase::Pose) poseFinished_ = true;
        }
        if (!dirty_) return;
        std::vector<Layer> layers;
        Point camera{0, 0};
        if (explorer_ && explorer_->state().phase != Phase::Home) {
            const auto& state = explorer_->state();
            const auto tileId = state.floors[state.floorIndex].tilesetId;
            const auto& tileAssets = tiles_.at(tileId);
            if (chunkVersion_ != state.world.version) {
                chunks_ = chunkGrid(state.world, explorationPackage_->tileset(tileId));
                chunkVersion_ = state.world.version;
            }
            camera = {state.position % kWidth * 24 + 12 - int32_t(size_ / (2 * scale_)),
                      state.position / kWidth * 24 + 12 - int32_t(size_ / (2 * scale_))};
            auto visible = [&](Point p, int size) {
                return p.x + size >= camera.x && p.y + size >= camera.y &&
                       p.x < camera.x + int(size_ / scale_) + 1 && p.y < camera.y + int(size_ / scale_) + 1;
            };
            const auto& frames = tileAssets.package.frames(0, 0);
            for (int tile = 0; tile < kTiles; ++tile) {
                const Point p{tile % kWidth * 24, tile / kWidth * 24};
                if (visible(p, 24)) layers.push_back({tileAssets.atlas.get(), frames[chunks_[tile]].rect, {0, 0}, p});
            }
            const Point stairPos{state.world.stairs % kWidth * 24 + 9, state.world.stairs / kWidth * 24 + 9};
            if (visible(stairPos, 7)) layers.push_back({&stairsMarker_, {0, 0, 7, 7}, {0, 0}, stairPos});
            for (const auto& e : state.population.entities) if (!e.done) {
                const Point p{e.tile % kWidth * 24 + 10, e.tile / kWidth * 24 + 10};
                if (visible(p, 5)) layers.push_back({e.kind == EntityKind::Item ? &itemMarker_ : &friendMarker_,
                                                   {0, 0, 5, 5}, {0, 0}, p});
            }
        }
        if (backgroundOn_ && options_.scenario != L"empty") layers.push_back({&background_, {0, 0, 32, 24}, {0, 0}, {0, 0}});
        const auto playback = currentPlayback();
        if (playback.frame) {
            const auto& f = *playback.frame;
            layers.push_back({assets_.atlas.get(), f.rect, f.anchor, playback.scenePosition});
        }
        bool changed = compose(surface_, layers, camera, scale_);
        dirty_ = false;
        if (!changed && submitted_) return;
        std::memcpy(dib_->bits, surface_.pixels.data(), surface_.pixels.size());
        RECT rect{}; winCheck(GetWindowRect(window_, &rect) != 0, "submit rect");
        POINT position{rect.left, rect.top}, source{0, 0}; SIZE size{LONG(size_), LONG(size_)};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        winCheck(UpdateLayeredWindow(window_, nullptr, &position, &size, dib_->dc, &source, 0, &blend, ULW_ALPHA) != 0, "UpdateLayeredWindow");
        ++submissions_; submitted_ = true;
        if (blocked()) ++hiddenSubmissions_;
    }
public:
    App(HINSTANCE instance, Options o) : instance_(instance), options_(std::move(o)), directory_(appDir()),
        assets_(Assets::load(options_.package)), surface_(PhysicalSize, PhysicalSize),
        clip_(options_.clip), direction_(options_.direction), scale_(options_.scale), loop_(options_.loop) {
        assets_.package.frames(clip_, direction_);
        if (options_.scenario == L"idle") clip_ = 7;
        if (options_.scenario == L"walk") clip_ = 0;
        assets_.package.frames(clip_, direction_);
        if (options_.metrics.empty()) options_.metrics = directory_ / L"metrics.csv";
        metrics_.open(options_.metrics, std::ios::trunc);
        if (!metrics_) throw std::runtime_error("cannot open metrics file");
        events_.open(directory_ / L"events.log", std::ios::app);
        if (!events_) throw std::runtime_error("cannot open event log");
        metrics_ << "elapsed_seconds,cpu_one_core_percent,cpu_all_cores_percent,logical_cores,working_set_bytes,private_bytes,peak_working_set_bytes,render_submissions,decode_count,marker_count,finite_finish_count,skipped_cycles,process_handles,gdi_objects,render_paused,submissions_while_paused,dpi,physical_size,integer_scale,window_x,window_y,switch_count,final_sample," << PlaybackSample::columns << '\n';
        decodes_ = assets_.decoded ? 1 : 0;
        if (!options_.exploration.empty()) {
            explorationPackage_ = loadExplorationPackage(options_.exploration);
            for (const auto& tileset : explorationPackage_->tilesets) {
                auto tiles = Assets::load(options_.exploration.parent_path() / tileset.packageFile);
                if (tiles.package.frames(0, 0).size() != tileset.chunkCount)
                    throw std::runtime_error("tileset atlas count mismatch");
                decodes_ += tiles.decoded ? 1 : 0; tiles_.emplace(tileset.id, std::move(tiles));
            }
            if (options_.snapshot.empty()) options_.snapshot = directory_ / L"exploration.mdes";
            if (!options_.newRun && std::filesystem::exists(options_.snapshot))
                explorer_ = std::make_unique<Explorer>(loadSnapshot(options_.snapshot, explorationPackage_->assetVersion));
            else {
                explorer_ = std::make_unique<Explorer>(static_cast<uint64_t>(nowUs()), options_.seed,
                            explorationPackage_->assetVersion, explorationPackage_->floors);
                saveSnapshot(options_.snapshot, explorer_->state());
            }
            assets_.package.frames(0, 0); assets_.package.frames(7, 0); assets_.package.frames(17, 0);
            if (!options_.scaleExplicit) scale_ = 1;
            clip_ = explorer_->state().phase == Phase::Pose ? 17 : 7;
            loop_ = explorer_->state().phase != Phase::Pose;
            auto marker = [](Surface& s, uint8_t blue, uint8_t green, uint8_t red) {
                for (size_t offset = 0; offset < s.pixels.size(); offset += 4) {
                    s.pixels[offset] = blue; s.pixels[offset+1] = green; s.pixels[offset+2] = red; s.pixels[offset+3] = 255;
                }
            };
            marker(itemMarker_, 30, 210, 240); marker(friendMarker_, 230, 150, 70); marker(stairsMarker_, 220, 220, 220);
        }
        for (unsigned y = 0; y < background_.height; ++y) for (unsigned x = 0; x < background_.width; ++x) {
            auto offset = (y * background_.width + x) * 4;
            uint8_t shade = ((x / 4 + y / 4) % 2) ? 45 : 65;
            background_.pixels[offset] = shade; background_.pixels[offset + 1] = shade; background_.pixels[offset + 2] = shade; background_.pixels[offset + 3] = 100;
        }
    }
    ~App() {
        if (session_ && window_) WTSUnRegisterSessionNotification(window_);
        if (power_) UnregisterPowerSettingNotification(power_);
        if (tray_) { NOTIFYICONDATAW n{}; n.cbSize = sizeof(n); n.hWnd = window_; n.uID = 1; Shell_NotifyIconW(NIM_DELETE, &n); }
        if (window_ && IsWindow(window_)) DestroyWindow(window_);
        UnregisterClassW(L"MysteryDungeonPet.A2", instance_);
    }
    int run() {
        WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.hInstance = instance_; wc.lpfnWndProc = procedure;
        wc.lpszClassName = L"MysteryDungeonPet.A2"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        winCheck(RegisterClassExW(&wc) != 0, "RegisterClass");
        POINT position = restoredPosition();
        window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"MysteryDungeonPet A-3", WS_POPUP,
            position.x, position.y, int(size_), int(size_), nullptr, nullptr, instance_, this);
        winCheck(window_ != nullptr, "CreateWindow");
        clamp(position);
        winCheck(SetWindowPos(window_, topmost_ ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != 0, "initial topmost");
        dib_ = std::make_unique<Dib>(size_);
        timer_.value = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        winCheck(timer_.value != nullptr, "CreateWaitableTimer");
        start_ = previousMetric_ = nowUs(); previousCpu_ = cpuTime(); metricDeadline_ = start_ + 1000000;
        if (options_.duration) stopDeadline_ = start_ + int64_t(options_.duration) * 1000000;
        if (options_.scenario == L"switch") scenarioDeadline_ = start_ + 1000000;
        initialSessionState();
        registerSession(start_);
        power_ = RegisterPowerSettingNotification(window_, &SessionDisplayGuid, DEVICE_NOTIFY_WINDOW_HANDLE);
        winCheck(power_ != nullptr, "RegisterPowerSettingNotification");
        NOTIFYICONDATAW icon{}; icon.cbSize = sizeof(icon); icon.hWnd = window_; icon.uID = 1;
        icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; icon.uCallbackMessage = TrayMessage;
        icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wcscpy_s(icon.szTip, L"MysteryDungeonPet A-3: right click for controls");
        tray_ = Shell_NotifyIconW(NIM_ADD, &icon) != 0;
        winCheck(tray_, "tray icon (required to restore hidden window)");
        log("start assetKey=" + assets_.package.assetKey + " atlasSha256=" + assets_.package.pngSha256 + " testTimingHz=" + std::to_string(assets_.package.tickHz));
        startClip(nowUs());
        if (explorer_) exploreDeadline_ = nowUs() + 350000;
        synchronizePause(nowUs());
        MSG initial{};
        while (PeekMessageW(&initial, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&initial); DispatchMessageW(&initial); }
        render(nowUs());
        if (options_.scenario == L"hide") show(true); else show(false);
        while (running_) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { running_ = false; break; }
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (!running_) break;
            const int64_t now = nowUs();
            if (stopDeadline_ && now >= stopDeadline_) { PostMessageW(window_, WM_CLOSE, 0, 0); continue; }
            if (sessionRetry_ && now >= sessionRetry_) registerSession(now);
            if (scenarioDeadline_ && now >= scenarioDeadline_) {
                if (!blocked()) {
                    ++switches_;
                    const auto index = size_t(switches_ % assets_.package.clips.size());
                    clip_ = assets_.package.clips[index].id; direction_ = unsigned(switches_ % 8); startClip(now);
                    if (switches_ == 100) scenarioDeadline_ = 0;
                    else scenarioDeadline_ = now + 1000000;
                } else scenarioDeadline_ = now + 1000000;
            }
            render(now);
            if (now >= metricDeadline_) { sample(now); metricDeadline_ = now + 1000000; }
            int64_t deadline = metricDeadline_;
            if (stopDeadline_) deadline = std::min(deadline, stopDeadline_);
            if (sessionRetry_) deadline = std::min(deadline, sessionRetry_);
            if (scenarioDeadline_) deadline = std::min(deadline, scenarioDeadline_);
            if (explorer_ && !blocked() && explorer_->state().phase != Phase::Home) deadline = std::min(deadline, exploreDeadline_);
            if (!blocked() && animator_.active() && nextFrameDeadline_) deadline = std::min(deadline, *nextFrameDeadline_);
            // Relative timer, one shot; no high-resolution request or 1ms polling.
            LARGE_INTEGER due{}; due.QuadPart = -std::max<int64_t>(1, deadline - nowUs()) * 10;
            winCheck(SetWaitableTimerEx(timer_.value, &due, 0, nullptr, nullptr, nullptr, 10) != 0, "SetWaitableTimer");
            const HANDLE handle = timer_.value;
            DWORD result = MsgWaitForMultipleObjectsEx(1, &handle, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            winCheck(result != WAIT_FAILED, "MsgWaitForMultipleObjectsEx");
        }
        if (!finalSampled_ && IsWindow(window_)) finalSample();
        log("exit submissions=" + std::to_string(submissions_) + " decodes=" + std::to_string(decodes_) + " switches=" + std::to_string(switches_));
        return 0;
    }
};
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    std::filesystem::path report;
    bool validation = false;
    try {
        ComScope com;
        auto o = options(); report = o.report; validation = o.validate;
        if (o.validate) {
            auto a = pet::Assets::load(o.package);
            auto cached = pet::Assets::load(o.package, &a);
            if (cached.decoded || cached.atlas.get() != a.atlas.get()) throw std::runtime_error("cache reuse failed");
            if (a.package.assetKey == "test:synthetic:a2-v1") {
                auto pixel = [&](size_t x, size_t y, std::initializer_list<uint8_t> expected) {
                    const auto at = (y * a.atlas->width + x) * 4;
                    if (!std::equal(expected.begin(), expected.end(), a.atlas->pixels.begin() + at))
                        throw std::runtime_error("synthetic WIC alpha/black/color check failed");
                };
                pixel(0, 0, {0, 0, 0, 0}); pixel(6, 7, {0, 0, 0, 255});
                pixel(8, 9, {230, 130, 40, 255}); pixel(8, 3, {115, 65, 20, 128});
            }
            // Validate all selectors/directions, including aliases, before any window exists.
            for (const auto& c : a.package.clips) for (unsigned d = 0; d < 8; ++d) a.package.frames(c.id, d);
            if (!report.empty()) {
                std::ofstream output(report);
                if (!output) throw std::runtime_error("cannot open validation report");
                output << "status=pass\nsourceKind=" << a.package.sourceKind << "\nassetKey=" << a.package.assetKey
                       << "\natlasSha256=" << a.package.pngSha256 << "\nwidth=" << a.atlas->width << "\nheight=" << a.atlas->height
                       << "\ndecodeCount=1\ncacheReuse=pass\nPBGRA=pass\nWindows_UI=not_exercised\n";
            }
            return 0;
        }
        App app(instance, std::move(o));
        return app.run();
    } catch (const std::exception& e) {
        if (!report.empty()) { std::ofstream output(report); output << "status=fail\nerror=" << e.what() << '\n'; }
        OutputDebugStringW((wide(e.what()) + L"\n").c_str());
        if (!validation) MessageBoxW(nullptr, wide(e.what()).c_str(), L"MysteryDungeonPet A-3", MB_OK | MB_ICONERROR);
        return 1;
    }
}
