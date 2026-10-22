#include <windows.h>
#include <windowsx.h>
#include <wtsapi32.h>
#include <powrprof.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>
#include "windows/assets.hpp"
#include "windows/text_overlay.hpp"
#include "core/animator.hpp"
#include "core/playback_sample.hpp"
#include "core/exploration_package.hpp"
#include "core/gameplay_scene.hpp"
#include "core/guild_scene.hpp"
#include "core/completion_scene.hpp"
#include "core/character_selection.hpp"
#include "core/npc_characters.hpp"
#include "core/dialogue_catalogue.hpp"
#include "core/gameplay_notice.hpp"
#include "core/event_registry.hpp"
#include "core/event_runtime.hpp"
#include "core/holiday_scene.hpp"
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
constexpr UINT ExpeditionRevisit=626, ExpeditionBranchBase=700;
constexpr UINT ResetExploration = 625, PlayerCharacterBase=1000, PartnerCharacterBase=1100;
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
    std::filesystem::path metrics, report, movementTrace;
    std::filesystem::path exploration, snapshot, characterProfile;
    std::filesystem::path gameplayAssets = runtimePath(L"assets/gameplay",L"assets/original/gameplay");
    unsigned seed = 1;
    bool newRun = false, solo = false, defaultExploration = false;
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
        else if(arg==L"--character-profile")o.characterProfile=value();
        else if (arg == L"--seed") o.seed = number(value(), UINT32_MAX);
        else if (arg == L"--solo") o.solo = true;
        else if (arg == L"--new-run") o.newRun = true;
        else if (arg == L"--metrics") o.metrics = value();
        else if (arg == L"--movement-trace") o.movementTrace = value();
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
    if (o.exploration.empty() && !animationTest) {
        o.defaultExploration=true;
        o.exploration = runtimePath(L"assets/exploration/dungeon-14/exploration.mdpe",L"assets/original/exploration/dungeon-14/exploration.mdpe");
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
    CharacterCatalogue characterCatalogue_;
    std::filesystem::path characterProfile_;
    std::optional<CharacterSelection> pendingCharacterSelection_;
    DialogueCast dialogueCast_;
    DialogueCatalogue dialogueCatalogue_;
    EventRegistry eventRegistry_;
    HolidayRegistry holidayRegistry_;
    struct HolidayAssets {std::map<std::string,Assets> grounds;std::map<unsigned,Assets> floats;std::optional<Assets> water;unsigned decodes=0;};
    HolidayAssets holidayAssets_;
    Assets assets_;
    std::optional<Assets> companionAssets_, homeAssets_, propsAssets_, itemAssets_;
    NpcCatalogue npcCatalogue_;
    using NpcAssetCache=std::map<std::string,std::shared_ptr<Assets>>;
    NpcAssetCache npcAssets_;
    struct NpcAssetsChange {NpcAssetCache cache;unsigned decodes=0;};
    std::optional<Assets> appleAssets_,uiAssets_,portraitAssets_;
    Animator appleAnimator_;
    std::size_t appleFrame_=0;
    std::optional<Surface> dialoguePanel_,portraitFrame_;
    CompletionRegistry completionRegistry_;
    std::optional<Assets> expeditionGround_;
    std::string expeditionGroundId_;
    std::map<unsigned,ExplorationPackage> expeditionBranches_;
    std::map<std::uint32_t,Assets> completionAssets_;
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
    Animator companionAnimator_;
    Motion companionMotion_;
    uint32_t companionClip_=7;
    unsigned companionDirection_=0;
    size_t companionFrame_=0;
    uint64_t companionPhaseStarted_=UINT64_MAX;
    Point lastCompanionDraw_{-1,-1};
    Animator enemyAnimator_;
    uint32_t enemyClip_ = 7;
    size_t enemyFrame_ = 0;
    unsigned enemyDirection_ = 0;
    std::optional<ExplorationPackage> explorationPackage_, pendingDefaultPackage_;
    std::map<unsigned,ExplorationPackage> explorationPool_;
    std::filesystem::path explorationRoot_;
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
    uint64_t partyPlayerToken_=0,partyCompanionToken_=0;
    std::map<uint32_t,uint64_t> partyActorTokens_;
    std::map<uint32_t,uint32_t> partyActorClips_,partyActorDirections_;
    Surface friendMarker_{5, 5}, stairsMarker_{7, 7};
    Animator animator_;
    Surface surface_, background_{32, 24};
    Surface dungeonBackdrop_{200, 200};
    std::unique_ptr<Dib> dib_;
    TextOverlay textOverlay_;
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
    bool finalSampled_ = false,traceCompletionImage_=false,traceDeathImage_=false,traceCharacterImagePending_=false;
    std::wstring submittedText_,submittedHud_;
    unsigned submittedFade_=UINT_MAX,submittedTitleOpacity_=UINT_MAX;
    uint64_t submissions_ = 0, decodes_ = 0, markers_ = 0, finishes_ = 0, skippedCycles_ = 0, switches_ = 0;
    uint64_t hiddenSubmissions_ = 0;
    int64_t start_ = 0, metricDeadline_ = 0, stopDeadline_ = 0, scenarioDeadline_ = 0, sessionRetry_ = 0;
    unsigned sessionAttempts_ = 0;
    int64_t previousMetric_ = 0;
    uint64_t previousCpu_ = 0;
    std::ofstream metrics_, events_, movementTrace_;
    unsigned traceImages_ = 0;
    bool traceAlarmImage_=false;
    std::array<bool,6> traceDialogueImages_{};
    std::set<std::string> traceVarietyImages_;
    void traceImage(const char* suffix) {
        if(!movementTrace_ || !dib_ || !submitted_) return;
        auto path=options_.movementTrace;path.replace_extension();path+=suffix;
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        if(!out) throw std::runtime_error("cannot open movement frame image");
        BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
        file.bfSize=file.bfOffBits+size_*size_*4;
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=LONG(size_);info.biHeight=-LONG(size_);
        info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=size_*size_*4;
        out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&info),sizeof(info));
        out.write(static_cast<const char*>(dib_->bits),size_*size_*4);
        if(!out) throw std::runtime_error("cannot write movement frame image");
    }
    Point homeCamera(int64_t now) const {
        const auto& g=explorer_->state().gameplay;const bool bedroom=bedroomScene(g);
        const auto& room=guildRooms_[bedroom?1:0];
        WorldPoint center=bedroom?WorldPoint{180,184}:g.sceneBeat==SceneBeat::DiningPan?guildCamera_.sample(now):g.sceneBeat==SceneBeat::DiningExit?WorldPoint{168,212}:WorldPoint{368,212};
        if(g.sceneBeat==SceneBeat::RoomExit || g.sceneBeat==SceneBeat::MorningFadeOut) {const auto p=bedroomExitCamera(g,utcMs());center={double(p.x),double(p.y)};}
        return cameraFor(center,room.width,room.height,int((size_+scale_-1)/scale_));
    }
    const ExpeditionStop* currentExpeditionStop() const {
        return explorer_ && expeditionScene(explorer_->state())?expeditionStop(explorer_->state()):nullptr;
    }
    Point expeditionPosition(bool partner=false) const {
        const auto* stop=currentExpeditionStop();
        return {int(partner?stop->partnerX:stop->playerX),int(partner?stop->partnerY:stop->playerY)};
    }
    Point expeditionCamera() const {
        const auto* stop=currentExpeditionStop();const auto a=expeditionPosition(),b=expeditionPosition(true);
        return cameraFor({double(a.x+b.x)/2,double(a.y+b.y)/2-24},stop->width,stop->height,int((size_+scale_-1)/scale_));
    }
    void stageExpeditionGround(const ExplorationState& state) {
        const auto* stop=expeditionScene(state)?expeditionStop(state):nullptr;
        if(!stop){expeditionGround_.reset();expeditionGroundId_.clear();return;}
        if(expeditionGroundId_==stop->groundId)return;
        auto asset=Assets::load(options_.gameplayAssets/L"expeditions"/stop->assetFile);
        const auto& frame=asset.package.frames(0,0).front();
        if(frame.rect.x || frame.rect.y || frame.rect.w!=int(stop->width) || frame.rect.h!=int(stop->height))throw std::runtime_error("expedition ground dimensions mismatch");
        decodes_+=asset.decoded?1:0;expeditionGround_=std::move(asset);expeditionGroundId_=stop->groundId;
    }
    const CompletionDestination* completionDestination() const {
        if(!explorer_ || !completionScene(explorer_->state().gameplay))return nullptr;
        const auto id=explorer_->state().floors[explorer_->state().floorIndex].dungeonId;
        if(const auto* direct=completionRegistry_.destination(id))return direct;
        for(const auto& d:completionRegistry_.destinations)if(d.terminalId==id)return &d;
        return nullptr;
    }
    Point endingPosition(bool partner=false) const {
        return completionPosition(*completionDestination(),explorer_->state().gameplay,utcMs(),partner);
    }
    Point endingCamera() const {
        const auto* d=completionDestination();const auto player=endingPosition(),partner=endingPosition(true);
        const WorldPoint center{double(player.x+partner.x)/2,double(player.y+partner.y)/2-24};
        return cameraFor(center,d->width,d->height,int((size_+scale_-1)/scale_));
    }
    std::wstring mapHud() const {
        return {};
    }
    void traceMovement(int64_t now,bool submitted) {
        if(!movementTrace_ || !explorer_) return;
        const auto& state=explorer_->state();const auto& g=state.gameplay;const bool dungeon=dungeonScene(g);
        const auto scene=completionDestination()?endingPosition():dungeon?raster(playerMotion_.sample(now)):currentPlayback().scenePosition;
        const auto world=dungeon && !completionDestination()?playerMotion_.sample(now):WorldPoint{double(scene.x),double(scene.y)};
        const auto camera=completionDestination()?endingCamera():dungeon?cameraFor(world,kWidth*24,kHeight*24,int((size_+scale_-1)/scale_)):homeCamera(now);
        uint64_t hash=14695981039346656037ULL,light=0;unsigned opaque=0;
        const auto* pixels=submitted_?static_cast<const std::uint8_t*>(dib_->bits):surface_.pixels.data();
        for(size_t i=0;i<surface_.pixels.size();++i) {hash^=pixels[i];hash*=1099511628211ULL;if(i%4==3){if(pixels[i]==255) ++opaque;}else light+=pixels[i];}
        movementTrace_ << now-start_ << ',' << g.movement.actionId << ',' << g.movement.startedMs << ',' << g.movement.durationMs << ','
            << g.movement.from << ',' << g.movement.to << ',' << std::setprecision(12) << world.x << ',' << world.y << ','
            << scene.x << ',' << scene.y << ',' << camera.x << ',' << camera.y << ',' << direction_ << ',' << clip_ << ',' << frame_ << ','
            << scale_ << ',' << (g.sceneBeat==SceneBeat::CompletionWalk || playerMotion_.active(now)) << ',' << opaque << ',' << hash << ',' << submitted << ','
            << markers_ << ',' << static_cast<unsigned>(g.life) << ',' << static_cast<unsigned>(g.sceneBeat) << ',' << utcMs() << ',' << g.walkStartedMs << ','
            << state.floorIndex << ',' << state.world.start << ',' << state.world.stairs << ',' << state.position << ','
            << std::count_if(state.population.entities.begin(),state.population.entities.end(),[&](const auto& e){return e.tile==state.world.start;}) << ',' << holdingScenePose();
        const auto partner=std::find_if(guildLive_.begin(),guildLive_.end(),[](const auto& a){return a.cue->kind==GuildActorKind::Partner;});
        movementTrace_ << ',' << (partner==guildLive_.end()?0:partner->clip) << ',' << (partner==guildLive_.end()?0:partner->frame) << ',' << light;
        const auto& p=g.companion;const auto end=completionDestination()?endingPosition(true):Point{};
        const auto point=completionDestination()?WorldPoint{double(end.x),double(end.y)}:companionMotion_.sample(now);
        movementTrace_ << ',' << p.enabled << ',' << p.tile << ',' << point.x << ',' << point.y << ',' << companionClip_ << ',' << companionFrame_ << ',' << companionMotion_.active(now)
            << ',' << g.hearts << ',' << p.hearts << ',' << g.actionRound << ',' << g.playerActionRound << ',' << p.actionRound << ',' << g.enemyTargetsCompanion << ',' << static_cast<unsigned>(g.combat)
            << ',' << g.independentParty << ',' << static_cast<unsigned>(g.partyPhase)
            << ',' << partyPlayerToken_ << ',' << partyCompanionToken_ << ',';
        bool firstActor=true;
        for(const auto& e:state.population.entities) if(e.kind==EntityKind::Friend && e.visible &&
            (!e.done || std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)==g.hostile.end())) {
            if(!firstActor)movementTrace_ << ';';firstActor=false;
            const bool focused=!partyRendering() && (g.life==LifePhase::Combat || g.life==LifePhase::Dialogue) && e.instanceId==g.enemyInstance;
            const bool moving=actorMotions_.count(e.instanceId) && actorMotions_.at(e.instanceId).active(now);
            movementTrace_ << e.instanceId << ':' << e.tile << ':'
                << (partyRendering()?partyActorClips_[e.instanceId]:focused?enemyClip_:moving?0:7) << ':'
                << (partyRendering()?actorFrames_[e.instanceId]:focused?enemyFrame_:moving?actorFrames_[e.instanceId]:0) << ':'
                << partyActorTokens_[e.instanceId] << ':'
                << (focused?enemyDirection_:e.direction);
        }
        movementTrace_ << ',' << p.movement.startedMs << ',' << p.movement.durationMs << ',';
        bool firstMove=true;
        for(const auto& e:state.population.entities) if(e.kind==EntityKind::Friend && e.visible && !e.done) {
            if(!firstMove)movementTrace_ << ';';firstMove=false;
            movementTrace_ << e.instanceId << ':' << e.movement.startedMs << ':' << e.movement.durationMs
                << ':' << e.movement.from << ':' << e.movement.to << ':' << e.movement.actionId;
        }
        const auto alarm=std::find_if(guildLive_.begin(),guildLive_.end(),[](const auto& a){return a.cue->id==109;});
        const auto alarmPoint=alarm==guildLive_.end()?Point{-1,-1}:loudredAlarm(alarm->cue->position,g,utcMs()).position;
        unsigned shown=0,total=0;
        if(dungeon)for(int tile=0;tile<kTiles;++tile)if(state.world.walkable(tile) && state.world.room[tile]==state.world.room[state.position]){++total;shown+=state.screenSeen[tile];}
        movementTrace_ << ',' << alarmPoint.x << ',' << alarmPoint.y << ',' << (alarm==guildLive_.end()?-1:int(alarm->clip))
            << ',' << (alarm==guildLive_.end()?-1:int(alarm->direction)) << ',' << shown << ',' << total;
        const auto partnerPoint=partner==guildLive_.end()?Point{-1,-1}:
            g.sceneBeat==SceneBeat::RoomExit || g.sceneBeat==SceneBeat::MorningFadeOut?
            bedroomExit(partner->cue->position,g,utcMs(),true).position:partner->cue->position;
        movementTrace_ << ',' << partnerPoint.x << ',' << partnerPoint.y << ',' << (partner==guildLive_.end()?-1:int(partner->direction))
            << ',' << guildAppleVisible(g) << ',' << appleFrame_;
        const auto portrait=cataloguePortraits(state,dialogueCatalogue_);
        const auto* selected=selectedDialogue(state,dialogueCatalogue_);const auto line=selected?selected->display:DialogueLine{};
        movementTrace_<<','<<dialogueCast_.actor(PortraitRole::Player).portraitClip<<','<<dialogueCast_.actor(PortraitRole::Partner).portraitClip
            <<','<<unsigned(portrait.speaker)<<','<<unsigned(portrait.emotion)<<','<<line.id<<','<<gameplayFadeOpacity(g,utcMs());
        const auto actual=currentPortrait();const auto* npc=npcSpeaker(actual);
        movementTrace_<<','<<(npc?npc->portraitClip:actual.speaker==PortraitRole::None?0:dialogueCast_.actor(actual.speaker).portraitClip)
            <<','<<unsigned(actual.emotion)<<',';
        bool firstNpc=true;
        for(const auto& e:state.population.entities)if(e.kind==EntityKind::Friend) {
            if(!firstNpc)movementTrace_<<';';
            firstNpc=false;
            const auto& c=npcCatalogue_.select(e.originalId);
            movementTrace_<<e.instanceId<<':'<<c.id<<':'<<c.spriteIndex;
        }
        movementTrace_<<','<<npcAssets_.size()<<','<<state.variety.dialogue.bundle<<','<<state.variety.dialogue.lineId<<','<<state.variety.dialogue.deadlineMs;
        const auto& h=state.variety.holiday;const auto& e=state.variety.event;
        movementTrace_<<','<<unsigned(e.id.type)<<','<<unsigned(e.progress)<<','<<e.variant<<','<<unsigned(h.beat);
        if(h.active) {
            const auto sample=holidaySample(state,holidayRegistry_,utcMs());
            movementTrace_<<','<<sample.scene<<','<<sample.player.x<<','<<sample.player.y<<','<<sample.partner.x<<','<<sample.partner.y
                <<','<<sample.playerDirection<<','<<sample.partnerDirection<<','<<(sample.bathing?20:sample.moving?0:7)<<','<<sample.bathing<<','<<holidayFade(state,utcMs());
        } else movementTrace_<<",,0,0,0,0,0,0,0,0,0";
        const auto* notice=activeGameplayNotice(state);
        movementTrace_<<','<<(notice?unsigned(notice->kind):0)<<','<<(notice?notice->item:0)<<','<<(notice?notice->deadlineMs:0)<<'\n';
        const bool textReady=!catalogueText(state,dialogueCatalogue_).empty() && revealedCatalogueText(state,dialogueCatalogue_,utcMs())==catalogueText(state,dialogueCatalogue_);
        if(submitted && (h.active || (eventActive(state) && textReady))) {
            const auto key=h.active?"holiday-"+std::to_string(unsigned(h.beat))+(textReady?"-"+state.variety.dialogue.lineId:""):
                "event-"+std::to_string(unsigned(e.id.type))+"-"+state.variety.dialogue.lineId;
            if(traceVarietyImages_.insert(key).second)traceImage(("-"+key+".bmp").c_str());
        }
        if(submitted && traceCharacterImagePending_){traceImage("-characters.bmp");traceCharacterImagePending_=false;}
        if(submitted && !traceDeathImage_ && g.life==LifePhase::Death && g.deathStage==DeathStage::PartnerTalk && textReady){traceImage("-defeat-dialogue.bmp");traceDeathImage_=true;}
        if(submitted && !traceCompletionImage_ && g.sceneBeat==SceneBeat::CompletionText && utcMs()>=g.phaseStartedMs+1500){traceImage("-completion.bmp");traceCompletionImage_=true;}
        if(submitted && !traceAlarmImage_ && g.sceneBeat==SceneBeat::WakeDelay && textReady){traceImage("-alarm.bmp");traceAlarmImage_=true;}
        if(submitted && textReady && (gameplayText(g).find(L"오랭열매")==0) && traceVarietyImages_.insert("full-heal").second)traceImage("-full-heal.bmp");
        if(submitted && textReady && activeGameplayNotice(state)) {
            const auto* notice=activeGameplayNotice(state);
            const auto key="notice-"+std::to_string(unsigned(notice->kind))+"-"+std::to_string(notice->item);
            if(traceVarietyImages_.insert(key).second)traceImage(("-"+key+".bmp").c_str());
        }
        if(submitted && textReady) {
            int capture=-1;const char* suffix="";
            if(g.life==LifePhase::Dialogue) {
                capture=(companionDialogue(g)?2:0)+(g.dialogueStage==DialogueStage::Npc?1:0);
                static const char* names[]{"-npc-player.bmp","-npc-reply.bmp","-companion-player.bmp","-companion-reply.bmp"};suffix=names[capture];
            } else if(g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::BedFace){capture=4;suffix="-bed-greeting.bmp";}
            else if(g.life==LifePhase::Ready && g.sceneBeat==SceneBeat::MorningFace){capture=5;suffix="-morning-greeting.bmp";}
            if(capture>=0 && !traceDialogueImages_[capture]){traceImage(suffix);traceDialogueImages_[capture]=true;}
        }
        if(submitted && !traceImages_) {traceImage("-first.bmp");++traceImages_;}
        if(submitted && explorer_ && !explorationTitle(explorer_->state()).empty() && utcMs()>=explorer_->state().gameplay.phaseStartedMs+1000 && explorationTitleOpacity(explorer_->state(),utcMs())==255 && traceVarietyImages_.insert("map-title").second)traceImage("-map-title.bmp");
        if(submitted && traceImages_==1 && now-start_>=1000000) {traceImage("-middle.bmp");++traceImages_;}
    }
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
            pauseUtc_=utcMs();playerMotion_.pause(now);guildCamera_.pause(now);animator_.pause(now);appleAnimator_.pause(now);enemyAnimator_.pause(now);companionAnimator_.pause(now);companionMotion_.pause(now);
            for(auto& entry:actorMotions_) entry.second.pause(now);
            for(auto& entry:actorAnimators_) entry.second.pause(now);
            for(auto& actor:guildLive_) actor.animator.pause(now);
            log("render_pause");
        } else {
            if(explorer_) {
                auto candidate=*explorer_;const auto utc=utcMs();candidate.shiftPausedClocks(utc>pauseUtc_?utc-pauseUtc_:0,pauseUtc_);
                saveRunSnapshot(options_.snapshot,candidate.state());*explorer_=std::move(candidate);
                const auto deadline=nextGameplayDeadline(explorer_->state());
                exploreDeadline_=deadlineFromUtc(now,utcMs(),deadline);
            }
            playerMotion_.resume(now);guildCamera_.resume(now);animator_.resume(now);appleAnimator_.resume(now);enemyAnimator_.resume(now);companionAnimator_.resume(now);companionMotion_.resume(now);
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
        if(state.gameplay.companion.enabled) restore(companionMotion_,kCompanionActorId,state.gameplay.companion.movement);
        for(const auto& e:state.population.entities) if(e.movement.actionId) {
            restore(actorMotions_[e.instanceId],e.instanceId,e.movement);
            if(actorMotions_[e.instanceId].active(now)) actorAnimators_[e.instanceId].start(npcAssets(e.instanceId).package,0,e.movement.direction, true,now-int64_t(std::min<uint64_t>(400,utc>e.movement.startedMs?utc-e.movement.startedMs:0))*1000);
        }
    }
    int64_t restoredClipStart(int64_t now) const {
        if (!explorer_) return now;
        const auto started = explorer_->state().gameplay.phaseStartedMs, utc = utcMs();
        return std::max<int64_t>(0,now-static_cast<int64_t>(std::min<uint64_t>(utc>started?utc-started:0,60000))*1000);
    }
    bool holdingScenePose() const {
        return explorer_ && holdGameplayPose(explorer_->state().gameplay);
    }
    int64_t sceneClipStart(int64_t now) const {
        // These stages already completed their finite pose before entering the fade.
        return holdingScenePose()?std::max<int64_t>(0,now-60000000):restoredClipStart(now);
    }
    void startClip(int64_t now, bool restoring = false) {
        frame_ = 0; nextFrameDeadline_.reset();
        if (options_.scenario == L"empty") animator_.stop();
        else {
            int64_t start=holdingScenePose() || (restoring && explorer_ && explorer_->state().gameplay.life!=LifePhase::Explore)?sceneClipStart(now):now;
            if(restoring && explorer_ && clip_==0 && explorer_->state().gameplay.walkStartedMs) {
                const auto since=explorer_->state().gameplay.walkStartedMs,utc=utcMs();
                start=std::max<int64_t>(0,now-int64_t(std::min<uint64_t>(utc>since?utc-since:0,60000))*1000);
            }
            animator_.start(assets_.package,clip_,direction_,loop_,start,explorer_?bedroomPlaybackScale(explorer_->state().gameplay):1);
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
            scene = currentExpeditionStop()?expeditionPosition():completionDestination()?endingPosition():raster(playerMotion_.sample(presentationNow_));
            (void)tile;
        } else if (explorer_ && !guildRooms_.empty()) {
            scene = guildRooms_[bedroomScene(explorer_->state().gameplay) ? 1 : 0].player().position;
            const auto& g=explorer_->state().gameplay;
            if(g.sceneBeat==SceneBeat::RoomExit || g.sceneBeat==SceneBeat::MorningFadeOut) {
                scene=bedroomExit(scene,g,utcMs()).position;
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
    void saveRunSnapshot(const std::filesystem::path& path,const ExplorationState& state) const {
        auto persistent=state;
        checkpointDialogue(persistent,dialogueCatalogue_,utcMs());
        checkpointHoliday(persistent,utcMs());
        if(options_.defaultExploration){persistent.target=-1;persistent.targetInstance=0;persistent.goalKind=GoalKind::None;}
        saveSnapshot(path,persistent);
    }
    void save() {
        if (explorer_) saveRunSnapshot(options_.snapshot, explorer_->state());
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
        if(explorer_) AppendMenuW(menu,MF_STRING|(explorer_->canResetExploration()?0:MF_GRAYED),ResetExploration,L"Reset 탐험 (귀가)");
        if(explorer_ && explorer_->state().expedition.enabled()) {
            const auto& state=explorer_->state();bool canRevisit=!state.completed;
            for(unsigned i=0;i<=state.floorIndex;++i)if(state.expedition.floors[i].fixedFloorId)canRevisit=false;
            AppendMenuW(menu,MF_STRING|(canRevisit?0:MF_GRAYED)|(state.expedition.revisit?MF_CHECKED:0),ExpeditionRevisit,L"재방문 경로");
            for(const auto& branch:expeditionBranches_)if(explorer_->canTakeExpeditionBranch(branch.first)) {
                const auto label=wide(branch.second.expedition.name)+L" 들렀다 오기";
                AppendMenuW(menu,MF_STRING,ExpeditionBranchBase+branch.first,label.c_str());
            }
        }

        if(explorer_) {
            const auto selection=characterSelection();
            for(auto role:{PortraitRole::Player,PortraitRole::Partner}) {
                const auto sub=CreatePopupMenu();winCheck(sub!=nullptr,"character menu");const auto ids=characterCatalogue_.menu(role);
                const auto selected=role==PortraitRole::Player?selection.player:selection.partner;
                for(unsigned i=0;i<ids.size();++i) {
                    const auto actor=characterCatalogue_.select(role,ids[i]);
                    const bool blocked=characterCatalogue_.sameSpecies(ids[i],role==PortraitRole::Player?selection.partner:selection.player);
                    const auto label=wide(actor.displayName)+(blocked?(role==PortraitRole::Player?L" (동료 선택 중)":L" (플레이어 선택 중)"):L"");
                    winCheck(AppendMenuW(sub,MF_STRING|(characterCatalogue_.sameSpecies(selected,ids[i])?MF_CHECKED:0)|(blocked?MF_GRAYED:0),(role==PortraitRole::Player?PlayerCharacterBase:PartnerCharacterBase)+i,label.c_str())!=0,"starter menu item");
                }
                winCheck(GetMenuItemCount(sub)==int(ids.size()),"starter menu count");
                winCheck(AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(sub),role==PortraitRole::Player?L"내 캐릭터":L"동료 캐릭터")!=0,"starter submenu");
                log("character menu role="+std::to_string(unsigned(role))+" options="+std::to_string(ids.size()));
            }
        }
        add(Quit, L"Exit");
        POINT cursor{}; GetCursorPos(&cursor);
        // Foreground is needed for tray menus to dismiss correctly; window remains NOACTIVATE.
        SetForegroundWindow(window_);
        UINT cmd = UINT(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window_, nullptr));
        DestroyMenu(menu); PostMessageW(window_, WM_NULL, 0, 0);
        if (cmd) command(cmd);
    }
    CharacterSelection characterSelection() const {
        return pendingCharacterSelection_.value_or(CharacterSelection{dialogueCast_.actor(PortraitRole::Player).portraitClip,dialogueCast_.actor(PortraitRole::Partner).portraitClip});
    }
    void requestCharacter(PortraitRole role,std::uint32_t id) {
        try {
            auto selection=characterSelection();if(role==PortraitRole::Player)selection.player=id;else selection.partner=id;
            characterCatalogue_.validate(selection);
            const auto actor=characterCatalogue_.select(role,id);
            auto candidate=Assets::load(options_.gameplayAssets/actor.bodyManifest);
            validateDialogueActor(actor,candidate.package,portraitAssets_->package);
            for(unsigned clip:{0U,1U,5U,6U,7U,13U,14U,15U,17U,22U,27U,33U,34U})for(unsigned dir=0;dir<8;++dir)candidate.package.frames(clip,dir);
            pendingCharacterSelection_=selection;
            log("character selection queued role="+std::to_string(unsigned(role))+" md="+std::to_string(id));dirty_=true;
        } catch(const std::exception& error) {log(std::string("character selection rejected: ")+error.what());}
    }
    void applyCharacterSelection(int64_t now) {
        if(!pendingCharacterSelection_ || !explorer_)return;
        const auto& state=explorer_->state();const auto& g=state.gameplay;
        if(varietyBusy(state.variety) || g.life==LifePhase::Dialogue || g.life==LifePhase::Combat || g.life==LifePhase::Death || g.life==LifePhase::Pose ||
           std::any_of(g.partyPlaybacks.begin(),g.partyPlaybacks.end(),[](const auto& v){return v.clip==1 || v.clip==6 || v.clip==33;}) ||
           (!g.homeScene && g.sceneBeat!=SceneBeat::None))return;
        const auto next=*pendingCharacterSelection_;
        try {
            const auto player=characterCatalogue_.select(PortraitRole::Player,next.player),partner=characterCatalogue_.select(PortraitRole::Partner,next.partner);
            auto playerAssets=Assets::load(options_.gameplayAssets/player.bodyManifest,&assets_);
            auto partnerAssets=Assets::load(options_.gameplayAssets/partner.bodyManifest,companionAssets_?&*companionAssets_:nullptr);
            validateDialogueActor(player,playerAssets.package,portraitAssets_->package);validateDialogueActor(partner,partnerAssets.package,portraitAssets_->package);
            saveCharacterSelection(characterProfile_,characterCatalogue_,next);
            animator_.stop();companionAnimator_.stop();guildLive_.clear();guildRoom_=-1;
            decodes_+=playerAssets.decoded?1:0;decodes_+=partnerAssets.decoded?1:0;
            assets_=std::move(playerAssets);companionAssets_=std::move(partnerAssets);
            for(auto& actor:dialogueCast_.actors)if(actor.role==PortraitRole::Player)actor=player;else if(actor.role==PortraitRole::Partner)actor=partner;
            pendingCharacterSelection_.reset();partyPlayerToken_=partyCompanionToken_=UINT64_MAX;completion_={};poseFinished_=false;
            clip_=gameplayClip(g);loop_=gameplayLoop(g);direction_=g.homeScene?bedroomDirection(g,guildRooms_[bedroomScene(g)?1:0].player().direction):g.playerDirection;
            startClip(now,true);companionFrame_=0;syncCompanion(now);syncPartyActors(now);dirty_=true;
            traceCharacterImagePending_=true;
            log("characters applied player="+std::to_string(next.player)+" partner="+std::to_string(next.partner));
        } catch(const std::exception& error) {
            pendingCharacterSelection_.reset();log(std::string("character apply rejected: ")+error.what());
            saveCharacterSelection(characterProfile_,characterCatalogue_,characterSelection());
        }
    }
    void command(UINT id) {
        if(explorer_ && id>=ExpeditionBranchBase && id<ExpeditionBranchBase+200) {
            const auto found=expeditionBranches_.find(id-ExpeditionBranchBase);
            if(found==expeditionBranches_.end())return;
            auto candidate=*explorer_;
            if(candidate.takeExpeditionBranch(found->second.floors,found->second.expedition,utcMs())) {
                auto npcs=stageNpcAssets(candidate.state());saveRunSnapshot(options_.snapshot,candidate.state());
                *explorer_=std::move(candidate);commitNpcAssets(std::move(npcs));completion_={};poseFinished_=false;
                playerMotion_.reset(tilePoint(explorer_->state().position));companionMotion_.reset(tilePoint(explorer_->state().gameplay.companion.tile));
                chunkVersion_=0;exploreDeadline_=nowUs();dirty_=true;log("expedition branch selected dungeon="+std::to_string(found->first));
            }
            return;
        }
        if(explorer_ && id==ExpeditionRevisit) {
            auto candidate=*explorer_;if(candidate.setExpeditionRevisit(!candidate.state().expedition.revisit)) {
                saveRunSnapshot(options_.snapshot,candidate.state());*explorer_=std::move(candidate);dirty_=true;log("expedition revisit="+std::to_string(explorer_->state().expedition.revisit));
            }
            return;
        }

        if(explorer_ && ((id>=PlayerCharacterBase && id<PlayerCharacterBase+100) || (id>=PartnerCharacterBase && id<PartnerCharacterBase+100))) {
            const auto role=id>=PartnerCharacterBase?PortraitRole::Partner:PortraitRole::Player;
            const auto ids=characterCatalogue_.menu(role);const auto index=id-(role==PortraitRole::Player?PlayerCharacterBase:PartnerCharacterBase);
            if(index<ids.size())requestCharacter(role,ids[index]);
            return;
        }
        if (explorer_ && (id == Loop || id == Finite || id == Stop || id == Reload || id == Background ||
            (id >= ClipBase && id < ClipBase + assets_.package.clips.size()) ||
            (id >= DirectionBase && id < DirectionBase + 8))) return;
        int64_t now = nowUs();
        if (id >= ClipBase && id < ClipBase + assets_.package.clips.size()) {
            clip_ = assets_.package.clips[id - ClipBase].id; startClip(now);
        } else if (id >= DirectionBase && id < DirectionBase + 8) { direction_ = id - DirectionBase; startClip(now); }
        else switch (id) {
            case ResetExploration: {
                if(!explorer_ || !explorer_->canResetExploration()) break;
                const auto utc=utcMs();auto candidate=*explorer_;
                candidate.resetExploration(utc);
                auto backup=options_.snapshot;backup+=L".reset-backup";
                saveRunSnapshot(backup,explorer_->state());
                saveRunSnapshot(options_.snapshot,candidate.state());
                *explorer_=std::move(candidate);
                if(paused_) pauseUtc_=utc;
                settleNavigationReset(now);
                log("navigation reset reason=manual backup="+backup.u8string());
                break;
            }
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
                Assets candidate = Assets::load(explorer_?options_.gameplayAssets / dialogueCast_.actor(PortraitRole::Player).bodyManifest:options_.package, &assets_);
                if(explorer_)validateDialogueActor(dialogueCast_.actor(PortraitRole::Player),candidate.package,portraitAssets_->package);
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
            case WM_COMMAND: command(LOWORD(w)); return 0;
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
        traceImage("-last.bmp");
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
    const Assets& npcAssets(uint32_t actor) const {
        const auto& character=npcCatalogue_.forActor(explorer_->state(),actor);
        const auto found=npcAssets_.find(character.actor.bodyManifest);
        if(found==npcAssets_.end())throw std::runtime_error("NPC body is not loaded: "+std::to_string(character.id));
        return *found->second;
    }
    std::optional<HolidayAssets> stageHolidayAssets(const ExplorationState& state) const {
        if(!state.variety.holiday.active){if(holidayAssets_.grounds.empty())return {};return HolidayAssets{};}
        if(!holidayAssets_.grounds.empty())return {};
        HolidayAssets next;const bool spring=state.variety.schedule.destination==DayDestination::HotSpring;
        const std::vector<std::string> grounds=spring?std::vector<std::string>{"P01P01A","P02P01A"}:std::vector<std::string>{"P01P01A","T01P01A","T01P02A"};
        for(const auto& name:grounds){auto asset=Assets::load(options_.gameplayAssets/"holiday"/(name+".mdpa"));next.decodes+=asset.decoded?1:0;next.grounds.emplace(name,std::move(asset));}
        if(spring) {
            next.water=Assets::load(options_.gameplayAssets/"holiday/water.mdpa");next.decodes+=next.water->decoded?1:0;
            for(auto role:{PortraitRole::Player,PortraitRole::Partner}) {
                const auto md=dialogueCast_.actor(role).portraitClip;const auto binding=holidayRegistry_.floats.find(md);
                if(binding==holidayRegistry_.floats.end())throw std::runtime_error("selected character lacks original Float");
                auto body=Assets::load(options_.gameplayAssets/"holiday"/binding->second.manifest);
                if(body.package.assetKey!=binding->second.key || body.package.pngSha256!=binding->second.hash)throw std::runtime_error("holiday Float identity mismatch");
                for(unsigned dir=0;dir<8;++dir)body.package.frames(20,dir);
                next.decodes+=body.decoded?1:0;next.floats.emplace(md,std::move(body));
            }
        }
        return next;
    }
    void addHolidayLayers(std::vector<Layer>& layers,Point& camera,std::uint64_t utc) const {
        const auto& state=explorer_->state();const auto sample=holidaySample(state,holidayRegistry_,utc);
        const auto& ground=holidayAssets_.grounds.at(sample.scene);const auto& background=ground.package.frames(0,0).front();
        const WorldPoint center{double(sample.player.x+sample.partner.x)/2,double(sample.player.y+sample.partner.y)/2};
        camera=sample.scene=="P02P01A"?Point{184,100}:cameraFor(center,ground.package.width,ground.package.height,int((size_+scale_-1)/scale_));
        layers.push_back({ground.atlas.get(),background.rect,background.anchor,{0,0}});
        const auto elapsed=utc>state.variety.holiday.startedMs?utc-state.variety.holiday.startedMs:0;
        if(sample.npcVisible) {
            const auto& npc=npcAssets(kHolidayActorId);const auto index=holidayFrame(npc.package,7,sample.npcDirection,elapsed);
            const auto& f=npc.package.frames(7,sample.npcDirection)[index];layers.push_back({npc.atlas.get(),f.rect,f.anchor,sample.npc});
        }
        auto water=[&](bool front,Point point) {
            const auto& source=*holidayAssets_.water;const unsigned clip=1250+unsigned(front);
            const auto& f=source.package.frames(clip,0)[holidayFrame(source.package,clip,0,elapsed)];layers.push_back({source.atlas.get(),f.rect,f.anchor,point});
        };
        if(sample.bathing){water(false,sample.player);water(false,sample.partner);}
        auto body=[&](PortraitRole role,Point point,unsigned direction) {
            const auto md=dialogueCast_.actor(role).portraitClip;
            const auto& source=sample.bathing?holidayAssets_.floats.at(md):role==PortraitRole::Player?assets_:*companionAssets_;
            const auto clip=sample.bathing?20U:sample.moving?0U:7U;
            const auto& f=source.package.frames(clip,direction)[holidayFrame(source.package,clip,direction,elapsed)];layers.push_back({source.atlas.get(),f.rect,f.anchor,point});
        };
        body(PortraitRole::Player,sample.player,sample.playerDirection);body(PortraitRole::Partner,sample.partner,sample.partnerDirection);
        if(sample.bathing){water(true,sample.player);water(true,sample.partner);}
    }
    std::optional<NpcAssetsChange> stageNpcAssets(const ExplorationState& state) {
        std::set<std::string> needed;
        if(state.variety.holiday.active)needed.insert(npcCatalogue_.select(state.variety.holiday.npcMd).actor.bodyManifest);
        else for(const auto& e:state.population.entities)if(e.kind==EntityKind::Friend && e.visible)
            needed.insert(npcCatalogue_.select(e.originalId).actor.bodyManifest);
        if(needed.size()==npcAssets_.size() && std::all_of(needed.begin(),needed.end(),[&](const auto& key){return npcAssets_.count(key);}))return {};
        NpcAssetsChange change;
        for(const auto& key:needed) {
            const auto old=npcAssets_.find(key);
            auto asset=old!=npcAssets_.end()?old->second:std::make_shared<Assets>(Assets::load(options_.gameplayAssets/key));
            if(old==npcAssets_.end())change.decodes+=asset->decoded?1:0;
            if(state.variety.holiday.active)validateNpcCharacter(npcCatalogue_.select(state.variety.holiday.npcMd),asset->package,portraitAssets_->package);
            for(const auto& e:state.population.entities)if(e.kind==EntityKind::Friend) {
                const auto& character=npcCatalogue_.select(e.originalId);
                if(character.actor.bodyManifest==key)validateNpcCharacter(character,asset->package,portraitAssets_->package);
            }
            change.cache.emplace(key,std::move(asset));
        }
        return change;
    }
    void commitNpcAssets(std::optional<NpcAssetsChange> change) {
        if(!change)return;
        // Animators retain frame pointers. Stop them before dropping a previous floor's body.
        enemyAnimator_.stop();enemyFrame_=0;actorAnimators_.clear();actorMotions_.clear();actorFrames_.clear();
        partyActorClips_.clear();partyActorDirections_.clear();partyActorTokens_.clear();
        decodes_+=change->decodes;npcAssets_=std::move(change->cache);
        log("NPC cache bodies="+std::to_string(npcAssets_.size())+" decoded="+std::to_string(change->decodes));
    }
    const DialogueActor* npcSpeaker(DialoguePortraits portrait) const {
        return portrait.speaker==PortraitRole::Friendly?
            &npcCatalogue_.forActor(explorer_->state(),portrait.actorId).actor:nullptr;
    }
    DialoguePortraits currentPortrait() const {
        auto portrait=cataloguePortraits(explorer_->state(),dialogueCatalogue_);
        if(portrait.speaker==PortraitRole::Friendly)
            portrait.emotion=npcEmotion(npcCatalogue_.forActor(explorer_->state(),portrait.actorId),portrait.emotion);
        return portrait;
    }
    uint32_t npcClip(uint32_t requested,const Assets& source) const {
        const auto& clips=source.package.clips;
        return std::any_of(clips.begin(),clips.end(),[&](const auto& c){return c.id==requested;})?requested:requested==33?6:7;
    }
    bool partyRendering() const {
        if(!explorer_)return false;
        if(explorer_->state().variety.holiday.active)return false;
        const auto& g=explorer_->state().gameplay;
        return g.independentParty && !g.homeScene && g.sceneBeat==SceneBeat::None &&
            (g.life==LifePhase::Explore || g.life==LifePhase::Combat);
    }
    void collectPartyCompletion(uint32_t actor,uint64_t action) {
        if(!partyRendering())return;
        const auto& s=explorer_->state();const auto* v=partyPlayback(s.gameplay,actor);
        if(!v || !action || v->id!=action)return;
        completion_.keyed=true;
        if(std::none_of(completion_.actions.begin(),completion_.actions.end(),
            [&](const auto& old){return old.actorId==actor && old.actionId==action;}))
            completion_.actions.push_back({s.runId,v->round,action,s.floorIndex,actor});
    }
    void syncPartyActors(int64_t now) {
        if(!partyRendering())return;
        const auto& s=explorer_->state();const auto& g=s.gameplay;
        const auto utc=utcMs();
        auto sync=[&](uint32_t actor,const Assets& source,Animator& animator,
                      auto& clip,auto& facing,auto& frame,auto& token) {
            const auto* v=partyPlayback(g,actor);
            auto wanted=partyClip(g,actor);
            if(!v && ((actor==0 && playerMotion_.active(now)) || (actor==kCompanionActorId && companionMotion_.active(now))))wanted=0;
            if(!v && actor!=0 && actor!=kCompanionActorId &&
               actorMotions_.count(actor) && actorMotions_.at(actor).active(now))wanted=0;
            const auto actual=actor!=0 && actor!=kCompanionActorId?npcClip(wanted,source):wanted;
            const unsigned dir=v?v->direction:actor==0?g.playerDirection:
                actor==kCompanionActorId?g.companion.direction:
                std::find_if(s.population.entities.begin(),s.population.entities.end(),
                    [&](const auto& e){return e.instanceId==actor;})->direction;
            const uint64_t action=v?v->id:0;
            const bool loop=actual==0 || actual==7;
            if(!animator.active() || token!=action || clip!=actual || (!loop && facing!=dir)) {
                uint64_t started=utc;
                if(v)started=v->startedMs;
                else if(actual==0)started=actor==0?g.walkStartedMs:
                    actor==kCompanionActorId?g.companion.walkStartedMs:
                    std::find_if(s.population.entities.begin(),s.population.entities.end(),
                        [&](const auto& e){return e.instanceId==actor;})->movement.startedMs;
                const auto elapsed=std::min<uint64_t>(60000,utc>started?utc-started:0);
                animator.start(source.package,actual,dir,loop,
                    std::max<int64_t>(0,now-int64_t(elapsed)*1000));
                frame=0;dirty_=true;
            } else if(facing!=dir && !animator.tryChangeDirection(source.package,actual,dir))
                animator.start(source.package,actual,dir,loop,now);
            clip=actual;facing=dir;token=action;
        };
        sync(0,assets_,animator_,clip_,direction_,frame_,partyPlayerToken_);
        loop_=clip_==0 || clip_==7;
        sync(kCompanionActorId,*companionAssets_,companionAnimator_,
            companionClip_,companionDirection_,companionFrame_,partyCompanionToken_);
        for(const auto& e:s.population.entities) if(e.kind==EntityKind::Friend && e.visible && (!e.done || std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)==g.hostile.end()))
            sync(e.instanceId,npcAssets(e.instanceId),actorAnimators_[e.instanceId],
                partyActorClips_[e.instanceId],partyActorDirections_[e.instanceId],
                actorFrames_[e.instanceId],partyActorTokens_[e.instanceId]);
    }
    void syncCompanion(int64_t now) {
        if(partyRendering())return;
        const auto& g=explorer_->state().gameplay;
        if(!g.companion.enabled || !dungeonScene(g)) {companionAnimator_.stop();return;}
        const auto clip=companionClip(g);const auto dir=dialogueDirection(explorer_->state(),kCompanionActorId);
        const bool loop=companionLoop(g);
        if(!companionAnimator_.active() || clip!=companionClip_ || (!loop && companionPhaseStarted_!=g.phaseStartedMs) || (dir!=companionDirection_ && clip!=0)) {
            auto utc=utcMs();auto started=clip==0?g.companion.walkStartedMs:g.phaseStartedMs;
            const auto elapsed=std::min<uint64_t>(60000,utc>started?utc-started:0);
            companionAnimator_.start(companionAssets_->package,clip,dir,loop,std::max<int64_t>(0,now-int64_t(elapsed)*1000));companionFrame_=0;
        } else if(dir!=companionDirection_ && !companionAnimator_.tryChangeDirection(companionAssets_->package,clip,dir))
            companionAnimator_.start(companionAssets_->package,clip,dir,true,now);
        companionClip_=clip;companionDirection_=dir;companionPhaseStarted_=g.phaseStartedMs;
    }
    void settleNavigationReset(int64_t now) {
        playerMotion_.reset(playerMotion_.sample(now));companionMotion_.reset(companionMotion_.sample(now));companionAnimator_.stop();
        actorMotions_.clear();actorAnimators_.clear();actorFrames_.clear();enemyAnimator_.stop();
        completion_={};poseFinished_=false;clip_=gameplayClip(explorer_->state().gameplay);loop_=gameplayLoop(explorer_->state().gameplay);
        exploreDeadline_=deadlineFromUtc(now,utcMs(),nextGameplayDeadline(explorer_->state()));
        startClip(now);dirty_=true;
    }
    static std::map<uint32_t,Assets> loadExplorationTiles(const ExplorationPackage& package,const std::filesystem::path& directory) {
        std::map<uint32_t,Assets> result;
        for(const auto& tileset:package.tilesets) {
            auto asset=Assets::load(directory/tileset.packageFile);
            if(asset.package.frames(0,0).size()!=tileset.chunkCount)
                throw std::runtime_error("tileset atlas count mismatch");
            result.emplace(tileset.id,std::move(asset));
        }
        return result;
    }
    void advanceExploration(int64_t now) {
        if (!explorer_ || blocked() || (now < exploreDeadline_ && !poseFinished_ && completion_.actions.empty())) return;
        const int oldPosition = explorer_->state().position;
        const auto oldCompanion=explorer_->state().gameplay.companion;
        const auto oldEntities=explorer_->state().population.entities;
        const unsigned oldDirection = direction_;
        const auto oldLife = explorer_->state().gameplay.life;
        const auto oldCombat = explorer_->state().gameplay.combat;
        const auto oldBeat=explorer_->state().gameplay.sceneBeat;
        const auto oldDialogue=explorer_->state().gameplay.dialogueStage;
        auto candidate = *explorer_;
        if(!options_.solo && candidate.state().gameplay.life==LifePhase::Explore) candidate.enableCompanion();
        const auto& before=candidate.state().gameplay;
        if(options_.defaultExploration && before.life==LifePhase::Ready && before.sceneBeat==SceneBeat::MorningFadeOut && utcMs()>=before.deadlineMs &&
           !before.onceKeys.count("guild-start:"+std::to_string(candidate.state().runId)) && !pendingDefaultPackage_) {
            std::vector<unsigned> ids;for(const auto& entry:explorationPool_)ids.push_back(entry.first);
            const auto selected=candidate.chooseNextDungeon(ids);const auto& next=explorationPool_.at(selected);candidate.setNextRunPackage(next.assetVersion,next.floors,next.expedition);
        }
        const auto event = candidate.advanceGameplay(utcMs(), completion_,dialogueCatalogue_,&eventRegistry_,&holidayRegistry_);
        poseFinished_ = false; completion_={};
        if (encodeSnapshot(candidate.state()) == encodeSnapshot(explorer_->state())) {
            exploreDeadline_=deadlineFromUtc(now,utcMs(),nextGameplayDeadline(candidate.state()));
            if (candidate.state().gameplay.life == LifePhase::ReturnHome) dirty_ = true;
            return;
        }
        const ExplorationPackage* nextPackage=nullptr;
        if(candidate.state().assetVersion!=explorationPackage_->assetVersion) {
            if(pendingDefaultPackage_ && candidate.state().assetVersion==pendingDefaultPackage_->assetVersion)nextPackage=&*pendingDefaultPackage_;
            else for(const auto& entry:explorationPool_)if(entry.second.assetVersion==candidate.state().assetVersion){nextPackage=&entry.second;break;}
            if(!nextPackage)throw std::runtime_error("unregistered daily exploration package");
        }
        const bool changePackage=nextPackage!=nullptr;
        std::map<uint32_t,Assets> nextTiles;
        if(changePackage)nextTiles=loadExplorationTiles(*nextPackage,explorationRoot_/std::filesystem::path("dungeon-"+std::to_string(nextPackage->floors.front().dungeonId)));
        auto nextNpcs=stageNpcAssets(candidate.state());
        auto nextHoliday=stageHolidayAssets(candidate.state());
        stageExpeditionGround(candidate.state());
        saveRunSnapshot(options_.snapshot, candidate.state());
        commitNpcAssets(std::move(nextNpcs));
        if(nextHoliday){decodes_+=nextHoliday->decodes;holidayAssets_=std::move(*nextHoliday);}
        *explorer_ = std::move(candidate); // commit state before exposing inventory/clear event
        if(changePackage) {
            for(const auto& entry:nextTiles)decodes_+=entry.second.decoded?1:0;
            tiles_=std::move(nextTiles);explorationPackage_=*nextPackage;
            pendingDefaultPackage_.reset();chunkVersion_=0;chunks_.fill(0);
            log("default exploration upgraded at new run floors="+std::to_string(explorer_->state().floors.size()));
        }
        const auto& g = explorer_->state().gameplay;
        if(explorer_->state().expedition.enabled() && (oldBeat!=g.sceneBeat || oldLife!=g.life || event.event==GameplayEvent::FloorChanged)) {
            const auto& state=explorer_->state();const auto* stop=expeditionScene(state)?expeditionStop(state):nullptr;
            log("expedition="+state.expedition.routeId+" segment="+std::to_string(state.floors[state.floorIndex].dungeonId)+" localFloor="+std::to_string(state.floors[state.floorIndex].localFloor)+" ground="+(stop?stop->groundId:std::string("none"))+" outcome="+std::to_string(unsigned(state.expedition.outcome)));
        }
        const bool newFloor=event.event==GameplayEvent::FloorChanged || (event.event==GameplayEvent::Explore && oldLife==LifePhase::Ready);
        if(g.companion.enabled) {
            if(newFloor || !oldCompanion.enabled || event.event==GameplayEvent::Warp) companionMotion_.reset(tilePoint(g.companion.tile));
            else if(oldCompanion.tile!=g.companion.tile) companionMotion_.begin(kCompanionActorId,g.companion.movement.actionId,tilePoint(oldCompanion.tile),tilePoint(g.companion.tile),now,g.companion.direction);
        }
        syncCompanion(now);
        exploreDeadline_=deadlineFromUtc(now,utcMs(),nextGameplayDeadline(explorer_->state()));
        for(const auto& e:explorer_->state().population.entities) {
            const auto old=std::find_if(oldEntities.begin(),oldEntities.end(),[&](const auto& x){return x.instanceId==e.instanceId;});
            if(old!=oldEntities.end() && old->tile!=e.tile && e.kind==EntityKind::Friend) {
                actorMotions_[e.instanceId].begin(e.instanceId,g.actionRound,tilePoint(old->tile),tilePoint(e.tile),now,e.direction);
                auto& animation=actorAnimators_[e.instanceId];
                // A prior dialogue may leave Idle/Nod in the cache. Preserve
                // phase only within a compatible Walk; never relabel another clip.
                if(!partyRendering()) {
                    if(g.independentParty || !animation.tryChangeDirection(npcAssets(e.instanceId).package,0,e.direction))
                        animation.start(npcAssets(e.instanceId).package,0,e.direction,true,now);
                }
            }
        }
        if(partyRendering()) {
            if(event.event==GameplayEvent::Warp)playerMotion_.reset(tilePoint(explorer_->state().position));
            else if(oldPosition!=explorer_->state().position)
                playerMotion_.begin(0,g.movement.actionId,tilePoint(oldPosition),
                    tilePoint(explorer_->state().position),now,g.playerDirection);
            syncPartyActors(now);
            if(!event.onceKey.empty())log("gameplay onceKey="+event.onceKey);
            for(const auto& a:event.actors)
                log("party actor="+std::to_string(a.actorId)+" target="+std::to_string(a.targetId)+
                    " action="+std::to_string(a.actionId)+" round="+std::to_string(a.round)+
                    " event="+std::to_string(static_cast<unsigned>(a.event))+" damage="+std::to_string(a.damage));
            if(oldLife!=g.life)log(std::string("life=")+lifeName(g.life)+" combat="+combatName(g.combat)+
                " day="+std::to_string(g.day)+" hearts="+std::to_string(g.hearts));
            dirty_=true;return;
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
        if(completionScene(g))direction_=g.playerDirection;
        if(companionDialogue(g)) direction_=g.playerDirection;
        else if (g.life == LifePhase::Combat || g.life==LifePhase::Dialogue) {
            const auto entity = std::find_if(explorer_->state().population.entities.begin(), explorer_->state().population.entities.end(), [&](const auto& e) { return e.instanceId == g.enemyInstance; });
            const int dx = entity->tile % kWidth - oldPosition % kWidth, dy = entity->tile / kWidth - oldPosition / kWidth;
            if(g.combat!=CombatPhase::PlayerApproach || g.life!=LifePhase::Combat) direction_ = directionBetween(explorer_->state().position,entity->tile);
            (void)dx; (void)dy;
            enemyDirection_ = entity->direction;
        }
        if (g.homeScene) direction_ = g.sceneBeat==SceneBeat::RoomExit || g.sceneBeat==SceneBeat::MorningFadeOut?
            bedroomExit(guildRooms_[1].player().position,g,utcMs()).direction:guildRooms_[bedroomScene(g)?1:0].player().direction;
        direction_=bedroomDirection(g,direction_);
        if(g.sceneBeat==SceneBeat::DiningPan && oldBeat!=g.sceneBeat) guildCamera_.begin(UINT32_MAX,g.day,{368,212},{168,212},now,6,4000000);
        if (clip_ == 0 && nextClip == 0 && loop_ && oldLife == g.life && oldCombat == g.combat) {
            if(oldDirection != direction_ && !animator_.tryChangeDirection(assets_.package,0,direction_))startClip(now);
        } else if (clip_ != nextClip || loop_ != nextLoop || oldDirection != direction_ || oldLife != g.life || oldCombat != g.combat || oldDialogue!=g.dialogueStage || oldBeat!=g.sceneBeat) { clip_ = nextClip; loop_ = nextLoop; startClip(now); }
        if(event.event == GameplayEvent::FloorChanged || (event.event == GameplayEvent::Explore && oldLife==LifePhase::Ready)) {playerMotion_.reset(tilePoint(explorer_->state().position));actorMotions_.clear();actorAnimators_.clear();}
        if (!companionDialogue(g) && (g.life == LifePhase::Combat || g.life==LifePhase::Dialogue) && (oldLife != g.life || oldCombat != g.combat || oldDialogue!=g.dialogueStage)) {
            enemyClip_ = g.life==LifePhase::Dialogue ? (g.dialogueStage==DialogueStage::Npc?npcClip(22,npcAssets(g.enemyInstance)):7) : g.combat == CombatPhase::EnemyAttack ? 1 : g.combat == CombatPhase::EnemyHurt ? 6 : g.combat == CombatPhase::Faint ? npcClip(33,npcAssets(g.enemyInstance)) : 7;
            enemyFrame_ = 0;
            enemyAnimator_.start(npcAssets(g.enemyInstance).package, enemyClip_, enemyDirection_, enemyClip_ == 7 && !(g.life==LifePhase::Dialogue && g.dialogueStage==DialogueStage::Npc), now);
        }
        if (!event.onceKey.empty()) log("gameplay onceKey=" + event.onceKey);
        if (oldLife != g.life || oldCombat != g.combat || oldBeat!=g.sceneBeat || oldDialogue!=g.dialogueStage) log(std::string("life=") + lifeName(g.life) + " combat=" + combatName(g.combat) + " day=" + std::to_string(g.day) + " hearts=" + std::to_string(g.hearts) + " sceneBeat=" + std::to_string(static_cast<unsigned>(g.sceneBeat)) + " dialogueStage=" + std::to_string(static_cast<unsigned>(g.dialogueStage)));
        if (event.event == GameplayEvent::UseOranBerry) log("OranBerry70 text: restored all hearts; consumes one player action");
        if (event.event == GameplayEvent::PlayerAttack || event.event == GameplayEvent::EnemyAttack)
            log("move355 damage=" + std::to_string(event.damage) + " critical=" + std::to_string(event.critical));
        if (event.event == GameplayEvent::HomeScene || (event.event==GameplayEvent::ReturnHome && g.homeScene)) log(std::string("life=HomeScene opaque original ")+(bedroomScene(g)?(nightScene(g)?"night bedroom":"day bedroom"):"dining map"));
        if(event.navigationDetour) log("navigation detour reason=repeated-joint-state from="+std::to_string(oldPosition)+" to="+std::to_string(explorer_->state().position));
        if(g.life==LifePhase::ReturnHome) dirty_=true;
        dirty_ = true;
    }
    void advanceGuildActors(int64_t now) {
        if (!explorer_ || dungeonScene(explorer_->state().gameplay)) { guildLive_.clear(); appleAnimator_.stop(); guildRoom_ = -1; return; }
        const auto& g = explorer_->state().gameplay;
        const int room = bedroomScene(g)?1:0;
        if(guildAppleVisible(g)) {
            if(!appleAnimator_.active())appleAnimator_.start(appleAssets_->package,624,0,true,now);
            const auto sample=appleAnimator_.advance(now);
            dirty_=dirty_ || appleFrame_!=sample.frame;appleFrame_=sample.frame;
            if(sample.nextDeadlineUs && (!nextFrameDeadline_ || *sample.nextDeadlineUs<*nextFrameDeadline_))nextFrameDeadline_=sample.nextDeadlineUs;
        } else appleAnimator_.stop();
        if (guildRoom_ != room || guildLife_ != g.life || guildBeat_!=g.sceneBeat) {
            guildLive_.clear(); guildRoom_ = room; guildLife_ = g.life;guildBeat_=g.sceneBeat;
            for (const auto& cue : guildRooms_[room].actors) {
                if (cue.kind == GuildActorKind::Player) continue;
                if(room==1 && cue.id==109 && g.sceneBeat!=SceneBeat::WakeDelay && g.sceneBeat!=SceneBeat::AlarmEnter && g.sceneBeat!=SceneBeat::AlarmLeave)continue;
                const Assets* source = nullptr;
                if (cue.kind == GuildActorKind::Partner) source = &*companionAssets_;
                else {
                    auto it = guildAssets_.find(cue.assetFile);
                    if (it == guildAssets_.end()) {
                        auto loaded = Assets::load(options_.gameplayAssets / (cue.id==109?dialogueCast_.actor(PortraitRole::Alarm).bodyManifest:cue.assetFile));
                        if(cue.id==109)validateDialogueActor(dialogueCast_.actor(PortraitRole::Alarm),loaded.package,portraitAssets_->package);
                        decodes_ += loaded.decoded ? 1 : 0;
                        it = guildAssets_.emplace(cue.assetFile, std::move(loaded)).first;
                    }
                    source = &it->second;
                }
                GuildLiveActor actor; actor.cue = &cue; actor.assets = source; actor.direction = cue.direction;
                if(cue.kind==GuildActorKind::Partner)actor.direction=bedroomDirection(g,actor.direction,true);
                actor.clip = cue.kind == GuildActorKind::Object ? cue.actionClip :
                    cue.kind == GuildActorKind::Partner ? (room == 1 ? gameplayClip(g) : g.life == LifePhase::Eat ? 15U : 7U) :
                    g.life == LifePhase::Eat ? cue.actionClip : 7U;
                if(room==1 && cue.id==109) {actor.clip=g.sceneBeat==SceneBeat::WakeDelay?cue.actionClip:0;actor.direction=g.sceneBeat==SceneBeat::AlarmLeave?6:2;}
                if(room==1 && cue.kind==GuildActorKind::Partner && (g.sceneBeat==SceneBeat::RoomExit || g.sceneBeat==SceneBeat::MorningFadeOut)) {
                    const auto sample=bedroomExit(cue.position,g,utcMs(),true);
                    actor.clip=sample.moving?0:7;actor.direction=sample.direction;
                }
                actor.animator.start(source->package, actor.clip, actor.direction, room==1?(cue.id==109?true:gameplayLoop(g)):true, cue.id==109 && room==1?restoredClipStart(now):sceneClipStart(now),cue.kind==GuildActorKind::Partner?bedroomPlaybackScale(g):1);
                guildLive_.push_back(std::move(actor));
            }
            log("guild room=" + guildRooms_[room].name + " characters=" + std::to_string(1+std::count_if(guildLive_.begin(),guildLive_.end(),[](const auto& actor){return actor.cue->kind!=GuildActorKind::Object;})) + " objectCount=" + std::to_string(room == 0 ? 1 : 0));
            dirty_ = true;
        }
        for (auto& actor : guildLive_) {
            if(g.sceneBeat==SceneBeat::RoomExit && actor.cue->kind==GuildActorKind::Partner) {
                const auto sample=bedroomExit(actor.cue->position,g,utcMs(),true);
                const unsigned clip=sample.moving?0U:7U;
                if(actor.clip!=clip) {
                    actor.clip=clip;actor.direction=sample.direction;
                    actor.animator.start(actor.assets->package,clip,actor.direction,true,now);dirty_=true;
                } else if(actor.direction!=sample.direction) {
                    actor.direction=sample.direction;actor.animator.changeDirection(actor.assets->package,clip,actor.direction);dirty_=true;
                }
            }
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
            if (result.finishedEvent && actor.cue->kind == GuildActorKind::Partner && g.life == LifePhase::Wake && g.sceneBeat==SceneBeat::Waking) log("guild partner wake finished");
            dirty_ = dirty_ || actor.frame != result.frame; actor.frame = result.frame;
            if (result.nextDeadlineUs && (!nextFrameDeadline_ || *result.nextDeadlineUs < *nextFrameDeadline_)) nextFrameDeadline_ = result.nextDeadlineUs;
        }
    }
    void render(int64_t now) {
        presentationNow_=now;
        synchronizePause(now);
        if (blocked()) return;
        syncPartyActors(now);
        const auto playerPoint=raster(playerMotion_.sample(now));
        if(playerPoint.x!=lastPlayerDraw_.x||playerPoint.y!=lastPlayerDraw_.y) {dirty_=true;lastPlayerDraw_=playerPoint;}
        for(auto& entry:actorMotions_) {
            if(partyRendering()){if(entry.second.active(now))dirty_=true;continue;}
            if(entry.second.active(now)) {dirty_=true;auto result=actorAnimators_[entry.first].advance(now);actorFrames_[entry.first]=result.frame;}
            else if(actorAnimators_[entry.first].active()) {actorAnimators_[entry.first].stop();dirty_=true;}
        }
        advanceExploration(now);
        applyCharacterSelection(now);
        if(explorer_ && explorer_->state().gameplay.life==LifePhase::Dialogue) {
            const auto& state=explorer_->state();const auto facing=dialogueDirection(state,0);
            if(direction_!=facing){direction_=facing;if(!animator_.tryChangeDirection(assets_.package,clip_,facing))startClip(now,true);dirty_=true;}
            if(!companionDialogue(state.gameplay)) {
                const auto npcFacing=dialogueDirection(state,state.gameplay.enemyInstance);
                if(enemyDirection_!=npcFacing){enemyDirection_=npcFacing;
                    if(!enemyAnimator_.tryChangeDirection(npcAssets(state.gameplay.enemyInstance).package,enemyClip_,npcFacing)) {
                        enemyFrame_=0;enemyAnimator_.start(npcAssets(state.gameplay.enemyInstance).package,enemyClip_,npcFacing,enemyClip_==7 && state.gameplay.dialogueStage!=DialogueStage::Npc,sceneClipStart(now));
                    }
                    dirty_=true;}
            }
            syncCompanion(now);
        }
        if(explorer_ && explorer_->state().gameplay.sceneBeat==SceneBeat::RoomExit) {
            const auto sample=bedroomExit(guildRooms_[1].player().position,explorer_->state().gameplay,utcMs());
            const unsigned clip=sample.moving?0U:7U;
            if(clip_!=clip) {clip_=clip;direction_=sample.direction;loop_=true;startClip(now);}
            else if(direction_!=sample.direction) {direction_=sample.direction;animator_.changeDirection(assets_.package,clip,direction_);dirty_=true;}
        }
        bool playerFinished = false;
        if (animator_.active()) {
            auto result = animator_.advance(now);
            dirty_ = dirty_ || frame_ != result.frame; frame_ = result.frame;
            if(!holdingScenePose()) {markers_ += result.markers.size();finishes_ += result.finishedEvent?1:0;skippedCycles_ += result.skippedCycles;}
            nextFrameDeadline_ = result.nextDeadlineUs;
            if (result.finishedEvent && !holdingScenePose()) log("finite finished assetKey=" + assets_.package.assetKey);
            if(explorer_) completion_.player=result.finished;
            if(result.finished)collectPartyCompletion(0,partyPlayerToken_);
            playerFinished = result.finished;
        }
        if (explorer_ && !companionDialogue(explorer_->state().gameplay) && (explorer_->state().gameplay.life == LifePhase::Combat || explorer_->state().gameplay.life==LifePhase::Dialogue) && !partyRendering() && enemyAnimator_.active()) {
            const auto result = enemyAnimator_.advance(now);
            dirty_ = dirty_ || enemyFrame_ != result.frame; enemyFrame_ = result.frame;
            if (result.nextDeadlineUs && (!nextFrameDeadline_ || *result.nextDeadlineUs < *nextFrameDeadline_)) nextFrameDeadline_ = result.nextDeadlineUs;
            completion_.enemy=result.finished;
        }
        if(explorer_ && dungeonScene(explorer_->state().gameplay) && companionAnimator_.active()) {
            const auto point=raster(companionMotion_.sample(now));
            dirty_=dirty_ || point.x!=lastCompanionDraw_.x || point.y!=lastCompanionDraw_.y;lastCompanionDraw_=point;
            const auto result=companionAnimator_.advance(now);dirty_=dirty_ || companionFrame_!=result.frame;companionFrame_=result.frame;
            completion_.partner=result.finished;
            if(result.finished)collectPartyCompletion(kCompanionActorId,partyCompanionToken_);
            if(result.nextDeadlineUs && (!nextFrameDeadline_ || *result.nextDeadlineUs<*nextFrameDeadline_)) nextFrameDeadline_=result.nextDeadlineUs;
        }
        if(partyRendering()) {
            completion_.keyed=true;
            const auto& state=explorer_->state();
            for(const auto& e:state.population.entities) {
                auto found=actorAnimators_.find(e.instanceId);
                if(e.kind!=EntityKind::Friend || found==actorAnimators_.end() || !found->second.active())continue;
                const auto result=found->second.advance(now);
                dirty_=dirty_ || actorFrames_[e.instanceId]!=result.frame;
                actorFrames_[e.instanceId]=result.frame;
                if(result.finished)collectPartyCompletion(e.instanceId,partyActorTokens_[e.instanceId]);
                if(result.nextDeadlineUs && (!nextFrameDeadline_ || *result.nextDeadlineUs<*nextFrameDeadline_))
                    nextFrameDeadline_=result.nextDeadlineUs;
            }
        }
        advanceGuildActors(now);
        if (explorer_ && bedroomScene(explorer_->state().gameplay))
            completion_.partner = std::all_of(guildLive_.begin(), guildLive_.end(), [](const auto& actor) { return actor.cue->kind != GuildActorKind::Partner || actor.finished; });
        if(explorer_) {
            const auto& g=explorer_->state().gameplay;
            const bool enemy=g.life==LifePhase::Combat && (g.combat==CombatPhase::EnemyAttack || g.combat==CombatPhase::EnemyHurt || g.combat==CombatPhase::Faint);
            const bool partner=g.life==LifePhase::Combat && (g.combat==CombatPhase::CompanionAttack || g.combat==CombatPhase::CompanionHurt || g.combat==CombatPhase::CompanionFaint);
            const bool joint=(g.life==LifePhase::Pose && g.companion.enabled) || (g.life==LifePhase::Wake && g.sceneBeat==SceneBeat::Waking) || g.sceneBeat==SceneBeat::BedFace || g.sceneBeat==SceneBeat::MorningFace || (g.life==LifePhase::Sleep && g.sceneBeat==SceneBeat::Sleeping);
            const bool body=(g.life==LifePhase::Wake && g.sceneBeat==SceneBeat::Waking) || g.life==LifePhase::Pose || g.life==LifePhase::Emotion || (g.life==LifePhase::Combat &&
                (g.combat==CombatPhase::PlayerAttack || g.combat==CombatPhase::PlayerHurt || g.combat==CombatPhase::PlayerFaint || enemy || partner));
            poseFinished_=joint?(completion_.player&&completion_.partner):body?(enemy?completion_.enemy:partner?completion_.partner:completion_.player):false;
            if(g.life==LifePhase::Dialogue && utcMs()>=g.deadlineMs)
                poseFinished_=g.dialogueStage==DialogueStage::Player?completion_.player:companionDialogue(g)?completion_.partner:completion_.enemy;
            if(g.life==LifePhase::Death || g.life==LifePhase::ReturnHome || g.sceneBeat!=SceneBeat::None) dirty_=true;
        }
        if(explorer_ && explorer_->state().gameplay.sceneBeat==SceneBeat::RoomExit) {
            completion_.player=bedroomExit(guildRooms_[1].player().position,explorer_->state().gameplay,utcMs()).finished;
            completion_.partner=std::all_of(guildLive_.begin(),guildLive_.end(),[&](const auto& actor) {
                return actor.cue->kind!=GuildActorKind::Partner || bedroomExit(actor.cue->position,explorer_->state().gameplay,utcMs(),true).finished;
            });
            poseFinished_=completion_.player && completion_.partner;
        }
        if(explorer_ && (explorer_->state().gameplay.sceneBeat==SceneBeat::BedFace || explorer_->state().gameplay.sceneBeat==SceneBeat::MorningFace) && utcMs()<explorer_->state().gameplay.deadlineMs)poseFinished_=false;
        if(partyRendering())poseFinished_=!completion_.actions.empty();
        (void)playerFinished;
        if(explorer_ && revealedCatalogueText(explorer_->state(),dialogueCatalogue_,utcMs())!=submittedText_)dirty_=true;
        if (!dirty_) {traceMovement(now,false);return;}
        std::vector<Layer> layers;
        std::optional<Layer> playerLayer;
        Point camera{0, 0};
        if(explorer_ && explorer_->state().variety.holiday.active) {
            addHolidayLayers(layers,camera,utcMs());
        } else if(currentExpeditionStop()) {
            const auto& frame=expeditionGround_->package.frames(0,0).front();camera=expeditionCamera();
            layers.push_back({expeditionGround_->atlas.get(),frame.rect,frame.anchor,{0,0}});
        } else if(completionDestination()) {
            const auto& background=completionAssets_.at(completionDestination()->dungeonId);
            const auto& frame=background.package.frames(0,0).front();camera=endingCamera();
            layers.push_back({background.atlas.get(),frame.rect,frame.anchor,{0,0}});
        } else if (explorer_ && dungeonScene(explorer_->state().gameplay)) {
            const auto& state = explorer_->state();
            const auto tileId = state.floors[state.floorIndex].tilesetId;
            const auto& tileAssets = tiles_.at(tileId);
            camera = cameraFor(playerMotion_.sample(now),kWidth*24,kHeight*24,int((size_+scale_-1)/scale_));
            auto visible = [&](Point p,int size){return p.x+size>=camera.x && p.y+size>=camera.y && p.x<camera.x+int(size_/scale_)+1 && p.y<camera.y+int(size_/scale_)+1;};
            const auto& frames=tileAssets.package.frames(0,0);
            if(tileId>=170) {
                const auto& floor=state.expedition.floors.at(state.floorIndex);
                layers.push_back({tileAssets.atlas.get(),frames.front().rect,{0,0},{-floor.backgroundX,-floor.backgroundY}});
            } else {
                if(chunkVersion_!=state.world.version || chunkSeed_!=state.mapSeed){chunks_=chunkGrid(state.world,explorationPackage_->tileset(tileId));chunkVersion_=state.world.version;chunkSeed_=state.mapSeed;}
                layers.push_back({&dungeonBackdrop_,{0,0,200,200},{0,0},camera});
                for(int tile=0;tile<kTiles;++tile){const Point p{tile%kWidth*24,tile/kWidth*24};if(visible(p,24))layers.push_back({tileAssets.atlas.get(),frames[chunks_[tile]].rect,{0,0},p});}
            }
            const Point stairPos=raster(tilePoint(state.world.stairs));
            if(!fixedBattlePending(state) && visible({stairPos.x-12,stairPos.y-12},24))addProp(layers,1,stairPos);
            const auto& event=state.variety.event;
            if(event.objectTile>=0 && !event.carrying && event.id.type==VarietyEvent::LostProperty)addProp(layers,4,raster(tilePoint(event.objectTile)));
            if(event.sourceTile>=0 && event.id.type==VarietyEvent::Reunion)addProp(layers,5,raster(tilePoint(event.sourceTile)));
            if(event.sourceTile>=0 && event.id.type==VarietyEvent::Shop)addProp(layers,6,raster(tilePoint(event.sourceTile)));
            for (const auto& e : state.population.entities) if (e.visible && (!e.done || (e.kind==EntityKind::Friend && std::find(state.gameplay.hostile.begin(),state.gameplay.hostile.end(),e.instanceId)==state.gameplay.hostile.end()))) {
                if (e.kind == EntityKind::Friend) {
                    const auto& g = state.gameplay;
                    const bool active = !partyRendering() && (g.life == LifePhase::Combat || g.life==LifePhase::Dialogue) && e.instanceId == g.enemyInstance;
                    const bool moving=actorMotions_.count(e.instanceId)&&actorMotions_.at(e.instanceId).active(now);
                    const auto& body=npcAssets(e.instanceId);
                    const auto& f = body.package.frames(partyRendering()?partyActorClips_[e.instanceId]:active?enemyClip_:moving?0:7, active?enemyDirection_:e.direction)[partyRendering()?actorFrames_[e.instanceId]:active?enemyFrame_:moving?actorFrames_[e.instanceId]:0];
                    const auto world=actorMotions_.count(e.instanceId)?raster(actorMotions_.at(e.instanceId).sample(now)):raster(tilePoint(e.tile));
                    if(showFriendlyDisc(g,std::find(g.hostile.begin(),g.hostile.end(),e.instanceId)==g.hostile.end(),false)) addDisc(layers,f,world);
                    layers.push_back({body.atlas.get(), f.rect, f.anchor, world});
                    continue;
                }
                const Point p=raster(tilePoint(e.tile));
                if(visible({p.x-8,p.y-8},16)) {
                    const auto& frame=itemAssets_->package.frames(e.originalId,0).front();
                    layers.push_back({itemAssets_->atlas.get(),frame.rect,frame.anchor,p});
                }
            }
        } else if (explorer_) {
            const auto& g = explorer_->state().gameplay;
            const bool bedroom=bedroomScene(g);
            const auto& f = homeAssets_->package.frames(bedroom ? (nightScene(g)?2:1) : 0, 0).front();
            camera=homeCamera(now);
            if(g.sceneBeat==SceneBeat::WakeDelay) camera.x+=((utcMs()-g.phaseStartedMs)/80)%2?2:-2;
            layers.push_back({homeAssets_->atlas.get(), f.rect, f.anchor, {0, 0}});
            for (const auto& cue : guildRooms_[bedroom ? 1 : 0].actors) {
                if (cue.kind == GuildActorKind::Player) {
                    const auto sample = currentPlayback();
                    if (sample.frame) playerLayer=Layer{assets_.atlas.get(), sample.frame->rect, sample.frame->anchor, sample.scenePosition};
                } else {
                    const auto it = std::find_if(guildLive_.begin(), guildLive_.end(), [&](const auto& a) { return a.cue->id == cue.id; });
                    if(it==guildLive_.end()) continue;
                    const auto& frame = it->assets->package.frames(it->clip, it->direction)[it->frame];
                    const auto position=bedroom && cue.id==109?loudredAlarm(cue.position,g,utcMs()).position:bedroom && cue.kind==GuildActorKind::Partner &&
                        (g.sceneBeat==SceneBeat::RoomExit || g.sceneBeat==SceneBeat::MorningFadeOut)?
                        bedroomExit(cue.position,g,utcMs(),true).position:cue.position;
                    layers.push_back({it->assets->atlas.get(), frame.rect, frame.anchor, position});
                    if(cue.effectId==624 && guildAppleVisible(g)) {
                        const auto& apple=appleAssets_->package.frames(624,0)[appleFrame_];
                        const auto head=std::find_if(frame.markers.begin(),frame.markers.end(),[](const auto& m){return m.kind==0;});
                        if(head==frame.markers.end())throw std::runtime_error("Wigglytuff head marker missing");
                        const Point attachment{position.x+head->point.x-frame.anchor.x,position.y+head->point.y-frame.anchor.y};
                        layers.push_back({appleAssets_->atlas.get(),apple.rect,apple.anchor,attachment});
                    }
                }
            }
        }
        if (backgroundOn_ && options_.scenario != L"empty") layers.push_back({&background_, {0, 0, 32, 24}, {0, 0}, {0, 0}});
        const auto playback = currentPlayback();
        if (playback.frame && (!explorer_ || (!explorer_->state().variety.holiday.active && !explorer_->state().gameplay.playerHidden && dungeonScene(explorer_->state().gameplay)))) {
            const auto& f = *playback.frame;
            playerLayer=Layer{assets_.atlas.get(), f.rect, f.anchor, playback.scenePosition};
        }
        if(explorer_ && dungeonScene(explorer_->state().gameplay)) {
            const auto& p=explorer_->state().gameplay.companion;
            if(p.enabled && !p.hidden) {
                const auto& f=companionAssets_->package.frames(companionClip_,companionDirection_)[companionFrame_];
                layers.push_back({companionAssets_->atlas.get(),f.rect,f.anchor,currentExpeditionStop()?expeditionPosition(true):completionDestination()?endingPosition(true):raster(companionMotion_.sample(now))});
            }
        }
        if(playerLayer)layers.push_back(*playerLayer); // player wins any sprite overlap
        const auto previous = surface_.pixels;
        (void)compose(surface_, layers, camera, scale_);
        if (explorer_) gameplayOverlay(surface_, explorer_->state().gameplay, utcMs(),dialoguePanel_?&*dialoguePanel_:nullptr,false,!explorer_->state().variety.holiday.active,selectedDialogue(explorer_->state(),dialogueCatalogue_)!=nullptr || activeGameplayNotice(explorer_->state())!=nullptr);
        const auto speaker=explorer_?currentPortrait():DialoguePortraits{};
        if(explorer_ && portraitAssets_) dialoguePortraitOverlay(surface_,*portraitAssets_->atlas,portraitAssets_->package,speaker,dialogueCast_,*portraitFrame_,npcSpeaker(speaker));
        std::wstring overlayText=explorer_?revealedCatalogueText(explorer_->state(),dialogueCatalogue_,utcMs()):std::wstring{};
        const auto hud=mapHud();
        const auto fade=explorer_?(explorer_->state().variety.holiday.active?holidayFade(explorer_->state(),utcMs()):gameplayFadeOpacity(explorer_->state().gameplay,utcMs())):0;
        const auto titleOpacity=explorer_?explorationTitleOpacity(explorer_->state(),utcMs()):0;
        const bool textChanged=overlayText!=submittedText_ || hud!=submittedHud_ || fade!=submittedFade_ || titleOpacity!=submittedTitleOpacity_;
        bool changed = previous != surface_.pixels;
        dirty_ = false;
        if (!changed && !textChanged && submitted_) {traceMovement(now,false);return;}
        std::memcpy(dib_->bits, surface_.pixels.data(), surface_.pixels.size());
        textOverlay_.draw(dib_->dc,dib_->bits,size_,overlayText,
            explorer_ && explorer_->state().gameplay.life==LifePhase::Death && explorer_->state().gameplay.deathStage==DeathStage::Blackout);
        textOverlay_.drawMapHud(dib_->dc,dib_->bits,size_,hud);
        if(explorer_)textOverlay_.drawMapTitle(dib_->dc,dib_->bits,size_,explorationTitle(explorer_->state()),explorationTitleOpacity(explorer_->state(),utcMs()));
        if(explorer_ && explorer_->state().variety.holiday.active) {
            auto* pixels=static_cast<std::uint8_t*>(dib_->bits);for(std::size_t i=0;i<surface_.pixels.size();i+=4)for(unsigned ch=0;ch<3;++ch)pixels[i+ch]=std::uint8_t((unsigned(pixels[i+ch])*(255-fade)+127)/255);
        } else if(explorer_)applyGameplayFade(static_cast<std::uint8_t*>(dib_->bits),surface_.pixels.size(),explorer_->state().gameplay,utcMs());
        if(explorer_ && explorer_->state().gameplay.life==LifePhase::Death && explorer_->state().gameplay.deathStage==DeathStage::Blackout)
            textOverlay_.draw(dib_->dc,dib_->bits,size_,overlayText,true);
        RECT rect{}; winCheck(GetWindowRect(window_, &rect) != 0, "submit rect");
        POINT position{rect.left, rect.top}, source{0, 0}; SIZE size{LONG(size_), LONG(size_)};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        winCheck(UpdateLayeredWindow(window_, nullptr, &position, &size, dib_->dc, &source, 0, &blend, ULW_ALPHA) != 0, "UpdateLayeredWindow");
        ++submissions_; submitted_ = true;
        if(explorer_ && explorer_->state().screenExploration && !completionScene(explorer_->state().gameplay) && !expeditionScene(explorer_->state())) {
            explorer_->enableScreenExploration((size_+scale_-1)/scale_);
            explorer_->observeViewport(camera.x,camera.y);
        } submittedText_=std::move(overlayText);submittedHud_=hud;submittedFade_=fade;submittedTitleOpacity_=titleOpacity;
        traceMovement(now,true);
        if (blocked()) ++hiddenSubmissions_;
    }
public:
    App(HINSTANCE instance, Options o) : instance_(instance), options_(std::move(o)), directory_(appDir()),
        characterCatalogue_(options_.exploration.empty()?CharacterCatalogue{}:loadCharacterCatalogue(options_.gameplayAssets/L"starters/starters.mdsc")),
        characterProfile_(options_.characterProfile.empty()?directory_/L"characters.state":options_.characterProfile),
        dialogueCast_(options_.exploration.empty()?DialogueCast{}:selectedDialogueCast(loadDialogueCast(options_.gameplayAssets/L"portraits/cast.mddc"),characterCatalogue_,characterProfile_)),
        assets_(Assets::load(options_.exploration.empty() ? options_.package : options_.gameplayAssets / dialogueCast_.actor(PortraitRole::Player).bodyManifest)), surface_(PhysicalSize, PhysicalSize),
        clip_(options_.clip), direction_(options_.direction), scale_(options_.scale), loop_(options_.loop) {
        assets_.package.frames(clip_, direction_);
        if (options_.scenario == L"idle") clip_ = 7;
        if (options_.scenario == L"walk") clip_ = 0;
        assets_.package.frames(clip_, direction_);
        if (options_.metrics.empty()) options_.metrics = directory_ / L"metrics.csv";
        metrics_.open(options_.metrics, std::ios::trunc);
        if (!metrics_) throw std::runtime_error("cannot open metrics file");
        if(!options_.movementTrace.empty()) {
            movementTrace_.open(options_.movementTrace,std::ios::trunc);
            if(!movementTrace_) throw std::runtime_error("cannot open movement trace");
            movementTrace_ << "elapsed_us,action_id,started_ms,duration_ms,from_tile,to_tile,world_x,world_y,scene_x,scene_y,camera_x,camera_y,direction,clip,frame_index,scale,moving,opaque_pixels,surface_hash,submitted,marker_count,life,scene_beat,utc_ms,walk_started_ms,floor,entry_tile,stairs_tile,player_tile,entry_entities,holding_pose,partner_clip,partner_frame,rgb_sum,companion_enabled,companion_tile,companion_world_x,companion_world_y,companion_clip,companion_frame,companion_moving,player_hp,companion_hp,round,player_round,companion_round,enemy_targets_companion,combat,independent_party,party_phase,player_party_action,companion_party_action,actor_states,companion_started_ms,companion_duration_ms,actor_movements,guild_alarm_x,guild_alarm_y,guild_alarm_clip,guild_alarm_direction,room_shown_tiles,room_total_tiles,guild_partner_x,guild_partner_y,guild_partner_direction,guild_apple_visible,guild_apple_frame,player_character,partner_character,speaker,emotion,line_id,fade_opacity,speaker_md,actual_emotion,npc_bindings,npc_cache_bodies,dialogue_bundle,dialogue_line,dialogue_deadline_ms,event_type,event_progress,event_variant,holiday_beat,holiday_scene,holiday_player_x,holiday_player_y,holiday_partner_x,holiday_partner_y,holiday_player_direction,holiday_partner_direction,holiday_clip,bathing,holiday_fade,notice_kind,notice_item,notice_deadline_ms\n";
        }
        events_.open(directory_ / L"events.log", std::ios::app);
        if (!events_) throw std::runtime_error("cannot open event log");
        metrics_ << "elapsed_seconds,cpu_one_core_percent,cpu_all_cores_percent,logical_cores,working_set_bytes,private_bytes,peak_working_set_bytes,render_submissions,decode_count,marker_count,finite_finish_count,skipped_cycles,process_handles,gdi_objects,render_paused,submissions_while_paused,dpi,physical_size,integer_scale,window_x,window_y,switch_count,final_sample," << PlaybackSample::columns << '\n';
        decodes_ = assets_.decoded ? 1 : 0;
        if (!options_.exploration.empty()) {
            companionAssets_ = Assets::load(options_.gameplayAssets / dialogueCast_.actor(PortraitRole::Partner).bodyManifest);
            decodes_+=companionAssets_->decoded?1:0;
            homeAssets_ = Assets::load(options_.gameplayAssets / L"home/home.mdpa");
            propsAssets_=Assets::load(options_.gameplayAssets / L"props/props.mdpa");
            itemAssets_=Assets::load(options_.gameplayAssets / L"items/items.mdpa");
            decodes_+=itemAssets_->decoded?1:0;
            appleAssets_=Assets::load(options_.gameplayAssets / L"effects/apple.mdpa");
            uiAssets_=Assets::load(options_.gameplayAssets / L"ui/ui.mdpa");
            portraitAssets_=Assets::load(options_.gameplayAssets / L"portraits/portraits.mdpa");
            validateDialogueActor(dialogueCast_.actor(PortraitRole::Player),assets_.package,portraitAssets_->package);
            validateDialogueActor(dialogueCast_.actor(PortraitRole::Partner),companionAssets_->package,portraitAssets_->package);
            decodes_+=portraitAssets_->decoded?1:0;
            dialoguePanel_=originalDialoguePanel(*uiAssets_->atlas,uiAssets_->package);
            portraitFrame_=originalWindowPanel(*uiAssets_->atlas,uiAssets_->package,56,56);
            decodes_+=appleAssets_->decoded?1:0;decodes_+=uiAssets_->decoded?1:0;
            decodes_+=propsAssets_->decoded?1:0;
            guildRooms_ = loadGuildScene(options_.gameplayAssets / L"guild/guild.mdgs");
            decodes_ += homeAssets_->decoded ? 1 : 0;
            explorationPackage_ = loadExplorationPackage(options_.exploration);
            explorationRoot_=options_.exploration.parent_path().parent_path();
            npcCatalogue_=loadNpcCatalogue(options_.gameplayAssets/L"npcs/characters.mdnc");
            dialogueCatalogue_=loadDialogueCatalogue(options_.gameplayAssets/L"dialogue/catalogue.mdlc");
            eventRegistry_=loadEventRegistry(options_.gameplayAssets/L"events/contexts.mder");
            holidayRegistry_=loadHolidayRegistry(options_.gameplayAssets/L"holiday");
            if(eventRegistry_.romHash!=npcCatalogue_.romHash)throw std::runtime_error("event map/NPC ROM mismatch");
            if(npcCatalogue_.romHash!=explorationPackage_->romHash)throw std::runtime_error("NPC/exploration ROM mismatch");
            validateNpcCoverage(npcCatalogue_,explorationPackage_->floors);
            if(characterCatalogue_.romHash!=explorationPackage_->romHash)throw std::runtime_error("starter/exploration ROM mismatch");
            completionRegistry_=loadCompletionRegistry(options_.gameplayAssets / L"completion/destinations.mdec");
            if(completionRegistry_.romHash!=explorationPackage_->romHash)throw std::runtime_error("completion/exploration ROM mismatch");
            if(options_.defaultExploration)for(const auto& choice:eventRegistry_.maps) {
                auto package=loadExplorationPackage(explorationRoot_/choice.packageFile);
                if(package.assetVersion!=choice.assetVersion || package.romHash!=eventRegistry_.romHash || (!package.expedition.enabled() && !completionRegistry_.destination(choice.dungeon)))throw std::runtime_error("unverified daily map route");
                validateNpcCoverage(npcCatalogue_,package.floors);
                for(const auto& floor:eventRegistry_.floors)if(floor.dungeon==choice.dungeon)for(const auto& weight:floor.monsterHouse)if(weight.value)(void)npcCatalogue_.select(weight.originalId);
                explorationPool_.emplace(choice.dungeon,std::move(package));
            }
            for(unsigned id:{9U,11U,13U}) {
                const auto path=explorationRoot_/std::filesystem::path("dungeon-"+std::to_string(id))/L"exploration.mdpe";
                if(std::filesystem::exists(path)){auto branch=loadExplorationPackage(path);if(branch.romHash!=eventRegistry_.romHash)throw std::runtime_error("sidepath ROM mismatch");validateNpcCoverage(npcCatalogue_,branch.floors);expeditionBranches_.emplace(id,std::move(branch));}
            }
            for(const auto& destination:completionRegistry_.destinations) {
                auto asset=Assets::load(options_.gameplayAssets / L"completion" / destination.assetFile);
                const auto& rect=asset.package.frames(0,0).front().rect;
                if(rect.w!=int(destination.width) || rect.h!=int(destination.height) || rect.x || rect.y)
                    throw std::runtime_error("completion background dimensions mismatch");
                decodes_+=asset.decoded?1:0;completionAssets_.emplace(destination.dungeonId,std::move(asset));
            }
            auto explorationDirectory=options_.exploration.parent_path();
            if (options_.snapshot.empty()) options_.snapshot = directory_ / L"exploration.mdes";
            const bool restoring = !options_.newRun && std::filesystem::exists(options_.snapshot);
            if (restoring) {
                try {
                    explorer_=std::make_unique<Explorer>(loadSnapshot(options_.snapshot,explorationPackage_->assetVersion));
                } catch(const std::runtime_error& error) {
                    if(!options_.defaultExploration || std::string(error.what())!="snapshot asset version mismatch")throw;
                    bool known=false;
                    for(const auto& entry:explorationPool_)if(entry.second.assetVersion!=explorationPackage_->assetVersion) {
                        try {explorer_=std::make_unique<Explorer>(loadSnapshot(options_.snapshot,entry.second.assetVersion));explorationPackage_=entry.second;
                            explorationDirectory=explorationRoot_/std::filesystem::path("dungeon-"+std::to_string(entry.first));known=true;break;
                        } catch(const std::runtime_error& candidateError){if(std::string(candidateError.what())!="snapshot asset version mismatch")throw;}
                    }
                    if(!known) {
                        for(unsigned id:{15U,18U,0U}) {
                            const auto legacyPath=explorationRoot_/std::filesystem::path("dungeon-"+std::to_string(id))/L"exploration.mdpe";
                            auto legacyPackage=loadExplorationPackage(legacyPath);
                            try {
                                auto legacyState=loadSnapshot(options_.snapshot,legacyPackage.assetVersion);
                                explorer_=std::make_unique<Explorer>(std::move(legacyState));
                                pendingDefaultPackage_=std::move(explorationPackage_);explorationPackage_=std::move(legacyPackage);
                                explorationDirectory=legacyPath.parent_path();
                                explorer_->setNextRunPackage(pendingDefaultPackage_->assetVersion,pendingDefaultPackage_->floors,pendingDefaultPackage_->expedition);
                                known=true;log("legacy exploration preserved; full expedition starts on next scheduled day");break;
                            }catch(const std::runtime_error& candidateError){if(std::string(candidateError.what())!="snapshot asset version mismatch")throw;}
                        }
                        if(!known)throw std::runtime_error("snapshot asset version mismatch");
                    }

                }
            } else {
                explorer_ = std::make_unique<Explorer>(static_cast<uint64_t>(nowUs()), options_.seed,
                            explorationPackage_->assetVersion, explorationPackage_->floors,SpawnPolicy{}, std::vector<uint64_t>{},explorationPackage_->expedition);
                saveRunSnapshot(options_.snapshot, explorer_->state());
            }
            validateNpcCoverage(npcCatalogue_,explorer_->state().floors);
            auto validateItems=[&](const ExplorationPackage& package) {
                for(const auto& floor:package.floors) {
                    for(const auto& category:floor.floorItems)if(category.weight)for(const auto& item:category.items)if(item.value)itemAssets_->package.frames(item.originalId,0);
                    for(auto item:floor.guaranteed)itemAssets_->package.frames(item,0);
                }
            };
            validateItems(*explorationPackage_);for(const auto& entry:explorationPool_)validateItems(entry.second);
            commitNpcAssets(stageNpcAssets(explorer_->state()));
            tiles_=loadExplorationTiles(*explorationPackage_,explorationDirectory);
            for(const auto& entry:tiles_)decodes_+=entry.second.decoded?1:0;
            playerMotion_.reset(tilePoint(explorer_->state().position));
            explorer_->enableGameplay(utcMs());
            if(!restoring && !options_.defaultExploration && explorer_->state().gameplay.life==LifePhase::Explore) {
                auto initial=explorer_->state();initial.gameplay.sceneBeat=SceneBeat::ExploreFadeIn;
                initial.gameplay.phaseStartedMs=utcMs();initial.gameplay.deadlineMs=initial.gameplay.phaseStartedMs+3000;
                explorer_=std::make_unique<Explorer>(std::move(initial));
            }
            if(!options_.solo) explorer_->enableCompanion();
            explorer_->enableHolidaySchedule();
            if(options_.defaultExploration) {
                explorer_->enableScreenExploration((size_+scale_-1)/scale_);
                explorer_->startGuildMorning(utcMs());
                log("startup=guild-morning exploration-goal=discarded");
            }
            explorer_->restoreHolidayClock(utcMs());
            explorer_->restoreDialogue(dialogueCatalogue_,utcMs());
            stageExpeditionGround(explorer_->state());
            if(auto holiday=stageHolidayAssets(explorer_->state())){decodes_+=holiday->decodes;holidayAssets_=std::move(*holiday);}
            if(explorer_->state().gameplay.companion.enabled)companionMotion_.reset(tilePoint(explorer_->state().gameplay.companion.tile));
            restoreMovement(nowUs());
            if(explorer_->state().gameplay.sceneBeat==SceneBeat::DiningPan) guildCamera_.begin(UINT32_MAX,explorer_->state().gameplay.day,{368,212},{168,212},restoredClipStart(nowUs()),6,4000000);
            syncCompanion(nowUs());
            saveRunSnapshot(options_.snapshot, explorer_->state());
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
            if(g.homeScene) direction_=g.sceneBeat==SceneBeat::RoomExit || g.sceneBeat==SceneBeat::MorningFadeOut?
                bedroomExit(guildRooms_[1].player().position,g,utcMs()).direction:guildRooms_[bedroomScene(g)?1:0].player().direction;
            direction_=bedroomDirection(g,direction_);
            if(!companionDialogue(g) && (g.life==LifePhase::Combat || g.life==LifePhase::Dialogue)) {
                const auto enemy=std::find_if(explorer_->state().population.entities.begin(),explorer_->state().population.entities.end(),[&](const auto& e){return e.instanceId==g.enemyInstance;});
                enemyDirection_=enemy->direction;
                enemyClip_=g.life==LifePhase::Dialogue?(g.dialogueStage==DialogueStage::Npc?npcClip(22,npcAssets(g.enemyInstance)):7):g.combat==CombatPhase::EnemyAttack?1:g.combat==CombatPhase::EnemyHurt?6:g.combat==CombatPhase::Faint?npcClip(33,npcAssets(g.enemyInstance)):7;
                const auto restoredNow=nowUs();
                enemyAnimator_.start(npcAssets(g.enemyInstance).package,enemyClip_,enemyDirection_,enemyClip_==7 && !(g.life==LifePhase::Dialogue && g.dialogueStage==DialogueStage::Npc),enemyClip_==7?restoredNow:restoredClipStart(restoredNow));
            }
            auto marker = [](Surface& s, uint8_t blue, uint8_t green, uint8_t red) {
                for (size_t offset = 0; offset < s.pixels.size(); offset += 4) {
                    s.pixels[offset] = blue; s.pixels[offset+1] = green; s.pixels[offset+2] = red; s.pixels[offset+3] = 255;
                }
            };
            marker(friendMarker_, 230, 150, 70); marker(stairsMarker_, 220, 220, 220);
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
        if(explorer_) {const auto deadline=nextGameplayDeadline(explorer_->state());exploreDeadline_=deadlineFromUtc(nowUs(),utcMs(),deadline);}
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
                if(explorer_->state().variety.holiday.active)deadline=std::min(deadline,now+16667);
                const auto& g=explorer_->state().gameplay;
                // A finite NPC idle can outlast the text's minimum duration.
                // Wait for its animation event instead of spinning on a past text deadline.
                if(poseFinished_ || exploreDeadline_>now || g.life!=LifePhase::Dialogue)
                    deadline=std::min(deadline,poseFinished_?now:exploreDeadline_);
                if(g.life==LifePhase::ReturnHome || (g.life==LifePhase::Death && g.deathStage==DeathStage::FadeOut) ||
                   g.sceneBeat==SceneBeat::DiningExit || g.sceneBeat==SceneBeat::BedFadeOut || g.sceneBeat==SceneBeat::BedFadeIn ||
                   g.sceneBeat==SceneBeat::NightFadeIn || g.sceneBeat==SceneBeat::MorningFadeOut || g.sceneBeat==SceneBeat::ExploreFadeIn ||
                   g.life==LifePhase::Eat || g.sceneBeat==SceneBeat::AlarmEnter || g.sceneBeat==SceneBeat::AlarmLeave ||
                   completionScene(g) || expeditionScene(explorer_->state()) || g.sceneBeat==SceneBeat::FloorFadeOut || g.sceneBeat==SceneBeat::FloorFadeIn || g.sceneBeat==SceneBeat::RoomExit) deadline=std::min(deadline,now+16667);
                if(auto cameraDeadline=guildCamera_.nextDeadline(now)) deadline=std::min(deadline,*cameraDeadline);
                if(auto d=companionMotion_.nextDeadline(now)) deadline=std::min(deadline,*d);
                if(auto motionDeadline=playerMotion_.nextDeadline(now)) deadline=std::min(deadline,*motionDeadline);
                for(const auto& entry:actorMotions_) if(auto d=entry.second.nextDeadline(now)) deadline=std::min(deadline,*d);
            }
            if(!blocked() && explorer_ && revealedCatalogueText(explorer_->state(),dialogueCatalogue_,utcMs())!=catalogueText(explorer_->state(),dialogueCatalogue_)) deadline=std::min(deadline,now+25000);
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
        // App destruction closes its stream before this handler runs. Preserve
        // the fatal reason alongside normal events for zero-argument launches.
        try {
            std::ofstream events(appDir() / L"events.log", std::ios::app);
            events << nowUs() << " fatal " << e.what() << '\n';
        } catch (...) { /* Keep the original startup/runtime failure. */ }
        if (!report.empty()) { std::ofstream output(report); output << "status=fail\nerror=" << e.what() << '\n'; }
        OutputDebugStringW((wide(e.what()) + L"\n").c_str());
        if (!validation) MessageBoxW(nullptr, wide(e.what()).c_str(), L"MysteryDungeonPet A-3", MB_OK | MB_ICONERROR);
        return 1;
    }
}
