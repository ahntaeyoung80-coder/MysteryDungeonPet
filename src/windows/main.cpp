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
#include "core/gameplay_scene.hpp"
#include "core/guild_scene.hpp"
#include "core/motion.hpp"
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
uint64_t utcMs() { return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); }
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
std::filesystem::path runtimePath(const wchar_t* installed,const wchar_t* repository) {
    const auto root=executableDir();
    const auto path=root/installed;
    return std::filesystem::exists(path)?path:root/repository;
}
struct Options {
    std::filesystem::path package = runtimePath(L"assets/original/asset-0/asset.mdpa",L"assets/original/runtime/asset-0/asset.mdpa");
    std::filesystem::path metrics, report;
    std::filesystem::path exploration, snapshot;
    std::filesystem::path gameplayAssets = runtimePath(L"assets/gameplay",L"assets/original/gameplay");
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
    bool animationTest = false;
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    winCheck(args != nullptr, "CommandLineToArgv");
    std::unique_ptr<wchar_t*, decltype(&LocalFree)> owner(args, LocalFree);
    for (int i = 1; i < count; ++i) {
        std::wstring arg = args[i];
        auto value = [&]() -> std::wstring { if (++i == count) throw std::runtime_error("missing option value"); return args[i]; };
        if (arg == L"--package") { o.package = value(); animationTest = true; }
        else if (arg == L"--diagnostics") animationTest = true;
        else if (arg == L"--explore") o.exploration = value();
        else if (arg == L"--gameplay-assets") o.gameplayAssets = value();
        else if (arg == L"--snapshot") o.snapshot = value();
        else if (arg == L"--seed") o.seed = number(value(), UINT32_MAX);
        else if (arg == L"--new-run") o.newRun = true;
        else if (arg == L"--metrics") o.metrics = value();
        else if (arg == L"--report") o.report = value();
        else if (arg == L"--clip") { o.clip = number(value(), INT32_MAX); animationTest = true; }
        else if (arg == L"--direction") { o.direction = number(value(), 7); animationTest = true; }
        else if (arg == L"--size") { if (number(value(), PhysicalSize) != PhysicalSize) throw std::runtime_error("physical window size is fixed at 200"); }
        else if (arg == L"--scale") { o.scale = number(value(), 8); o.scaleExplicit = true; }
        else if (arg == L"--duration") o.duration = number(value(), 86400);
        else if (arg == L"--scenario") { o.scenario = value(); animationTest = true; }
        else if (arg == L"--finite") { o.loop = false; animationTest = true; }
        else if (arg == L"--validate-package") { o.validate = true; animationTest = true; }
        else throw std::runtime_error("unknown command-line option");
    }
    // A normal desktop launch owns exploration. Keep explicit A diagnostics compatible;
    // an explicit --explore still wins over their legacy --package selector.
    if (o.exploration.empty() && !animationTest)
        o.exploration = runtimePath(L"assets/exploration/dungeon-0/exploration.mdpe",L"assets/original/exploration/dungeon-0/exploration.mdpe");
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
    std::optional<Assets> enemyAssets_, homeAssets_, propsAssets_;
    std::vector<GuildRoom> guildRooms_;
    std::map<std::string, Assets> guildAssets_;
    struct GuildLiveActor {
        const GuildActor* cue = nullptr;
        const Assets* assets = nullptr;
        Animator animator;
        std::uint32_t clip = 7;
        unsigned direction = 0;
        size_t frame = 0;
        bool finished = false;
    };
    std::vector<GuildLiveActor> guildLive_;
    int guildRoom_ = -1;
    LifePhase guildLife_ = LifePhase::Explore;
    SceneBeat guildBeat_=SceneBeat::None;
    Animator enemyAnimator_;
    uint32_t enemyClip_ = 7;
    size_t enemyFrame_ = 0;
    unsigned enemyDirection_ = 0;
    std::optional<ExplorationPackage> explorationPackage_;
    std::unique_ptr<Explorer> explorer_;
    std::map<uint32_t, Assets> tiles_;
    std::array<uint32_t, kTiles> chunks_{};
    uint64_t chunkVersion_ = 0, chunkSeed_ = 0;
    int64_t exploreDeadline_ = 0;
    bool poseFinished_ = false;
    ActorCompletion completion_;
    Motion playerMotion_, guildCamera_;
    std::map<uint32_t,Motion> actorMotions_;
    int64_t presentationNow_ = 0;
    uint64_t pauseUtc_=0;
    Point lastPlayerDraw_{-1,-1};
    std::map<uint32_t,Animator> actorAnimators_;
    std::map<uint32_t,size_t> actorFrames_;
    Surface itemMarker_{5, 5}, friendMarker_{5, 5}, stairsMarker_{7, 7};
    Animator animator_;
    Surface surface_, background_{32, 24};
    Surface dungeonBackdrop_{200, 200};
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
    std::wstring submittedText_;
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
        const bool p=blocked();if(p==paused_) return;
        if(p) {
            pauseUtc_=utcMs();playerMotion_.pause(now);guildCamera_.pause(now);animator_.pause(now);enemyAnimator_.pause(now);
            for(auto& entry:actorMotions_) entry.second.pause(now);
            for(auto& entry:actorAnimators_) entry.second.pause(now);
            for(auto& actor:guildLive_) actor.animator.pause(now);
            log("render_pause");
        } else {
            if(explorer_) {
                auto candidate=*explorer_;const auto utc=utcMs();candidate.shiftPausedClocks(utc>pauseUtc_?utc-pauseUtc_:0,pauseUtc_);
                saveSnapshot(options_.snapshot,candidate.state());*explorer_=std::move(candidate);
                const auto deadline=explorer_->state().gameplay.deadlineMs;
                exploreDeadline_=deadlineFromUtc(now,utcMs(),deadline);
            }
            playerMotion_.resume(now);guildCamera_.resume(now);animator_.resume(now);enemyAnimator_.resume(now);
            for(auto& entry:actorMotions_) entry.second.resume(now);
            for(auto& entry:actorAnimators_) entry.second.resume(now);
            for(auto& actor:guildLive_) actor.animator.resume(now);
            log("render_resume (not user activity)");
        }
        paused_=p;
    }
    void restoreMovement(int64_t now) {
        const auto& state=explorer_->state();const auto utc=utcMs();
        auto restore=[&](Motion& motion,uint32_t actor,const MoveRecord& m) {
            if(!m.actionId) return;
            const auto elapsed=std::min<uint64_t>(m.durationMs,utc>m.startedMs?utc-m.startedMs:0);
            motion.begin(actor,m.actionId,tilePoint(m.from),tilePoint(m.to),now-int64_t(elapsed*1000),m.direction,int64_t(m.durationMs)*1000);
        };
        restore(playerMotion_,0,state.gameplay.movement);
        for(const auto& e:state.population.entities) if(e.movement.actionId) {
            restore(actorMotions_[e.instanceId],e.instanceId,e.movement);
            if(actorMotions_[e.instanceId].active(now)) actorAnimators_[e.instanceId].start(enemyAssets_->package,0,e.movement.direction, true,now-int64_t(std::min<uint64_t>(400,utc>e.movement.startedMs?utc-e.movement.startedMs:0))*1000);
        }
    }
    int64_t restoredClipStart(int64_t now) const {
        if (!explorer_) return now;
        const auto started = explorer_->state().gameplay.phaseStartedMs, utc = utcMs();
        return std::max<int64_t>(0,now-static_cast<int64_t>(std::min<uint64_t>(utc>started?utc-started:0,60000))*1000);
    }
    bool holdingScenePose() const {
        if(!explorer_) return false;
        const auto beat=explorer_->state().gameplay.sceneBeat;
        return beat==SceneBeat::MorningFadeOut || beat==SceneBeat::BedFadeOut;
    }
    int64_t sceneClipStart(int64_t now) const {
        // These stages already completed their finite pose before entering the fade.
        return holdingScenePose()?std::max<int64_t>(0,now-60000000):restoredClipStart(now);
    }
    void startClip(int64_t now, bool restoring = false) {
        frame_ = 0; nextFrameDeadline_.reset();
        if (options_.scenario == L"empty") animator_.stop();
        else {
            int64_t start=restoring && explorer_ && explorer_->state().gameplay.life!=LifePhase::Explore?sceneClipStart(now):now;
            if(restoring && explorer_ && clip_==0 && explorer_->state().gameplay.walkStartedMs) {
                const auto since=explorer_->state().gameplay.walkStartedMs,utc=utcMs();
                start=std::max<int64_t>(0,now-int64_t(std::min<uint64_t>(utc>since?utc-since:0,60000))*1000);
            }
            animator_.start(assets_.package,clip_,direction_,loop_,start);
            if (paused_) animator_.pause(now);
        }
        dirty_ = true;
        log("select clip=" + std::to_string(clip_) + " direction=" + std::to_string(direction_) + " loop=" + std::to_string(loop_));
    }
    void addProp(std::vector<Layer>& layers,unsigned id,Point position) const {
        const auto& f=propsAssets_->package.frames(id,0).front();
        layers.push_back({propsAssets_->atlas.get(),f.rect,f.anchor,position});
    }
    void addDisc(std::vector<Layer>& layers,const Frame& frame,Point actor) const {
        addProp(layers,3,{actor.x+frame.shadow.x-frame.anchor.x,actor.y+frame.shadow.y-frame.anchor.y});
    }
    PlaybackSample currentPlayback() const {
        Point scene{int32_t(size_ / (2 * scale_)), int32_t(size_ / (2 * scale_))};
        if (explorer_ && dungeonScene(explorer_->state().gameplay)) {
            const int tile = explorer_->state().position;
            scene = raster(playerMotion_.sample(presentationNow_));
            (void)tile;
        } else if (explorer_ && !guildRooms_.empty()) {
            scene = guildRooms_[bedroomScene(explorer_->state().gameplay) ? 1 : 0].player().position;
            const auto& g=explorer_->state().gameplay;
            if(g.sceneBeat==SceneBeat::RoomExit) {
                const auto utc=utcMs();
                const auto elapsed=utc>g.phaseStartedMs?utc-g.phaseStartedMs:0;
                scene.y+=int32_t((guildRooms_[1].height+64-scene.y)*std::min<uint64_t>(elapsed,kRoomExitMs)/kRoomExitMs);
            }
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
        HMENU menu = CreatePopupMenu();
        HMENU clips = explorer_ ? nullptr : CreatePopupMenu();
        HMENU directions = explorer_ ? nullptr : CreatePopupMenu();
        if (!menu || (!explorer_ && (!clips || !directions))) {
            if (menu) DestroyMenu(menu);
            if (clips) DestroyMenu(clips);
            if (directions) DestroyMenu(directions);
            winCheck(false, "CreatePopupMenu");
        }
        if (!explorer_) {
            for (size_t i = 0; i < assets_.package.clips.size(); ++i) {
                const auto& c = assets_.package.clips[i];
                AppendMenuW(clips, MF_STRING | (c.id == clip_ ? MF_CHECKED : 0), ClipBase + i, wide(c.name).c_str());
            }
            const wchar_t* names[] = {L"Down", L"DownRight", L"Right", L"UpRight", L"Up", L"UpLeft", L"Left", L"DownLeft"};
            for (unsigned i = 0; i < 8; ++i) AppendMenuW(directions, MF_STRING | (i == direction_ ? MF_CHECKED : 0), DirectionBase + i, names[i]);
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(clips), L"Animation");
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(directions), L"Direction");
        }
        auto add = [&](UINT id, const wchar_t* text, bool checked = false) { AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), id, text); };
        if (!explorer_) { add(Loop, L"Loop", loop_); add(Finite, L"Finite", !loop_); add(Stop, L"Stop (explicit Sleep end)"); }
        add(Hide, hidden_ ? L"Show" : L"Hide"); add(Top, L"Always on top", topmost_);
        add(Scale1, L"Scale 1", scale_ == 1); add(Scale2, L"Scale 2", scale_ == 2); add(Scale3, L"Scale 3", scale_ == 3);
        if (!explorer_) { add(Background, L"Test background", backgroundOn_); add(Reload, L"Reload package"); }
        add(Quit, L"Exit");
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
        if (!explorer_ || blocked() || (now < exploreDeadline_ && !poseFinished_)) return;
        const int oldPosition = explorer_->state().position;
        const auto oldEntities=explorer_->state().population.entities;
        const unsigned oldDirection = direction_;
        const auto oldLife = explorer_->state().gameplay.life;
        const auto oldCombat = explorer_->state().gameplay.combat;
        const auto oldBeat=explorer_->state().gameplay.sceneBeat;
        const auto oldDialogue=explorer_->state().gameplay.dialogueStage;
        auto candidate = *explorer_;
        const auto event = candidate.advanceGameplay(utcMs(), completion_);
        poseFinished_ = false; completion_={};
        if (encodeSnapshot(candidate.state()) == encodeSnapshot(explorer_->state())) {
            exploreDeadline_=deadlineFromUtc(now,utcMs(),candidate.state().gameplay.deadlineMs);
            if (candidate.state().gameplay.life == LifePhase::ReturnHome) dirty_ = true;
            return;
        }
        saveSnapshot(options_.snapshot, candidate.state());
        *explorer_ = std::move(candidate); // commit state before exposing inventory/clear event
        const auto& g = explorer_->state().gameplay;
        exploreDeadline_=deadlineFromUtc(now,utcMs(),g.deadlineMs);
        for(const auto& e:explorer_->state().population.entities) {
            const auto old=std::find_if(oldEntities.begin(),oldEntities.end(),[&](const auto& x){return x.instanceId==e.instanceId;});
            if(old!=oldEntities.end() && old->tile!=e.tile && event.event==GameplayEvent::ActorMove) {
                actorMotions_[e.instanceId].begin(e.instanceId,g.actionRound,tilePoint(old->tile),tilePoint(e.tile),now,e.direction);
                auto& animation=actorAnimators_[e.instanceId];
                if(animation.active()) animation.changeDirection(enemyAssets_->package,0,e.direction);
                else animation.start(enemyAssets_->package,0,e.direction,true,now);
            }
        }
        uint32_t nextClip = gameplayClip(g); bool nextLoop = gameplayLoop(g);
        if (event.event == GameplayEvent::Move) nextClip = 0;
        if (event.event == GameplayEvent::Greeting) { nextClip = 22; nextLoop = false; }
        if (event.event == GameplayEvent::Move) {
            const auto pos = explorer_->state().position;
            const int dx = pos % kWidth - oldPosition % kWidth, dy = pos / kWidth - oldPosition / kWidth;
            direction_ = directionBetween(oldPosition,pos);
            playerMotion_.begin(0, g.movement.actionId, tilePoint(oldPosition),tilePoint(pos),now,direction_);
            (void)dx; (void)dy;
        }
        if (g.life == LifePhase::Combat || g.life==LifePhase::Dialogue) {
            const auto entity = std::find_if(explorer_->state().population.entities.begin(), explorer_->state().population.entities.end(), [&](const auto& e) { return e.instanceId == g.enemyInstance; });
            const int dx = entity->tile % kWidth - oldPosition % kWidth, dy = entity->tile / kWidth - oldPosition / kWidth;
            direction_ = directionBetween(explorer_->state().position,entity->tile);
            (void)dx; (void)dy;
            enemyDirection_ = (direction_ + 4) % 8;
        }
        if (g.homeScene) direction_ = g.sceneBeat==SceneBeat::RoomExit?0:guildRooms_[bedroomScene(g)?1:0].player().direction;
        if(g.sceneBeat==SceneBeat::BedFace || g.sceneBeat==SceneBeat::MorningFace || g.sceneBeat==SceneBeat::MorningFadeOut) direction_=6;
        if(g.sceneBeat==SceneBeat::DiningPan && oldBeat!=g.sceneBeat) guildCamera_.begin(UINT32_MAX,g.day,{368,212},{168,212},now,6,4000000);
        if (clip_ == 0 && nextClip == 0 && loop_ && oldLife == g.life && oldCombat == g.combat) {
            if(oldDirection != direction_) animator_.changeDirection(assets_.package,0,direction_);
        } else if (clip_ != nextClip || loop_ != nextLoop || oldDirection != direction_ || oldLife != g.life || oldCombat != g.combat || oldDialogue!=g.dialogueStage || oldBeat!=g.sceneBeat) { clip_ = nextClip; loop_ = nextLoop; startClip(now); }
        if(event.event == GameplayEvent::FloorChanged || (event.event == GameplayEvent::Explore && oldLife==LifePhase::Ready)) {playerMotion_.reset(tilePoint(explorer_->state().position));actorMotions_.clear();actorAnimators_.clear();}
        if ((g.life == LifePhase::Combat || g.life==LifePhase::Dialogue) && (oldLife != g.life || oldCombat != g.combat || oldDialogue!=g.dialogueStage)) {
            enemyClip_ = g.life==LifePhase::Dialogue ? (g.dialogueStage==DialogueStage::Npc?22:7) : g.combat == CombatPhase::EnemyAttack ? 1 : g.combat == CombatPhase::EnemyHurt ? 6 : g.combat == CombatPhase::Faint ? 33 : 7;
            enemyFrame_ = 0;
            enemyAnimator_.start(enemyAssets_->package, enemyClip_, enemyDirection_, enemyClip_ == 7, now);
        }
        if (!event.onceKey.empty()) log("gameplay onceKey=" + event.onceKey);
        if (oldLife != g.life || oldCombat != g.combat || oldBeat!=g.sceneBeat || oldDialogue!=g.dialogueStage) log(std::string("life=") + lifeName(g.life) + " combat=" + combatName(g.combat) + " day=" + std::to_string(g.day) + " hearts=" + std::to_string(g.hearts) + " sceneBeat=" + std::to_string(static_cast<unsigned>(g.sceneBeat)) + " dialogueStage=" + std::to_string(static_cast<unsigned>(g.dialogueStage)));
        if (event.event == GameplayEvent::UseOranBerry) log("OranBerry70 text: healed 2 hearts (capped at 5); enemy turn follows");
        if (event.event == GameplayEvent::PlayerAttack || event.event == GameplayEvent::EnemyAttack)
            log("move355 damage=" + std::to_string(event.damage) + " critical=" + std::to_string(event.critical));
        if (event.event == GameplayEvent::HomeScene) log(std::string("life=HomeScene opaque original ")+(bedroomScene(g)?(nightScene(g)?"night bedroom":"day bedroom"):"dining map"));
        if(g.life==LifePhase::ReturnHome) dirty_=true;
        dirty_ = true;
    }
    void advanceGuildActors(int64_t now) {
        if (!explorer_ || dungeonScene(explorer_->state().gameplay)) { guildLive_.clear(); guildRoom_ = -1; return; }
        const auto& g = explorer_->state().gameplay;
        const int room = bedroomScene(g)?1:0;
        if (guildRoom_ != room || guildLife_ != g.life || guildBeat_!=g.sceneBeat) {
            guildLive_.clear(); guildRoom_ = room; guildLife_ = g.life;guildBeat_=g.sceneBeat;
            for (const auto& cue : guildRooms_[room].actors) {
                if (cue.kind == GuildActorKind::Player || (room==1 && cue.kind==GuildActorKind::Partner)) continue;
                const Assets* source = nullptr;
                if (cue.kind == GuildActorKind::Partner) source = &*enemyAssets_;
                else {
                    auto it = guildAssets_.find(cue.assetFile);
                    if (it == guildAssets_.end()) {
                        auto loaded = Assets::load(options_.gameplayAssets / cue.assetFile);
                        decodes_ += loaded.decoded ? 1 : 0;
                        it = guildAssets_.emplace(cue.assetFile, std::move(loaded)).first;
                    }
                    source = &it->second;
                }
                GuildLiveActor actor; actor.cue = &cue; actor.assets = source; actor.direction = cue.direction;
                if(cue.kind==GuildActorKind::Partner && (g.sceneBeat==SceneBeat::BedFace || g.sceneBeat==SceneBeat::MorningFace || g.sceneBeat==SceneBeat::MorningFadeOut)) actor.direction=2;
                actor.clip = cue.kind == GuildActorKind::Object ? cue.actionClip :
                    cue.kind == GuildActorKind::Partner ? (room == 1 ? gameplayClip(g) : g.life == LifePhase::Eat ? 15U : 7U) :
                    g.life == LifePhase::Eat ? cue.actionClip : 7U;
                actor.animator.start(source->package, actor.clip, actor.direction, room==1?gameplayLoop(g):true, sceneClipStart(now));
                guildLive_.push_back(std::move(actor));
            }
            log("guild room=" + guildRooms_[room].name + " characters=" + std::to_string(1+std::count_if(guildLive_.begin(),guildLive_.end(),[](const auto& actor){return actor.cue->kind!=GuildActorKind::Object;})) + " objectCount=" + std::to_string(room == 0 ? 1 : 0));
            dirty_ = true;
        }
        for (auto& actor : guildLive_) {
            if (g.life == LifePhase::Eat && actor.cue->id == 79) {
                const auto elapsed = utcMs() > g.phaseStartedMs ? utcMs()-g.phaseStartedMs : 0;
                const unsigned direction = (elapsed*60/1000/20)%2 ? 0 : 4;
                if (direction != actor.direction) {
                    actor.direction=direction;
                    if(actor.clip==0) actor.animator.changeDirection(actor.assets->package,actor.clip,direction);
                    else actor.animator.start(actor.assets->package,actor.clip,direction,true,now);
                    dirty_=true;
                }
            }
            const auto result = actor.animator.advance(now);
            actor.finished = result.finished;
            if (result.finishedEvent && actor.cue->kind == GuildActorKind::Partner && g.life == LifePhase::Wake) log("guild partner wake finished");
            dirty_ = dirty_ || actor.frame != result.frame; actor.frame = result.frame;
            if (result.nextDeadlineUs && (!nextFrameDeadline_ || *result.nextDeadlineUs < *nextFrameDeadline_)) nextFrameDeadline_ = result.nextDeadlineUs;
        }
    }
    void render(int64_t now) {
        presentationNow_=now;
        synchronizePause(now);
        if (blocked()) return;
        const auto playerPoint=raster(playerMotion_.sample(now));
        if(playerPoint.x!=lastPlayerDraw_.x||playerPoint.y!=lastPlayerDraw_.y) {dirty_=true;lastPlayerDraw_=playerPoint;}
        for(auto& entry:actorMotions_) {
            if(entry.second.active(now)) {dirty_=true;auto result=actorAnimators_[entry.first].advance(now);actorFrames_[entry.first]=result.frame;}
            else if(actorAnimators_[entry.first].active()) {actorAnimators_[entry.first].stop();dirty_=true;}
        }
        advanceExploration(now);
        bool playerFinished = false;
        if (animator_.active()) {
            auto result = animator_.advance(now);
            dirty_ = dirty_ || frame_ != result.frame; frame_ = result.frame;
            if(!holdingScenePose()) {markers_ += result.markers.size();finishes_ += result.finishedEvent?1:0;skippedCycles_ += result.skippedCycles;}
            nextFrameDeadline_ = result.nextDeadlineUs;
            if (result.finishedEvent && !holdingScenePose()) log("finite finished assetKey=" + assets_.package.assetKey);
            if(explorer_) completion_.player=result.finished;
            playerFinished = result.finished;
        }
        if (explorer_ && (explorer_->state().gameplay.life == LifePhase::Combat || explorer_->state().gameplay.life==LifePhase::Dialogue) && enemyAnimator_.active()) {
            const auto result = enemyAnimator_.advance(now);
            dirty_ = dirty_ || enemyFrame_ != result.frame; enemyFrame_ = result.frame;
            if (result.nextDeadlineUs && (!nextFrameDeadline_ || *result.nextDeadlineUs < *nextFrameDeadline_)) nextFrameDeadline_ = result.nextDeadlineUs;
            completion_.enemy=result.finished;
        }
        advanceGuildActors(now);
        if (explorer_ && bedroomScene(explorer_->state().gameplay))
            completion_.partner = std::all_of(guildLive_.begin(), guildLive_.end(), [](const auto& actor) { return actor.cue->kind != GuildActorKind::Partner || actor.finished; });
        if(explorer_) {
            const auto& g=explorer_->state().gameplay;
            const bool enemy=g.life==LifePhase::Combat && (g.combat==CombatPhase::EnemyAttack || g.combat==CombatPhase::EnemyHurt || g.combat==CombatPhase::Faint);
            const bool joint=g.sceneBeat==SceneBeat::BedFace || g.sceneBeat==SceneBeat::MorningFace;
            const bool body=(g.life==LifePhase::Wake && g.sceneBeat!=SceneBeat::WakeDelay) || g.life==LifePhase::Pose || g.life==LifePhase::Emotion || (g.life==LifePhase::Combat &&
                (g.combat==CombatPhase::PlayerAttack || g.combat==CombatPhase::PlayerHurt || g.combat==CombatPhase::PlayerFaint || enemy));
            poseFinished_=joint?(completion_.player&&completion_.partner):body?(enemy?completion_.enemy:completion_.player):false;
            if(g.life==LifePhase::Death || g.life==LifePhase::ReturnHome || g.sceneBeat!=SceneBeat::None) dirty_=true;
        }
        if(explorer_ && explorer_->state().gameplay.sceneBeat==SceneBeat::RoomExit) {
            completion_.player=utcMs()>=explorer_->state().gameplay.deadlineMs;
            poseFinished_=completion_.player;
        }
        (void)playerFinished;
        if (!dirty_) return;
        std::vector<Layer> layers;
        Point camera{0, 0};
        if (explorer_ && dungeonScene(explorer_->state().gameplay)) {
            const auto& state = explorer_->state();
            const auto tileId = state.floors[state.floorIndex].tilesetId;
            const auto& tileAssets = tiles_.at(tileId);
            if (chunkVersion_ != state.world.version || chunkSeed_ != state.mapSeed) {
                chunks_ = chunkGrid(state.world, explorationPackage_->tileset(tileId));
                chunkVersion_ = state.world.version;
                chunkSeed_ = state.mapSeed;
            }
            camera = cameraFor(playerMotion_.sample(now),kWidth*24,kHeight*24,int((size_+scale_-1)/scale_));
            layers.push_back({&dungeonBackdrop_, {0, 0, 200, 200}, {0, 0}, camera});
            auto visible = [&](Point p, int size) {
                return p.x + size >= camera.x && p.y + size >= camera.y &&
                       p.x < camera.x + int(size_ / scale_) + 1 && p.y < camera.y + int(size_ / scale_) + 1;
            };
            const auto& frames = tileAssets.package.frames(0, 0);
            for (int tile = 0; tile < kTiles; ++tile) {
                const Point p{tile % kWidth * 24, tile / kWidth * 24};
                if (visible(p, 24)) layers.push_back({tileAssets.atlas.get(), frames[chunks_[tile]].rect, {0, 0}, p});
            }
            const Point stairPos=raster(tilePoint(state.world.stairs));
            if(visible({stairPos.x-12,stairPos.y-12},24)) addProp(layers,1,stairPos);
            for (const auto& e : state.population.entities) if (e.visible && (!e.done || (e.kind==EntityKind::Friend && std::find(state.gameplay.hostile.begin(),state.gameplay.hostile.end(),e.instanceId)==state.gameplay.hostile.end()))) {
                if (e.kind == EntityKind::Friend) {
                    const auto& g = state.gameplay;
                    const bool active = (g.life == LifePhase::Combat || g.life==LifePhase::Dialogue) && e.instanceId == g.enemyInstance;
                    const bool moving=actorMotions_.count(e.instanceId)&&actorMotions_.at(e.instanceId).active(now);
                    const auto& f = enemyAssets_->package.frames(active ? enemyClip_ : moving?0:7, active ? enemyDirection_ : e.direction)[active ? enemyFrame_ : moving?actorFrames_[e.instanceId]:0];
                    const auto world=actorMotions_.count(e.instanceId)?raster(actorMotions_.at(e.instanceId).sample(now)):raster(tilePoint(e.tile));
                    if(showFriendlyDisc(g,std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)==g.hostile.end(),false)) addDisc(layers,f,world);
                    layers.push_back({enemyAssets_->atlas.get(), f.rect, f.anchor, world});
                    continue;
                }
                const Point p=raster(tilePoint(e.tile));
                if(visible({p.x-8,p.y-8},16)) {
                    if(e.originalId==ITEM_ORAN_BERRY) addProp(layers,0,p);
                    else layers.push_back({&itemMarker_,{0,0,5,5},{2,2},p}); // unsupported item graphics remain explicit
                }
            }
        } else if (explorer_) {
            const auto& g = explorer_->state().gameplay;
            const bool bedroom=bedroomScene(g);
            const auto& f = homeAssets_->package.frames(bedroom ? (nightScene(g)?2:1) : 0, 0).front();
            const auto& room=guildRooms_[bedroom?1:0];
            WorldPoint center=bedroom?WorldPoint{180,184}:g.sceneBeat==SceneBeat::DiningPan?guildCamera_.sample(now):g.sceneBeat==SceneBeat::DiningExit?WorldPoint{168,212}:WorldPoint{368,212};
            camera=cameraFor(center,room.width,room.height,int((size_+scale_-1)/scale_));
            layers.push_back({homeAssets_->atlas.get(), f.rect, f.anchor, {0, 0}});
            for (const auto& cue : guildRooms_[bedroom ? 1 : 0].actors) {
                if (cue.kind == GuildActorKind::Player) {
                    const auto sample = currentPlayback();
                    if (sample.frame) layers.push_back({assets_.atlas.get(), sample.frame->rect, sample.frame->anchor, sample.scenePosition});
                } else {
                    const auto it = std::find_if(guildLive_.begin(), guildLive_.end(), [&](const auto& a) { return a.cue->id == cue.id; });
                    if(it==guildLive_.end()) continue;
                    const auto& frame = it->assets->package.frames(it->clip, it->direction)[it->frame];
                    layers.push_back({it->assets->atlas.get(), frame.rect, frame.anchor, cue.position});
                }
            }
        }
        if (backgroundOn_ && options_.scenario != L"empty") layers.push_back({&background_, {0, 0, 32, 24}, {0, 0}, {0, 0}});
        const auto playback = currentPlayback();
        if (playback.frame && (!explorer_ || (!explorer_->state().gameplay.playerHidden && dungeonScene(explorer_->state().gameplay)))) {
            const auto& f = *playback.frame;
            layers.push_back({assets_.atlas.get(), f.rect, f.anchor, playback.scenePosition});
        }
        const auto previous = surface_.pixels;
        (void)compose(surface_, layers, camera, scale_);
        if (explorer_) gameplayOverlay(surface_, explorer_->state().gameplay, utcMs());
        std::wstring overlayText;
        if(explorer_) {
            const auto& g=explorer_->state().gameplay;
            if(g.life==LifePhase::Combat && g.combat==CombatPhase::BerryText) overlayText=L"오랭열매! 2하트 회복";
            else if(g.life==LifePhase::Dialogue) overlayText=g.dialogueStage==DialogueStage::Player?L"안녕! 같이 탐험하자.":L"조심해서 다녀와!";
            else if(g.life==LifePhase::Death && g.deathStage==DeathStage::Blackout) overlayText=L"눈앞이 깜깜해졌다";
        }
        const bool textChanged=overlayText!=submittedText_;
        bool changed = previous != surface_.pixels;
        dirty_ = false;
        if (!changed && !textChanged && submitted_) return;
        std::memcpy(dib_->bits, surface_.pixels.data(), surface_.pixels.size());
        if (explorer_ && explorer_->state().gameplay.life == LifePhase::Combat && explorer_->state().gameplay.combat == CombatPhase::BerryText) {
            SetBkMode(dib_->dc, TRANSPARENT); SetTextColor(dib_->dc, RGB(255,255,255));
            RECT text{12, 155, 188, 188};
            DrawTextW(dib_->dc, L"오랭열매! 2하트 회복", -1, &text, DT_CENTER | DT_WORDBREAK);
            auto* pixels = static_cast<uint8_t*>(dib_->bits);
            for (unsigned y = 150; y < 190; ++y) for (unsigned x = 8; x < 192; ++x) pixels[(y*size_+x)*4+3] = 255;
        }
        if(explorer_ && explorer_->state().gameplay.life==LifePhase::Dialogue) {
            const auto& g=explorer_->state().gameplay;
            SetBkMode(dib_->dc,TRANSPARENT);SetTextColor(dib_->dc,RGB(255,255,255));RECT text{12,154,188,188};
            DrawTextW(dib_->dc,g.dialogueStage==DialogueStage::Player?L"안녕! 같이 탐험하자.":L"조심해서 다녀와!",-1,&text,DT_CENTER|DT_WORDBREAK);
            auto* pixels=static_cast<uint8_t*>(dib_->bits);for(unsigned y=150;y<190;++y) for(unsigned x=8;x<192;++x) pixels[(y*size_+x)*4+3]=255;
        }
        if(explorer_ && explorer_->state().gameplay.life==LifePhase::Death && explorer_->state().gameplay.deathStage==DeathStage::Blackout) {
            SetBkMode(dib_->dc,TRANSPARENT);SetTextColor(dib_->dc,RGB(255,255,255));RECT text{8,80,192,130};
            DrawTextW(dib_->dc,L"눈앞이 깜깜해졌다",-1,&text,DT_CENTER|DT_WORDBREAK);
            auto* pixels=static_cast<uint8_t*>(dib_->bits);for(unsigned y=0;y<size_;++y) for(unsigned x=0;x<size_;++x) pixels[(y*size_+x)*4+3]=255;
        }
        RECT rect{}; winCheck(GetWindowRect(window_, &rect) != 0, "submit rect");
        POINT position{rect.left, rect.top}, source{0, 0}; SIZE size{LONG(size_), LONG(size_)};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        winCheck(UpdateLayeredWindow(window_, nullptr, &position, &size, dib_->dc, &source, 0, &blend, ULW_ALPHA) != 0, "UpdateLayeredWindow");
        ++submissions_; submitted_ = true; submittedText_=std::move(overlayText);
        if (blocked()) ++hiddenSubmissions_;
    }
public:
    App(HINSTANCE instance, Options o) : instance_(instance), options_(std::move(o)), directory_(appDir()),
        assets_(Assets::load(options_.exploration.empty() ? options_.package : options_.gameplayAssets / L"actors/asset-0/asset.mdpa")), surface_(PhysicalSize, PhysicalSize),
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
            enemyAssets_ = Assets::load(options_.gameplayAssets / L"actors/asset-1/asset.mdpa");
            homeAssets_ = Assets::load(options_.gameplayAssets / L"home/home.mdpa");
            propsAssets_=Assets::load(options_.gameplayAssets / L"props/props.mdpa");
            decodes_+=propsAssets_->decoded?1:0;
            guildRooms_ = loadGuildScene(options_.gameplayAssets / L"guild/guild.mdgs");
            decodes_ += (enemyAssets_->decoded ? 1 : 0) + (homeAssets_->decoded ? 1 : 0);
            explorationPackage_ = loadExplorationPackage(options_.exploration);
            for (const auto& tileset : explorationPackage_->tilesets) {
                auto tiles = Assets::load(options_.exploration.parent_path() / tileset.packageFile);
                if (tiles.package.frames(0, 0).size() != tileset.chunkCount)
                    throw std::runtime_error("tileset atlas count mismatch");
                decodes_ += tiles.decoded ? 1 : 0; tiles_.emplace(tileset.id, std::move(tiles));
            }
            if (options_.snapshot.empty()) options_.snapshot = directory_ / L"exploration.mdes";
            const bool restoring = !options_.newRun && std::filesystem::exists(options_.snapshot);
            if (restoring)
                explorer_ = std::make_unique<Explorer>(loadSnapshot(options_.snapshot, explorationPackage_->assetVersion));
            else {
                explorer_ = std::make_unique<Explorer>(static_cast<uint64_t>(nowUs()), options_.seed,
                            explorationPackage_->assetVersion, explorationPackage_->floors);
                saveSnapshot(options_.snapshot, explorer_->state());
            }
            playerMotion_.reset(tilePoint(explorer_->state().position));
            explorer_->enableGameplay(utcMs());
            restoreMovement(nowUs());
            if(explorer_->state().gameplay.sceneBeat==SceneBeat::DiningPan) guildCamera_.begin(UINT32_MAX,explorer_->state().gameplay.day,{368,212},{168,212},restoredClipStart(nowUs()),6,4000000);
            saveSnapshot(options_.snapshot, explorer_->state());
            log(std::string("exploration snapshot=") + (restoring ? "restored" : "new") +
                " day=" + std::to_string(explorer_->state().gameplay.day) +
                " runId=" + std::to_string(explorer_->state().runId));
            assets_.package.frames(0, 0); assets_.package.frames(7, 0); assets_.package.frames(17, 0);
            if (!options_.scaleExplicit) scale_ = 1;
            clip_ = gameplayClip(explorer_->state().gameplay);
            loop_ = gameplayLoop(explorer_->state().gameplay);
            if(explorer_->state().gameplay.life==LifePhase::Explore && playerMotion_.active(nowUs())) {clip_=0;loop_=true;}
            const auto& g = explorer_->state().gameplay;
            direction_=g.playerDirection;
            if(g.homeScene) direction_=g.sceneBeat==SceneBeat::RoomExit?0:guildRooms_[bedroomScene(g)?1:0].player().direction;
            if(g.sceneBeat==SceneBeat::BedFace || g.sceneBeat==SceneBeat::MorningFace || g.sceneBeat==SceneBeat::MorningFadeOut) direction_=6;
            if(g.life==LifePhase::Combat || g.life==LifePhase::Dialogue) {
                enemyDirection_=(g.playerDirection+4)%8;
                enemyClip_=g.life==LifePhase::Dialogue?(g.dialogueStage==DialogueStage::Npc?22:7):g.combat==CombatPhase::EnemyAttack?1:g.combat==CombatPhase::EnemyHurt?6:g.combat==CombatPhase::Faint?33:7;
                const auto restoredNow=nowUs();
                enemyAnimator_.start(enemyAssets_->package,enemyClip_,enemyDirection_,enemyClip_==7,enemyClip_==7?restoredNow:restoredClipStart(restoredNow));
            }
            auto marker = [](Surface& s, uint8_t blue, uint8_t green, uint8_t red) {
                for (size_t offset = 0; offset < s.pixels.size(); offset += 4) {
                    s.pixels[offset] = blue; s.pixels[offset+1] = green; s.pixels[offset+2] = red; s.pixels[offset+3] = 255;
                }
            };
            marker(itemMarker_, 30, 210, 240); marker(friendMarker_, 230, 150, 70); marker(stairsMarker_, 220, 220, 220);
            marker(dungeonBackdrop_, 35, 30, 25);
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
        window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName,
            explorer_ ? L"MysteryDungeonPet" : L"MysteryDungeonPet A-3", WS_POPUP,
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
        wcscpy_s(icon.szTip, explorer_ ? L"MysteryDungeonPet: automatic exploration" : L"MysteryDungeonPet A-3: right click for controls");
        tray_ = Shell_NotifyIconW(NIM_ADD, &icon) != 0;
        winCheck(tray_, "tray icon (required to restore hidden window)");
        log("start assetKey=" + assets_.package.assetKey + " atlasSha256=" + assets_.package.pngSha256 + " testTimingHz=" + std::to_string(assets_.package.tickHz));
        log(explorer_ ? "mode=automatic-exploration" : "mode=animation-diagnostics");
        startClip(nowUs(), true);
        if(explorer_) {const auto deadline=explorer_->state().gameplay.deadlineMs;exploreDeadline_=deadlineFromUtc(nowUs(),utcMs(),deadline);}
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
            if (explorer_ && !blocked()) {
                deadline = std::min(deadline, poseFinished_ ? now : exploreDeadline_);
                const auto& g=explorer_->state().gameplay;
                if(g.life==LifePhase::ReturnHome || (g.life==LifePhase::Death && g.deathStage==DeathStage::FadeOut) ||
                   g.sceneBeat==SceneBeat::DiningExit || g.sceneBeat==SceneBeat::BedFadeOut || g.sceneBeat==SceneBeat::BedFadeIn ||
                   g.sceneBeat==SceneBeat::NightFadeIn || g.sceneBeat==SceneBeat::MorningFadeOut || g.sceneBeat==SceneBeat::ExploreFadeIn ||
                   g.sceneBeat==SceneBeat::FloorFadeOut || g.sceneBeat==SceneBeat::FloorFadeIn || g.sceneBeat==SceneBeat::RoomExit) deadline=std::min(deadline,now+16667);
                if(auto cameraDeadline=guildCamera_.nextDeadline(now)) deadline=std::min(deadline,*cameraDeadline);
                if(auto motionDeadline=playerMotion_.nextDeadline(now)) deadline=std::min(deadline,*motionDeadline);
                for(const auto& entry:actorMotions_) if(auto d=entry.second.nextDeadline(now)) deadline=std::min(deadline,*d);
            }
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
