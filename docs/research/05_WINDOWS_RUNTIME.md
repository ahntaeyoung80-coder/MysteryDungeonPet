# Phase 5 — Windows 초경량 런타임

작성 기준: 2026-10-13 KST. 최종 기본안은 C++17 + Win32 + WIC PNG decoder + premultiplied BGRA DIB + UpdateLayeredWindow이다. 이 환경에서는 Windows 창 생성·네이티브 실행·CPU/RAM 측정을 할 수 없었다. 아래 수치는 계산 또는 계획 추정이며 성능 목표 달성의 증거가 아니다.

## 1. 기술 후보와 실제 재사용 코드

| 후보 | 실제 코드 | 배포·의존성 | 예상 비용·개발 난도 | 결정 |
| --- | --- | --- | --- | --- |
| C++/Win32/WIC/GDI | [Samples/Win7Samples/multimedia/wic/wicviewergdi/WicViewerGdi.cpp](https://github.com/microsoft/Windows-classic-samples/blob/434f6002bdf9cf9829406c3ff2b33387982d6168/Samples/Win7Samples/multimedia/wic/wicviewergdi/WicViewerGdi.cpp), [Samples/Win7Samples/multimedia/wic/wicviewergdi/WicViewerGdi.h](https://github.com/microsoft/Windows-classic-samples/blob/434f6002bdf9cf9829406c3ff2b33387982d6168/Samples/Win7Samples/multimedia/wic/wicviewergdi/WicViewerGdi.h) / DemoApp | OS DLL과 네이티브 exe; MSVC 정적 CRT 가능 | exe 0.2~2MB, RAM 8~25MB는 계획 추정. 100~200px CPU 합성·명시적 자원 관리 필요 | 채택 |
| C++/Direct2D | [Samples/Win7Samples/multimedia/wic/wicviewerd2d/WicViewerD2D.cpp](https://github.com/microsoft/Windows-classic-samples/blob/434f6002bdf9cf9829406c3ff2b33387982d6168/Samples/Win7Samples/multimedia/wic/wicviewerd2d/WicViewerD2D.cpp) / CreateD2DBitmapFromFile,CreateDeviceResources | OS D2D/DXGI/D3D 계열 | device/texture 비용 미측정. 많은 효과·고빈도 렌더에 유리할 수 있으나 작은 펫에는 복잡도 증가 | 필요 시 대안 |
| Rust/Win32 | [crates/samples/windows/window-message/src/main.rs](https://github.com/microsoft/windows-rs/blob/d5b6a1f68f4fdd0c79495954d162901ea90da4f4/crates/samples/windows/window-message/src/main.rs), [crates/samples/windows/window-message/Cargo.toml](https://github.com/microsoft/windows-rs/blob/d5b6a1f68f4fdd0c79495954d162901ea90da4f4/crates/samples/windows/window-message/Cargo.toml) | windows/windows-window; 소스별 feature 선택, 외부 GUI runtime 없음 | exe0.5~4MB는 계획 추정, RAM/CPU 미측정. 안전한 소유권의 장점과 버전/unsafe 경계 관리 | 팀이 Rust 선호 시 대안 |
| Rust/Direct2D | [crates/samples/windows/direct2d/src/main.rs](https://github.com/microsoft/windows-rs/blob/d5b6a1f68f4fdd0c79495954d162901ea90da4f4/crates/samples/windows/direct2d/src/main.rs) | D2D1CreateFactory,D3D11CreateDevice,device resources/present | device복구·투명창 통합 비용; 실측 없음 | 기본안 미채택 |
| Python/GTK/웹 런타임 | 기존 오프라인 도구 | 별도 interpreter/UI runtime과 패키지 | 20MB/30MB 목표에 불리한 의존성; 이 작업에서 최소 구현·실측 안 함 | 오프라인에만 사용 |

Windows-classic-samples는 MIT 기반 샘플이다. windows-rs는 MIT 또는 Apache-2.0이다. 실제 코드 이식 시 해당 copyright/license를 포함한다. 샘플 전체를 새 앱에 복사하지 말고 디코더·DIB 생성 흐름과 Win32 API 경계를 재사용한다.

이번 작성 시 WicViewerGdi.cpp의 실제 함수와 변환 형식을 다시 확인했다. 샘플은 GUID_WICPixelFormat32bppBGR이며 투명 펫용으로 그대로 사용하면 alpha가 보존되지 않는다. Rust 샘플의 현재 고정 커밋은 edition2024, windows feature ['winuser'], windows-window workspace 의존을 쓴다. 과거 windows-rs의 다른 feature 이름을 이 커밋에 그대로 적용하지 않는다.

## 2. WIC 샘플에서 가져올 것과 수정할 것

| 실제 함수 | 입력 → 출력 / 현재 흐름 | 수정·연결 | 성능·확인 수준 |
| --- | --- | --- | --- |
| DemoApp::Initialize(HINSTANCE) | 클래스/창/COM 초기화 → HRESULT | 펫용 popup/window host로 축소, 초기화·해제 순서 유지 | 본문 확인; Windows 실행 미실행 |
| DemoApp::CreateDIBFromFile(HWND) | 파일 선택·decoder 경로 → DIB | LocateImageFile/dialog 제거, AssetStore의 승인된 asset path를 decoder에 전달 | 동작 전환마다 디스크 재디코드하지 않음 |
| DemoApp::ConvertBitmapSource(HWND,IWICBitmapSource**) | bitmap source → scale/format 변환 | WICBitmapInterpolationModeFant 대신 정수 배율/NearestNeighbor, 목표 형식을 32bppPBGRA로 변경 | 픽셀 블러 방지; CPU 변환은 최초 로드/캐시 miss만 |
| DemoApp::CreateDIBSectionFromBitmapSource(IWICBitmapSource*) | width/height/CopyPixels → top-down DIB | 샘플의 32bppBGR format 검사도 PBGRA로 함께 변경; stride/buffer UIntMult 등 overflow 검사 유지 | DIB 재사용, width*height*4 경계 확인 |
| DemoApp::WndProc/OnPaint | message → 불투명 샘플 repaint | Layered Renderer는 UpdateLayeredWindow 경로로 교체. 스냅샷 surface에 sprites/background/effects 합성 | WM_PAINT마다 decoder 생성 금지 |

WIC 호출 연결: CoInitializeEx → IWICImagingFactory → CreateDecoderFromFilename → GetFrame(0) → CreateFormatConverter/Initialize(32bppPBGRA) → CopyPixels → DIB. PNG는 기본 정적 atlas로 사용하고 XML tick으로 애니메이션을 재생한다. 파일·COM·HBITMAP·HDC는 오류 경로에서도 닫는다.

BGRA는 premultiplied alpha여야 한다. A가0인 픽셀의 RGB도0이며 중간 alpha 픽셀은 색 채널×A/255 결과를 쓴다. 단순 RGBA→BGRA 채널 교환만으로 끝내지 않는다. WIC PBGRA 출력이면 다시 premultiply하지 않는다.

## 3. 창·투명·상단·드래그·DPI

| 기능 | Windows API·입출력 | 사용·연결 규칙 | 의존성·검증 |
| --- | --- | --- | --- |
| 100~200px popup | CreateWindowExW(exStyle,...,WS_POPUP,...) → HWND | WS_EX_LAYERED + WS_EX_TOOLWINDOW 사용. 클래스 CS_OWNDC/CS_CLASSDC와 충돌 여부 주의 | User32; 문서/소스 확인 |
| per-pixel alpha | UpdateLayeredWindow(hwnd,...,sourceDC,...,blend,ULW_ALPHA) → BOOL | 32bpp PBGRA DIB, BLENDFUNCTION=AC_SRC_OVER/SourceConstantAlpha255/AC_SRC_ALPHA. 같은 surface가 유지되면 재제출 생략 | User32/Gdi32; 실제 투명 화면 미검증 |
| Always on Top | SetWindowPos(HWND_TOPMOST,...,SWP_NOACTIVATE) → BOOL | 초기 표시도 focus를 가져오지 않게 구성. 사용자 drag 시 활성화 정책은 별도 테스트 | User32 |
| 드래그 | WM_LBUTTONDOWN/MOUSEMOVE/UP,SetCapture,ReleaseCapture | 비투명 영역에서 capture, 화면 좌표로 이동 후 release. 창 위치 저장은 종료/드래그 완료 시 debounce | NOACTIVATE와 capture 조합은 실제 Windows에서 확인 필요 |
| alpha hit test | WM_NCHITTEST 및 layered alpha 동작 | alpha0 영역의 통과와 클릭 가능한 몸체 검증. WS_EX_TRANSPARENT를 전체 창에 주면 드래그가 충돌할 수 있음 | User32; 클릭/통과 수동 테스트 필요 |
| DPI | PMv2 manifest,WM_DPICHANGED | monitor DPI와 추천 RECT 처리. 논리 크기와 정수 픽셀 배율 구분, monitor 변경 후 화면 안에 clamp | Windows10 1703+/11 API baseline; OS 지원 정책은 별도 |

UpdateLayeredWindow는 전체 surface를 갱신하므로 창은 작은 크기를 유지한다. SetLayeredWindowAttributes를 함께 호출한 뒤 UpdateLayeredWindow를 계속 쓰면 WS_EX_LAYERED를 reset하기 전 실패할 수 있다. 기본안은 한 alpha 경로로 통일한다.

공식 근거: [UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow), [BLENDFUNCTION](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-blendfunction), [SetLayeredWindowAttributes](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes), [Layered windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features), [DPI](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows).

## 4. 활성 프로그램·입력 유휴 감지

| 기능 | API·입출력 | 호출 방식·오류 처리 | 비용·연결 |
| --- | --- | --- | --- |
| foreground 변경 | SetWinEventHook(EVENT_SYSTEM_FOREGROUND,...,WINEVENT_OUTOFCONTEXT + WINEVENT_SKIPOWNPROCESS) → hook | UI thread message loop 유지. callback에서 snapshot/event queue로 전달하고 긴 처리 피함; reentrancy 고려 | 지속적인 모든 프로세스 polling 대신 사건 기반 |
| 현재 창·PID | GetForegroundWindow → HWND; GetWindowThreadProcessId → PID | NULL/창 파괴 race 허용. 핸들 재사용을 최신 사건과 대조 | 필요 시 초기 snapshot |
| 실행 파일 경로 | OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION),QueryFullProcessImageNameW | 권한 거부·보호 프로세스는 Unknown, CloseHandle 필수 | PID 변경 때만 조회. UI 입력 내용은 읽지 않음 |
| 키보드·마우스 idle | GetLastInputInfo(LASTINPUTINFO*) → BOOL/dwTime | cbSize 초기화. DWORD(GetTickCount()-dwTime)로 일반 rollover 처리; 세션 한정, tick이 항상 증가하지 않는 API 특성 고려 | 1~5초 주기 확인 제안; 상태 threshold 변경 때만 생활 사건 발생 |

idle은 해당 세션의 마지막 입력 시각이며 전체 세션의 사용량을 나타내지 않는다. dwTime은 SendInput 등으로 비단조일 수 있으므로 불가능한 값은 Unknown/보수적 최근 입력으로 처리하고 잠금/전원 상태와 함께 판정한다. input content·키 로그가 필요하지 않다.

공식 근거: [SetWinEventHook](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwineventhook), [GetLastInputInfo](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlastinputinfo), [QueryFullProcessImageNameW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamew).

## 5. 잠금·절전·디스플레이와 스케줄링

| 기능 | API·메시지 | 처리·연결 | 검증 수준 |
| --- | --- | --- | --- |
| 세션 잠금/해제 | WTSRegisterSessionNotification(hwnd,NOTIFY_FOR_THIS_SESSION);WM_WTSSESSION_CHANGE | WTS_SESSION_LOCK/UNLOCK을 활동 상태에 반영, 종료 시 unregister | 문서 확인. RPC_S_INVALID_BINDING은 서비스 준비 전 오류이므로 제한 retry 필요 |
| suspend/resume | WM_POWERBROADCAST/PBT_APMSUSPEND,PBT_APMRESUMEAUTOMATIC,PBT_APMRESUMESUSPEND | suspend 시 snapshot·렌더 중지. automatic resume는 사용자 복귀의 증거가 아님 | 실제 절전·복귀 테스트 미실행 |
| display off/on/dim | RegisterPowerSettingNotification(hwnd,GUID_SESSION_DISPLAY_STATUS,DEVICE_NOTIFY_WINDOW_HANDLE) → HPOWERNOTIFY | WM_POWERBROADCAST/PBT_POWERSETTINGCHANGE;값0/1/2를 Off/On/Dim으로 처리. session GUID 사용, 종료 시 해제 | winuser.h/User32.lib API; 서비스용 console 상태와 구분 |
| next deadline | CreateWaitableTimerExW/SetWaitableTimerEx → HANDLE | 다음 clip frame/life deadline/idle check만 one-shot 예약. 고해상도 timer와 timeBeginPeriod를 기본 사용하지 않음 | Kernel32, 현재 실행 미검증 |
| message+timer wait | MsgWaitForMultipleObjectsEx(...,QS_ALLINPUT,MWMO_INPUTAVAILABLE) | message drain 후 다음 deadline. spinning이나 1ms sleep loop 금지 | User32; hidden/lock/display off 때 렌더 deadline 취소 |

렌더 변화가 없는 Idle/Sleep 프레임은 제출하지 않는다. 움직임 보간이 필요할 때만 낮은 update rate를 허용하고 실제 CPU 결과로 조정한다. 재생 clock은 monotonic(QPC 등), 생활 수면 deadline은 UTC 기반으로 분리한다. 시스템 suspend 동안 과거 프레임을 모두 재생하지 않는다.

공식 근거: [WTS session notification](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsregistersessionnotification), [RegisterPowerSettingNotification](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerpowersettingnotification), [Power GUIDs](https://learn.microsoft.com/en-us/windows/win32/power/power-setting-guids), [WM_POWERBROADCAST](https://learn.microsoft.com/en-us/windows/win32/power/wm-powerbroadcast), [MsgWaitForMultipleObjectsEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex).

## 6. 저장·복원과 배포 구조

SHGetKnownFolderPath(FOLDERID_LocalAppData)로 사용자 쓰기 가능한 앱 디렉터리를 얻는다. state는 schemaVersion,original ROM IDs/asset pack version,dungeon/localFloor,map seed,RNG state,inventory instance IDs,lifecycle/event phase,onceKeys,savedUTC,window monitor/position을 저장한다. 포인터·HDC/COM·메모리 주소는 저장하지 않는다.

같은 디렉터리에 임시 파일을 CreateFileW/WriteFile/FlushFileBuffers로 쓴 뒤 MoveFileExW로 교체한다. 교체 오류·손상·구버전은 명시적으로 처리하고 마지막 정상 snapshot을 복구한다. 원자성·전원 손실 내구성은 파일 시스템과 flush 결과까지 Windows에서 검사해야 한다. 드래그 중 매 mousemove에 저장하지 않는다.

재시작 시 자산 hash와 state version을 확인하고 화면 밖 좌표를 현재 monitor work area 안으로 이동한다. Sleep deadline을 경과했어도 시스템이 잠겨 있으면 렌더/탐험을 시작하지 않는다. 음식 소비·획득·클리어가 두 번 실행되지 않도록 onceKey를 snapshot과 함께 저장한다.

배포는 exe + selected data/assets + credits/licenses를 기본으로 한다. /MT 정적 CRT로 VC 재배포 설치 의존을 줄일 수 있으나 실제 exe 크기는 빌드 후 확인한다. inbox Windows DLL을 설치 패키지에 복사하지 않는다. 전체 원작 ROM이나 전체 Python 환경을 설치물에 넣지 않는다.

## 7. 예산 계산과 실측 계획

200×200×4 DIB 1장은160,000bytes, 2장은320,000bytes≈0.305MiB라는 계산값이다. 작은 surface가 전체 프로세스 RAM30MB를 보장하지는 않는다. 1024×1024 RGBA atlas1개는4MiB이며 sprite cache·WIC 임시 buffer·OS DLL·GDI object를 함께 측정해야 한다. decode cache8MB는 초기 설계 예산이고 실측 뒤 조정한다.

| 항목 | 목표 / 평가 방법 | 현재 결과 |
| --- | --- | --- |
| 설치 크기 | exe+배포 DLL+선택 assets+데이터+credits ≤20,000,000bytes | 미측정; 전 종/폼/생활 자산 포함 가능 여부 미확인 |
| RAM | working set와 private bytes 모두 별도 기록, 목표30,000,000bytes | 미측정; 계획 추정만 존재 |
| 평균 CPU | 한 코어 기준 및 Task Manager 정규화 기준 모두 보고 | 미측정; 평균1% 달성 주장 불가 |
| PNG load | startup/최초 동작/캐시 miss 시간·peak memory | 미측정 |
| 비활성 상태 | lock/display off에서 render submit0, wakeup 최소화 | 정책 선정; 실행 미검증 |
| GDI/COM leak | 반복 asset 전환100회,drag/DPI/lock cycle 전후 object/handle | 미실행 |

GetProcessTimes의 kernel+user FILETIME 차이는100ns 단위다. 한 코어 CPU%=100×Δ(kernel+user)/(wallSeconds×10^7), Task Manager 스타일 전체 논리 코어 정규화 값은 이를 logicalCoreCount로 나눈다. 단위를 혼용해1%를 통과시키지 않는다.

K32GetProcessMemoryInfo/PROCESS_MEMORY_COUNTERS_EX로 WorkingSetSize,PeakWorkingSetSize,PrivateUsage를 기록한다. PSAPI_VERSION=2는 Kernel32 경로, 구형 래퍼는 Psapi.lib 차이가 있으므로 빌드 기준을 명시한다. [GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes), [GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo).

권장 최소 실행 harness는 runtime_smoke.cpp이며 아직 존재하지 않는 검증 코드다. WIC decode→PBGRA DIB→layered200px 창,frame deadline timer,drag/DPI,power/session event,state write,process metrics를 포함하도록 후속 구현한다. C++17/MSVC/Windows SDK를 사용하고 User32,Gdi32,Ole32,Windowscodecs,Wtsapi32,Shell32를 링크한다. GUID 정의 방식에 따라 Uuid.lib 또는 한 translation unit의 INITGUID가 추가로 필요할 수 있다.

측정 절차: warm-up1분을 제외하고 탐험5분,Idle3분,식사/수면/기상3분,drag/DPI2분,lock/display off2분의15분 시나리오를 수행한다. hardware,OS build,logical cores,DPI,assets 수/bytes,release compiler options,샘플 간격을 남기고 평균/P95/최대 CPU와 메모리 peak를 보고한다. 실제 Win+L와 절전을 검사하고 단순히 메시지를 주입한 테스트를 실제 잠금·전원 검증이라고 부르지 않는다.

## 8. 후속 Codex 적용 범위

가져올 코드: WIC decoder·format converter·overflow-safe DIB 생성 흐름. 새로 구현할 코드: WindowHost,LayeredRenderer,Animator deadline,ActivityMonitor,versioned StateStore. 오프라인 추출 출력과 runtime schema는 Phase 6의 계약을 사용한다.

Windows에서 최소 harness를 실행하기 전까지 런타임 기술은 소스·공식 API 조건에 근거한 채택안이다. 설치/CPU/RAM 목표 달성은 검증 대기이며, 추가 조사 없이 구현 명세에 이 성능 검증 gate를 넣을 수 있도록 위 절차를 고정한다.
