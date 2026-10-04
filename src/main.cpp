#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include "audio.hpp"
#include "game.hpp"
#include "render.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

using namespace anemoi;
namespace {
std::wstring executableDirectory() {
    std::array<wchar_t,32768> buffer{};
    const DWORD n=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(std::wstring(buffer.data(),n)).parent_path().wstring();
}
bool readSeasonFile(const std::wstring& path,Season& season) {
    std::error_code ec;const auto size=std::filesystem::file_size(path,ec);if(ec||size>SeasonSaveLimit)return false;
    std::ifstream input(std::filesystem::path(path),std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    return input.good()&&season.deserialize(data);
}
struct App {
    Race race;
    Season season;
    View view;
    Canvas canvas;
    Audio audio;
    HWND window=nullptr;
    bool running=true,smoke=false,jumpPending=false,skillPending=false,sprintPending=false;
    bool resultStored=false,hasFocus=true;
    std::array<bool,256> keys{};
    std::mt19937 startRandom{std::random_device{}()};
    std::array<std::array<std::array<float,5>,2>,CourseCount> best{};
    std::wstring directory=executableDirectory(),profile=directory+L"\\data\\profile.ini";

    App() {for(auto& course:best)for(auto& row:course)row.fill(-1);view.season=&season;}
    std::wstring keyName(int i,int d) const {return L"best_"+std::to_wstring(d)+L"_"+std::to_wstring(i);}
    std::wstring recordSection(int track) const {return track==0?L"records_v2":L"records_v3_map"+std::to_wstring(track);}
    std::wstring seasonPath() const {return directory+L"\\data\\season.dat";}
    void clearInput(){keys.fill(false);view.mouseDown=false;jumpPending=false;skillPending=false;sprintPending=false;}
    bool locked() const {return view.seasonMode&&season.active&&!season.selecting&&!season.finished();}
    void selectMode(bool seasonal) {
        view.seasonMode=seasonal;
        if(locked()){race.selected=season.character;race.difficulty=season.difficulty;}
    }
    void load() {
        race.selected=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"preferences",L"character",0,profile.c_str())),0,4);
        race.difficulty=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"preferences",L"difficulty",0,profile.c_str())),0,1);
        view.music=GetPrivateProfileIntW(L"preferences",L"music",1,profile.c_str())!=0;
        view.selectedCourse=std::clamp(static_cast<int>(GetPrivateProfileIntW(L"preferences",L"course",0,profile.c_str())),0,CourseCount-1);
        view.seasonMode=GetPrivateProfileIntW(L"preferences",L"season_mode",1,profile.c_str())!=0;
        for(int track=0;track<CourseCount;track++)for(int d=0;d<2;d++)for(int i=0;i<5;i++) {
            wchar_t text[64]{};GetPrivateProfileStringW(recordSection(track).c_str(),keyName(i,d).c_str(),L"-1",text,64,profile.c_str());
            const float val=static_cast<float>(std::wcstod(text,nullptr));
            if(std::isfinite(val)&&val>0&&val<3600)best[track][d][i]=val;
        }
        std::error_code ec;
        if(std::filesystem::exists(seasonPath(),ec)&&!readSeasonFile(seasonPath(),season)) {
            if(readSeasonFile(seasonPath()+L".bak",season))view.storageNotice=L"已从上次备份恢复赛季进度。";
            else view.storageNotice=L"赛季存档无法读取，原文件已保留；可重新开始赛季。";
        }
        selectMode(view.seasonMode);
    }
    bool savePreferences() {
        if(smoke)return true;
        std::error_code ec;std::filesystem::create_directories(directory+L"\\data",ec);if(ec)return false;
        bool ok=true;
        auto put=[&](const wchar_t* name,const std::wstring& value){ok=(WritePrivateProfileStringW(L"preferences",name,value.c_str(),profile.c_str())!=FALSE)&&ok;};
        put(L"character",std::to_wstring(race.selected));put(L"difficulty",std::to_wstring(race.difficulty));
        put(L"music",view.music?L"1":L"0");put(L"course",std::to_wstring(view.selectedCourse));put(L"season_mode",view.seasonMode?L"1":L"0");
        return ok;
    }
    bool saveSeason() {
        if(smoke){view.seasonDirty=false;return true;}
        std::error_code ec;std::filesystem::create_directories(directory+L"\\data",ec);
        bool ok=!ec;const auto path=seasonPath(),temporary=path+L".tmp";
        if(ok) {
            std::ofstream out(std::filesystem::path(temporary),std::ios::binary|std::ios::trunc);
            out<<season.serialize();out.flush();ok=out.good();out.close();ok=ok&&!out.fail();
        }
        if(ok&&std::filesystem::exists(path,ec)) {
            Season previous;const bool valid=readSeasonFile(path,previous);
            ok=CopyFileW(path.c_str(),(path+(valid?L".bak":L".corrupt")).c_str(),FALSE)!=FALSE;
        }
        if(ok)ok=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
        view.seasonDirty=!ok;view.saved=ok;
        view.storageNotice=ok?L"赛季进度已保存 · 下次可从当前环节继续":L"赛季尚未保存：请检查 data 目录；按 Enter 重试。";
        return ok;
    }
    void hub() {
        if(season.selecting){view.seasonMode=true;toMenu();return;}
        clearInput();view.seasonMode=true;race.paused=false;
        race.selected=season.character;race.difficulty=season.difficulty;race.course=season.round();
        race.phase=season.finished()?Phase::SeasonFinal:Phase::SeasonHub;
    }
    void toMenu(){race.returnToMenu();clearInput();selectMode(view.seasonMode);}
    void openHistory() {
        if(race.phase!=Phase::Menu||!view.seasonMode)return;
        clearInput();view.historyPage=0;race.paused=false;race.phase=Phase::SeasonHistory;
    }
    void historyPage(int direction) {
        view.historyPage=std::clamp(view.historyPage+direction,0,std::max(0,season.pastSeasonCount()-1));
    }
    void requestNewSeason() {
        if(race.phase!=Phase::Menu||!view.seasonMode||!season.active||season.selecting)return;
        view.confirmNewSeason=true;clearInput();
    }
    void confirmNewSeason() {
        clearInput();
        if(!view.confirmNewSeason)return;
        if(view.seasonDirty&&!saveSeason())return;
        const Season previous=season;
        if(!season.prepareNext())return;
        if(!saveSeason()) {
            season=previous;view.seasonDirty=false;
            view.storageNotice=L"新赛季未能保存，原赛季已保留；请稍后重试。";
            return;
        }
        view.confirmNewSeason=false;view.seasonMode=true;
        race.selected=season.character;race.difficulty=season.difficulty;toMenu();
    }
    void start() {
        if(race.phase==Phase::Menu&&view.seasonMode) {
            // Never replace a completed championship that has not reached disk.
            if(view.seasonDirty&&!saveSeason()){hub();return;}
            if(season.finished()){requestNewSeason();return;}
            if(season.selecting) {
                const Season previous=season;
                if(!season.confirmSelection(race.selected,race.difficulty))return;
                if(!saveSeason()) {
                    season=previous;view.seasonDirty=false;
                    view.storageNotice=L"选角未能保存，请稍后重试。";return;
                }
            } else if(!season.active) {
                if(!season.begin(race.selected,race.difficulty))return;
                view.seasonDirty=true;saveSeason();
            }
            hub();return;
        }
        if(race.phase==Phase::SeasonFinal){toMenu();return;}
        const bool seasonal=race.phase==Phase::SeasonHub||(race.session!=Session::Single&&race.phase!=Phase::Menu);
        if(seasonal) {
            if(view.seasonDirty&&!saveSeason())return;
            if(!season.startRace(race,startRandom())){hub();return;}
        } else race.start(race.selected,race.difficulty,20261003,view.selectedCourse);
        view.newRecord=false;resultStored=false;clearInput();view.saved=savePreferences();
    }
    void afterResult(){if(race.session==Session::Single)start();else{hub();if(!season.finished())start();}}
    void mute(){view.music=!view.music;audio.setPlaying(view.music&&!smoke&&hasFocus&&!race.paused);savePreferences();}
    void keyDown(int k) {
        if(view.confirmNewSeason) {
            if(k==VK_ESCAPE){view.confirmNewSeason=false;clearInput();}
            if(k==VK_RETURN)confirmNewSeason();
            return;
        }
        if(k=='M'){mute();return;}
        if(race.phase==Phase::SeasonHistory) {
            if(k==VK_ESCAPE||k==VK_RETURN)toMenu();
            else if(k==VK_LEFT)historyPage(-1);
            else if(k==VK_RIGHT)historyPage(1);
            return;
        }
        if(race.phase==Phase::Menu) {
            if(k=='N'){if(locked())requestNewSeason();return;}
            if(k=='T')selectMode(!view.seasonMode);
            if(!locked()) {
                if(k>='1'&&k<='5')race.selected=k-'1';
                if(k==VK_LEFT||k=='A')race.selected=(race.selected+4)%5;
                if(k==VK_RIGHT||k=='D')race.selected=(race.selected+1)%5;
                if(k==VK_TAB)race.difficulty=1-race.difficulty;
            }
            if(!view.seasonMode&&(k=='Q'||k=='E'))view.selectedCourse=(view.selectedCourse+(k=='Q'?CourseCount-1:1))%CourseCount;
            if(k==VK_RETURN||k==VK_SPACE)start();return;
        }
        if(race.phase==Phase::SeasonHub||race.phase==Phase::SeasonFinal) {
            if(k==VK_RETURN||k==VK_SPACE)start();
            if(k==VK_ESCAPE)toMenu();return;
        }
        if(race.phase==Phase::Results) {
            if(k==VK_RETURN||k=='R')afterResult();
            if(k==VK_ESCAPE){if(race.session==Session::Single)toMenu();else hub();}return;
        }
        if(k=='P'||k==VK_ESCAPE) {
            if(k==VK_ESCAPE&&race.paused){if(race.session==Session::Single)toMenu();else hub();}else race.togglePause();
            clearInput();return;
        }
        if(k=='R'){start();return;}
        if(!race.paused) {
            if(k==VK_SHIFT)sprintPending=true;
            if(k==VK_SPACE||k==VK_UP||k=='W')jumpPending=true;
            if(k=='E')skillPending=true;
        }
    }
    void mousePress() {
        const int x=view.mouseX,y=view.mouseY;
        if(view.confirmNewSeason) {
            if(ui::CancelSeason.contains(x,y)){view.confirmNewSeason=false;clearInput();}
            else if(ui::ConfirmSeason.contains(x,y))confirmNewSeason();
            return;
        }
        if(race.phase==Phase::Menu) {
            if(view.seasonMode&&ui::History.contains(x,y)){openHistory();return;}
            if(ui::SeasonMode.contains(x,y))selectMode(true);
            if(ui::SingleMode.contains(x,y))selectMode(false);
            if(!locked()) {
                for(int i=0;i<5;i++)if(ui::character(i).contains(x,y))race.selected=i;
                if(ui::Difficulty.contains(x,y))race.difficulty=1-race.difficulty;
            }
            if(!view.seasonMode) {
                if(ui::CoursePrev.contains(x,y))view.selectedCourse=(view.selectedCourse+CourseCount-1)%CourseCount;
                if(ui::CourseNext.contains(x,y))view.selectedCourse=(view.selectedCourse+1)%CourseCount;
            }
            if(locked()&&ui::NewSeason.contains(x,y))requestNewSeason();
            if(ui::Start.contains(x,y))start();
        }else if(race.phase==Phase::SeasonHistory) {
            if(ui::HubBack.contains(x,y))toMenu();
            else if(ui::HistoryPrev.contains(x,y))historyPage(-1);
            else if(ui::HistoryNext.contains(x,y))historyPage(1);
        }else if(race.phase==Phase::SeasonHub||race.phase==Phase::SeasonFinal) {
            if(ui::HubBack.contains(x,y))toMenu();
            if(ui::HubStart.contains(x,y))start();
        }else if(race.paused) {
            if(ui::Resume.contains(x,y)){race.togglePause();clearInput();}
            if(ui::Back.contains(x,y)){if(race.session==Session::Single)toMenu();else hub();}
        }else if(race.phase==Phase::Results) {
            if(ui::Again.contains(x,y))afterResult();
            if(ui::ResultMenu.contains(x,y)){if(race.session==Session::Single)toMenu();else hub();}
        }else {
            if(ui::Pause.contains(x,y))race.togglePause();
            if(ui::Sound.contains(x,y))mute();
            if(ui::Jump.contains(x,y))jumpPending=true;
            if(ui::Sprint.contains(x,y))sprintPending=true;
            if(ui::SkillButton.contains(x,y))skillPending=true;
        }
    }
    void update(float dt) {
        view.time+=dt;
        Input input{jumpPending,sprintPending||keys[VK_SHIFT]||(view.mouseDown&&ui::Sprint.contains(view.mouseX,view.mouseY)),skillPending};
        race.tick(dt,input);jumpPending=false;skillPending=false;sprintPending=false;
        if(race.phase==Phase::Results&&!resultStored) {
            if(race.session!=Session::Single) {
                if(season.record(race)){view.seasonDirty=true;saveSeason();}
            } else {
                const float time=race.riders[0].finishTime;float& record=best[race.course][race.difficulty][race.selected];
                view.newRecord=time>0&&(record<0||time<record);
                if(view.newRecord){record=time;if(!smoke) {
                    view.saved=savePreferences();
                    view.saved=(WritePrivateProfileStringW(recordSection(race.course).c_str(),keyName(race.selected,race.difficulty).c_str(),std::to_wstring(time).c_str(),profile.c_str())!=FALSE)&&view.saved;
                }}
            }
            resultStored=true;
        }
        view.best=best[race.phase==Phase::Menu?view.selectedCourse:race.course][race.difficulty][race.selected];
        audio.setPlaying(view.music&&!smoke&&hasFocus&&!race.paused);
    }
    void paint(HDC dc) {
        RECT bounds{};GetClientRect(window,&bounds);if(bounds.right<=0||bounds.bottom<=0)return;
        render(canvas,race,view);canvas.present(dc,bounds.right,bounds.bottom);
    }
};


LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));app->window=h;}
    if(!app)return DefWindowProcW(h,m,w,l);
    switch(m) {
        case WM_CLOSE:
            if(app->view.seasonDirty&&!app->saveSeason()) {
                if(MessageBoxW(h,L"赛季进度未能保存。仍然退出并放弃尚未保存的环节吗？",L"赛季尚未保存",MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES)return 0;
            }
            DestroyWindow(h);return 0;
        case WM_DESTROY:app->running=false;PostQuitMessage(0);return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);app->paint(dc);EndPaint(h,&ps);return 0;}
        case WM_KEYDOWN:if(w<256){app->keys[w]=true;if(!(l&(1LL<<30)))app->keyDown(static_cast<int>(w));}return 0;
        case WM_KEYUP:if(w<256)app->keys[w]=false;return 0;
        case WM_KILLFOCUS:
            app->clearInput();app->hasFocus=false;
            if(app->race.phase==Phase::Racing||app->race.phase==Phase::Countdown||app->race.phase==Phase::Finishing)app->race.paused=true;
            app->audio.setPlaying(false);return 0;
        case WM_SETFOCUS:app->hasFocus=true;return 0;
        case WM_MOUSEMOVE:{
            RECT rc{};GetClientRect(h,&rc);double s=std::min(rc.right/960.,rc.bottom/540.);
            if(s>0){app->view.mouseX=static_cast<int>(std::floor((GET_X_LPARAM(l)-(rc.right-960*s)/2)/s));app->view.mouseY=static_cast<int>(std::floor((GET_Y_LPARAM(l)-(rc.bottom-540*s)/2)/s));}
            return 0;
        }
        case WM_LBUTTONDOWN:app->view.mouseDown=true;SetCapture(h);app->mousePress();return 0;
        case WM_LBUTTONUP:app->view.mouseDown=false;ReleaseCapture();return 0;
        case WM_CAPTURECHANGED:app->view.mouseDown=false;return 0;
        case WM_GETMINMAXINFO:{auto* mm=reinterpret_cast<MINMAXINFO*>(l);mm->ptMinTrackSize={800,480};return 0;}
        default:return DefWindowProcW(h,m,w,l);
    }
}

int smokeTest(App& app,const std::wstring& output,bool captureImages) {
    std::error_code ec;std::filesystem::create_directories(output,ec);if(ec)return 2;
    std::ofstream report(std::filesystem::path(output+L"\\smoke-report.txt"));
    bool ok=true;
    auto check=[&](bool pass,const char* message){report<<(pass?"PASS ":"FAIL ")<<message<<'\n';ok&=pass;};
    auto shot=[&](const wchar_t* name){if(!captureImages)return;render(app.canvas,app.race,app.view);check(app.canvas.savePng(output+L"\\"+name),"PNG render capture");};
    auto click=[&](Rect target) {
        RECT bounds{};GetClientRect(app.window,&bounds);const double scale=std::min(bounds.right/960.,bounds.bottom/540.);
        const int x=static_cast<int>((bounds.right-960*scale)/2+(target.x+target.w/2)*scale);
        const int y=static_cast<int>((bounds.bottom-540*scale)/2+(target.y+target.h/2)*scale);
        SendMessageW(app.window,WM_MOUSEMOVE,0,MAKELPARAM(x,y));
        SendMessageW(app.window,WM_LBUTTONDOWN,0,0);SendMessageW(app.window,WM_LBUTTONUP,0,0);
    };
    app.view.time=3;shot(L"01-menu.png");
    click(ui::History);
    check(app.race.phase==Phase::SeasonHistory&&app.season.pastSeasonCount()==0,"Homepage history button opens the empty historical scoreboard");
    click(ui::HistoryNext);SendMessageW(app.window,WM_KEYDOWN,VK_LEFT,0);SendMessageW(app.window,WM_KEYDOWN,'R',0);
    check(app.view.historyPage==0&&!app.season.active&&app.race.phase==Phase::SeasonHistory,"Empty history navigation cannot start a race or create a season");
    SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);
    check(app.race.phase==Phase::Menu&&app.view.seasonMode,"History Escape returns to the same homepage mode");
    SendMessageW(app.window,WM_KEYDOWN,'T',0);
    check(!app.view.seasonMode,"Menu switches to independent single-race mode");
    SendMessageW(app.window,WM_KEYDOWN,'3',0);SendMessageW(app.window,WM_KEYUP,'3',0);
    check(app.race.selected==2,"Win32 keyboard character selection");
    SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);SendMessageW(app.window,WM_KEYUP,VK_RETURN,0);
    check(app.race.phase==Phase::Countdown,"Win32 Enter starts countdown");
    shot(L"02-countdown.png");
    bool captured=false,jumpCaptured=false,pausedCaptured=false,finalCaptured=false;
    for(int step=0;step<120*100&&app.race.phase!=Phase::Results;step++) {
        if(app.race.phase==Phase::Racing) {
            const auto in=autopilot(app.race);
            app.keys[VK_SHIFT]=in.sprint;app.jumpPending=in.jump;app.skillPending=in.skill;
        }
        app.update(FixedStep);
        if(app.race.clock>8&&!captured){shot(L"03-race.png");captured=true;}
        if(app.race.clock>8&&app.race.riders[0].jumpHeight>40&&!jumpCaptured){shot(L"04-jump.png");jumpCaptured=true;}
        if(app.race.riders[0].distance>810&&!finalCaptured){shot(L"07-final-straight.png");finalCaptured=true;}
        if(app.race.clock>12&&!pausedCaptured) {
            SendMessageW(app.window,WM_KEYDOWN,'P',0);
            const float before=app.race.clock;app.update(1.f);
            check(app.race.paused&&app.race.clock==before,"Pause freezes physics and race time");shot(L"05-pause.png");
            SendMessageW(app.window,WM_KEYDOWN,'P',0);pausedCaptured=true;
        }
    }
    check(app.race.phase==Phase::Results,"Complete playable race reaches results");
    check(app.race.riders[0].hits==0&&app.race.riders[0].jumps==8,"All hurdles can be cleared through input path");
    check(app.race.riders[0].skillUses>0,"Character skill activated through input path");
    shot(L"06-results.png");
    report<<"Player time: "<<app.race.riders[0].finishTime<<"; rank: "<<app.race.playerRank()<<"; jumps: "<<app.race.riders[0].jumps<<"; feathers: "<<app.race.riders[0].feathers<<'\n';
    SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);check(app.race.phase==Phase::Menu,"Escape returns to menu");
    // Exercise logical-coordinate mouse hit testing at the actual client size.
    RECT rc{};GetClientRect(app.window,&rc);const double s=std::min(rc.right/960.,rc.bottom/540.);
    const Rect card=ui::character(4);
    const int mx=static_cast<int>((rc.right-960*s)/2+(card.x+card.w/2)*s),my=static_cast<int>((rc.bottom-540*s)/2+(card.y+card.h/2)*s);
    SendMessageW(app.window,WM_MOUSEMOVE,0,MAKELPARAM(mx,my));SendMessageW(app.window,WM_LBUTTONDOWN,0,0);SendMessageW(app.window,WM_LBUTTONUP,0,0);
    check(app.race.selected==4,"Mouse selection after letterbox coordinate conversion");
    app.start();app.keys[VK_SHIFT]=true;
    SendMessageW(app.window,WM_KILLFOCUS,0,0);
    check(app.race.paused&&!app.keys[VK_SHIFT],"Focus loss pauses and releases held inputs");
    // Exercise E through the real window handler for every character and render
    // each menu description and active effect with the production renderer.
    for(int ch=0;ch<RiderCount;ch++) {
        app.toMenu();app.race.selected=ch;app.view.best=-1;app.view.time=3;
        shot((L"character-"+std::to_wstring(ch+1)+L"-menu.png").c_str());app.start();
        bool skillCaptured=false;
        for(int step=0;step<120*50&&app.race.phase!=Phase::Results;step++) {
            const auto input=autopilot(app.race);app.keys[VK_SHIFT]=input.sprint;app.jumpPending=input.jump;
            if(app.race.phase==Phase::Racing&&input.skill)SendMessageW(app.window,WM_KEYDOWN,'E',0);
            app.update(FixedStep);
            const auto& rider=app.race.riders[0];
            if(!skillCaptured&&rider.skillTime>0&&rider.skillTime<Characters[ch].skillDuration-.6f) {
                shot((L"character-"+std::to_wstring(ch+1)+L"-skill.png").c_str());skillCaptured=true;
            }
        }
        check(skillCaptured&&app.race.riders[0].skillUses>0,"Character E input and active skill render");
    }
    // Verify persistence in an isolated fixture, never in the player's real profile.
    {
        App saved;saved.directory=output+L"\\storage-fixture";saved.profile=saved.directory+L"\\data\\profile.ini";
        saved.view.music=false;saved.race.start(1,1);saved.race.phase=Phase::Results;saved.race.riders[0].finishTime=29.5f;
        saved.update(FixedStep);
        App restored;restored.directory=saved.directory;restored.profile=saved.profile;restored.load();
        check(saved.view.saved&&restored.race.selected==1&&restored.race.difficulty==1&&!restored.view.music&&restored.best[0][1][1]==29.5f,"Native profile write and reload preserves preferences and best time");
        WritePrivateProfileStringW(L"records",L"best_0_0",L"12.5",saved.profile.c_str());
        App versioned;versioned.directory=saved.directory;versioned.profile=saved.profile;versioned.load();
        wchar_t legacy[32];GetPrivateProfileStringW(L"records",L"best_0_0",L"missing",legacy,32,saved.profile.c_str());
        check(versioned.best[0][0][0]<0&&std::wstring(legacy)==L"12.5","Legacy records preserved separately from v0.2 skill balance");
    }
    // Full production App flow, with real atomic disk writes confined to a fixture.
    const auto originalDirectory=app.directory,originalProfile=app.profile;
    const auto singleRecordsBeforeSeason=app.best;
    app.directory=output+L"\\season-fixture";app.profile=app.directory+L"\\data\\profile.ini";
    app.smoke=false;app.view.music=false;app.season=Season{};app.toMenu();app.selectMode(true);
    app.race.selected=4;app.race.difficulty=1;
    SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
    check(app.race.phase==Phase::SeasonHub&&app.season.completed==0,"Menu creates a five-station season and opens briefing");
    shot(L"08-season-hub.png");
    SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);
    SendMessageW(app.window,WM_KEYDOWN,'1',0);SendMessageW(app.window,WM_KEYDOWN,VK_TAB,0);
    check(app.race.selected==4&&app.race.difficulty==1,"Active season locks character and difficulty");
    SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
    for(int event=0;event<SeasonSessions;event++) {
        if(app.race.phase==Phase::Results)SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);
        check(app.race.phase==Phase::SeasonHub&&app.season.completed==event,"Results return to the correct next-session briefing");
        if(event==2)shot(L"09-qualifying-grid.png");
        if(event==2) {
            const auto prior=app.season.serialize();
            app.toMenu();
            click(ui::NewSeason);
            check(app.view.confirmNewSeason&&app.season.serialize()==prior,"Mid-season new-season button asks before changing progress");
            shot(L"17-new-season-confirm.png");
            click(ui::Start);
            check(app.view.confirmNewSeason&&app.race.phase==Phase::Menu,"Confirmation blocks clicks through to the underlying page");
            SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);
            check(!app.view.confirmNewSeason&&app.season.serialize()==prior,"Escape cancels a restart and keeps completed sessions");
            SendMessageW(app.window,WM_KEYDOWN,'N',0);click(ui::CancelSeason);
            check(!app.view.confirmNewSeason&&app.season.serialize()==prior,"Mouse cancel keeps the current season");
            shot(L"18-active-season-menu.png");
            SendMessageW(app.window,WM_KEYDOWN,'C',0);
            check(app.race.phase==Phase::Menu,"Removed overview shortcut no longer navigates from the homepage");
            click(ui::Start);
            check(app.race.phase==Phase::SeasonHub&&app.season.serialize()==prior,"Homepage Continue opens career progress without resetting it");
        }
        if(event==9)shot(L"10-season-progress.png");
        SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
        check(app.race.phase==Phase::Countdown&&static_cast<int>(app.race.session)==1+event%3,"Enter launches the next practice, qualifying or feature");
        if(event==2)shot(L"11-feature-grid-start.png");
        bool mapCaptured=false;int capturedLights=0;bool signalCaptured=false;
        for(int step=0;step<120*120&&app.race.phase!=Phase::Results;step++) {
            const auto in=autopilot(app.race);app.keys[VK_SHIFT]=in.sprint;app.jumpPending=in.jump;app.skillPending=in.skill;
            app.update(FixedStep);
            if(event==2&&app.race.redLights()>capturedLights) {
                capturedLights=app.race.redLights();
                shot((L"start-light-"+std::to_wstring(capturedLights)+L".png").c_str());
            }
            if(event==2&&!signalCaptured&&app.race.phase==Phase::Racing) {
                shot(L"19-lights-out.png");signalCaptured=true;
            }
            if(event%3==0&&app.race.clock>12&&!mapCaptured) {
                shot((L"map-"+std::to_wstring(event/3+1)+L"-race.png").c_str());mapCaptured=true;
            }
        }
        check(app.race.phase==Phase::Results&&app.season.completed==event+1,"Session completion advances and persists once");
        const auto points=app.season.points();app.update(FixedStep);
        check(app.season.completed==event+1&&app.season.points()==points,"Repeated result updates do not duplicate points");
        App restored;restored.directory=app.directory;restored.profile=app.profile;restored.view.music=false;restored.load();
        check(restored.season.serialize()==app.season.serialize(),"Disk reload restores all results, grid and next session");
        if(event==0) {
            shot(L"12-practice-results.png");
            SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
            check(app.race.phase==Phase::Countdown&&app.race.session==Session::Qualifying,"Results Enter advances directly to qualifying");
            SendMessageW(app.window,WM_KEYDOWN,'P',0);SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);
            check(app.race.phase==Phase::SeasonHub&&app.season.completed==1,"Aborting a session keeps previously completed progress");
        }
        if(event==1)shot(L"13-qualifying-results.png");
        if(event==2)shot(L"14-feature-results.png");
        if(event==5) {
            app.season=restored.season;app.hub();check(app.season.round()==2,"Restart continues at station three practice");
            SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(app.window,WM_KEYDOWN,'T',0);
            for(int n=0;n<4;n++)SendMessageW(app.window,WM_KEYDOWN,'E',0);
            check(!app.view.seasonMode&&app.view.selectedCourse==4,"Single-race menu selects another map during a season");
            shot(L"16-single-map-menu.png");
            SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
            check(app.race.course==4&&app.race.session==Session::Single,"Selected map launches independently of the season");
            SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(app.window,WM_KEYDOWN,VK_ESCAPE,0);
            SendMessageW(app.window,WM_KEYDOWN,'T',0);SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
            check(app.race.phase==Phase::SeasonHub&&app.season.completed==6&&app.race.selected==4,"Return from single-race mode preserves season identity and progress");
        }
    }
    SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
    check(app.race.phase==Phase::SeasonFinal&&app.season.finished(),"Fifth feature opens season championship results");
    check(app.best==singleRecordsBeforeSeason,"Season sessions do not contaminate single-race records");
    shot(L"15-season-final.png");
    const auto firstSeasonPoints=app.season.points();
    app.toMenu();click(ui::History);
    check(app.race.phase==Phase::SeasonHistory&&app.season.pastSeasonCount()==1&&app.season.pastSeason(0)->points()==firstSeasonPoints,"Finished season appears in history before starting another season");
    click(ui::HubBack);app.hub();
    // A damaged current file must fall back to the last valid atomic backup.
    {std::ofstream broken(std::filesystem::path(app.seasonPath()),std::ios::trunc);broken<<"interrupted write";}
    App recovered;recovered.directory=app.directory;recovered.profile=app.profile;recovered.load();
    check(recovered.season.active&&recovered.season.completed==14&&!recovered.view.storageNotice.empty(),"Corrupt season falls back to the previous valid backup");
    check(app.saveSeason(),"Good in-memory season can repair a damaged current file");
    Season backup;check(readSeasonFile(app.seasonPath()+L".bak",backup)&&backup.completed==14,"Repair preserves the valid backup");
    // Simulate denied replacement without damaging the existing save.
    const HANDLE held=CreateFileW(app.seasonPath().c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(held!=INVALID_HANDLE_VALUE,"Open season fixture for save failure test");
    if(held!=INVALID_HANDLE_VALUE) {
        check(!app.saveSeason()&&app.view.seasonDirty,"Write failure leaves progress marked unsaved");
        app.toMenu();SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
        check(app.season.finished()&&app.season.number==1&&app.race.phase==Phase::SeasonFinal,"Unsaved championship cannot be replaced by a new season");
        CloseHandle(held);check(app.saveSeason()&&!app.view.seasonDirty,"Save retry succeeds after replacement is available");
    }
    app.toMenu();click(ui::NewSeason);
    SendMessageW(app.window,WM_KEYDOWN,'N',0);
    check(!app.view.confirmNewSeason&&app.season.finished()&&app.race.phase==Phase::Menu,"Finished season ignores hidden restart button and shortcut");
    click(ui::Start);
    check(app.view.confirmNewSeason&&app.season.finished(),"Finished-season replacement requires confirmation");
    SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
    check(app.race.phase==Phase::Menu&&app.season.number==2&&app.season.selecting&&!app.locked(),"Confirmed new season opens unlocked character selection");
    click(ui::History);
    check(app.race.phase==Phase::SeasonHistory&&app.season.pastSeasonCount()==1&&app.season.pastSeason(0)->points()==firstSeasonPoints,"Starting another season retains historical scores without duplication");
    SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
    SendMessageW(app.window,WM_KEYDOWN,'1',0);SendMessageW(app.window,WM_KEYDOWN,VK_TAB,0);
    SendMessageW(app.window,WM_KEYDOWN,VK_RETURN,0);
    check(app.race.phase==Phase::SeasonHub&&app.season.number==2&&app.season.character==0&&app.season.completed==0,"New season allows a new partner after championship completion");
    // Confirm a reset of an unfinished season, including rollback on disk failure.
    app.season.completed=1;check(app.saveSeason(),"Save active-season reset fixture");app.toMenu();
    const auto beforeReset=app.season.serialize();
    const HANDLE resetHeld=CreateFileW(app.seasonPath().c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(resetHeld!=INVALID_HANDLE_VALUE) {
        click(ui::NewSeason);click(ui::ConfirmSeason);
        check(app.view.confirmNewSeason&&app.season.serialize()==beforeReset&&!app.view.seasonDirty,"Failed restart save rolls back to the intact previous season");
        CloseHandle(resetHeld);
        click(ui::ConfirmSeason);
    } else {check(false,"Open reset fixture for replacement failure");click(ui::NewSeason);click(ui::ConfirmSeason);}
    App resetReload;resetReload.directory=app.directory;resetReload.profile=app.profile;resetReload.load();
    check(!app.view.confirmNewSeason&&app.race.phase==Phase::Menu&&app.season.number==3&&app.season.completed==0&&app.season.selecting&&resetReload.season.serialize()==app.season.serialize(),"Confirmed mid-season reset persists the next season's unlocked selection");
    check(!resetReload.locked(),"Reloading before character selection does not lock the previous partner");
    check(resetReload.season.pastSeasonCount()==2&&resetReload.season.pastSeason(1)->points()==firstSeasonPoints,"Atomic reload restores both completed and early-ended seasons");
    app.season=resetReload.season;app.toMenu();
    const auto beforeHistory=app.season.serialize();
    click(ui::History);
    check(app.view.historyPage==0&&app.season.pastSeason(app.view.historyPage)->number==2,"History opens the newest previous season");
    click(ui::HistoryNext);
    check(app.view.historyPage==1&&app.season.pastSeason(app.view.historyPage)->number==1,"History mouse button opens an older season");
    click(ui::HistoryNext);check(app.view.historyPage==1,"History pagination clamps at the oldest entry");
    SendMessageW(app.window,WM_KEYDOWN,VK_LEFT,0);
    SendMessageW(app.window,WM_KEYDOWN,'N',0);SendMessageW(app.window,WM_KEYDOWN,'R',0);
    check(app.view.historyPage==0&&!app.view.confirmNewSeason&&app.season.serialize()==beforeHistory,"History keyboard navigation is read-only and ignores race or reset shortcuts");
    click(ui::HubBack);
    check(app.race.phase==Phase::Menu&&!app.locked()&&app.season.serialize()==beforeHistory,"Returning from history preserves pending character selection");
    click(ui::SingleMode);click(ui::History);
    check(!app.view.seasonMode&&app.race.phase==Phase::Menu&&app.season.serialize()==beforeHistory,"Practice mode ignores hidden history button and preserves season records");
    click(ui::SeasonMode);
    click(ui::character(3));const int oldDifficulty=app.race.difficulty;click(ui::Difficulty);
    check(app.race.selected==3&&app.race.difficulty==1-oldDifficulty,"Mid-season reset permits mouse selection of a new partner and difficulty");
    click(ui::NewSeason);
    check(!app.view.confirmNewSeason&&app.season.number==3,"Pending selection cannot skip an extra season");
    const HANDLE selectionHeld=CreateFileW(app.seasonPath().c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(selectionHeld!=INVALID_HANDLE_VALUE) {
        click(ui::Start);
        check(app.race.phase==Phase::Menu&&app.season.selecting&&!app.locked()&&app.race.selected==3,"Failed selection save keeps the new season editable");
        CloseHandle(selectionHeld);
    } else check(false,"Open selection fixture for replacement failure");
    click(ui::Start);
    check(app.race.phase==Phase::SeasonHub&&app.season.number==3&&!app.season.selecting&&app.season.character==3&&app.season.difficulty==1-oldDifficulty,"Confirming a new partner starts the same next season without incrementing twice");
    app.toMenu();SendMessageW(app.window,WM_KEYDOWN,'2',0);click(ui::character(4));
    check(app.race.selected==3&&app.locked(),"Selected season identity stays locked until another confirmed restart");
    // Real Win32 input: a quick Shift tap must survive key-up before the next tick.
    app.season.begin(4,1,true);app.season.completed=2;app.hub();
    app.clearInput();app.race.start(4,1,47,0,Session::Feature,{{0,1,2,3,4}},31);app.resultStored=false;
    const float gridDistance=app.race.riders[0].distance;
    SendMessageW(app.window,WM_KEYDOWN,VK_SHIFT,0);SendMessageW(app.window,WM_KEYUP,VK_SHIFT,0);app.update(FixedStep);
    check(app.race.riders[0].started&&app.race.riders[0].distance>gridDistance&&app.race.riders[0].penaltyTime==5,"Quick early Shift tap starts moving and records one penalty");
    shot(L"20-false-start.png");
    for(int frame=0;frame<120*100&&app.race.phase!=Phase::Results;frame++) {
        const auto in=autopilot(app.race);app.keys[VK_SHIFT]=in.sprint;app.jumpPending=in.jump;app.skillPending=in.skill;app.update(FixedStep);
    }
    check(app.race.phase==Phase::Results&&std::abs(app.race.riders[0].finishTime-app.race.riders[0].splits[2]-5)<.0001f,"Win32 false-start path shows a penalty-adjusted result");
    check(app.season.completed==3&&app.season.roundPoints(0,4)==RacePoints[app.race.playerRank()-1],"App persists the penalty-adjusted feature rank and points");
    shot(L"21-penalty-results.png");
    app.clearInput();app.race.start(4,0,47,0,Session::Feature,{{0,1,2,3,4}},32);app.resultStored=false;
    for(int frame=0;frame<120*9&&app.race.phase==Phase::Countdown;frame++)app.update(FixedStep);
    for(int frame=0;frame<120;frame++)app.update(FixedStep);
    check(app.race.phase==Phase::Racing&&!app.race.riders[0].started&&app.race.riders[0].distance==gridDistance,"No-input player remains still after the lights go out");
    shot(L"22-waiting-for-shift.png");
    SendMessageW(app.window,WM_KEYDOWN,VK_SHIFT,0);SendMessageW(app.window,WM_KEYUP,VK_SHIFT,0);app.update(FixedStep);
    check(app.race.riders[0].started&&app.race.riders[0].penaltyTime==0,"Quick legal Shift tap starts without penalty");
    const float launchedAt=app.race.riders[0].distance;
    for(int frame=0;frame<120;frame++)app.update(FixedStep);
    check(app.race.riders[0].distance>launchedAt+20&&!app.race.riders[0].sprinting,"Automatic running continues after the starting tap is released");
    {
        App mapped;mapped.directory=output+L"\\map-record-fixture";mapped.profile=mapped.directory+L"\\data\\profile.ini";mapped.view.music=false;
        for(int map:{1,4}) {
            mapped.race.start(0,1,20261003,map);mapped.race.phase=Phase::Results;mapped.race.riders[0].finishTime=40.f+map;
            mapped.resultStored=false;mapped.update(FixedStep);
        }
        App reloaded;reloaded.directory=mapped.directory;reloaded.profile=mapped.profile;reloaded.load();
        check(reloaded.best[1][1][0]==41.f&&reloaded.best[4][1][0]==44.f&&reloaded.best[0][1][0]<0,"Single-race best times are isolated by map, character and difficulty");
    }
    app.directory=originalDirectory;app.profile=originalProfile;app.smoke=true;
    report<<(ok?"ALL SMOKE CHECKS PASSED\n":"SMOKE CHECK FAILED\n");return ok?0:1;
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware();
    int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    const bool inputTest=argc>=2&&std::wstring(argv[1])==L"--input-test";
    bool smoke=inputTest||(argc>=2&&std::wstring(argv[1])==L"--smoke");
    std::wstring captureDir=argc>=3?argv[2]:executableDirectory()+(inputTest?L"\\build\\input-checks":L"\\captures");
    LocalFree(argv);
    App app;app.smoke=smoke;if(!smoke)app.load();
    WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"AnemoiWindCupWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wc.style=CS_HREDRAW|CS_VREDRAW;
    if(!RegisterClassW(&wc))return 3;
    RECT desktop{};SystemParametersInfoW(SPI_GETWORKAREA,0,&desktop,0);
    const int availableW=desktop.right-desktop.left-80,availableH=desktop.bottom-desktop.top-100;
    const double size=std::min({availableW/960.,availableH/540.,4./3.});
    const int cw=static_cast<int>(960*std::max(.8,size)),ch=static_cast<int>(540*std::max(.8,size));
    RECT frame{0,0,cw,ch};AdjustWindowRect(&frame,WS_OVERLAPPEDWINDOW,FALSE);
    HWND window=CreateWindowExW(0,wc.lpszClassName,L"Anemoi · 风野杯 | 像素赛马",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,frame.right-frame.left,frame.bottom-frame.top,nullptr,nullptr,instance,&app);
    if(!window)return 4;
    if(smoke){const int code=smokeTest(app,captureDir,!inputTest);DestroyWindow(window);return code;}
    ShowWindow(window,show);UpdateWindow(window);timeBeginPeriod(1);
    using FrameClock=std::chrono::steady_clock;
    const auto frameInterval=std::chrono::duration_cast<FrameClock::duration>(std::chrono::duration<double>(1./60.));
    auto previous=FrameClock::now(),nextFrame=previous;double accumulator=0;
    while(app.running) {
        MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){app.running=false;break;}TranslateMessage(&msg);DispatchMessageW(&msg);}
        if(!app.running)break;
        const auto now=FrameClock::now();
        accumulator+=std::min(.1,std::chrono::duration<double>(now-previous).count());previous=now;
        while(accumulator>=FixedStep){app.update(FixedStep);accumulator-=FixedStep;}
        if(now>=nextFrame) {
            if(!IsIconic(window)) {
                // WM_PAINT is the only window drawing path. Invalidate without
                // background erase; the presenter supplies a complete frame.
                InvalidateRect(window,nullptr,FALSE);
                UpdateWindow(window);
            }
            nextFrame+=frameInterval;
            const auto afterPaint=FrameClock::now();
            if(nextFrame<=afterPaint)nextFrame=afterPaint+frameInterval;
        }
        const double waitMs=std::chrono::duration<double,std::milli>(nextFrame-FrameClock::now()).count();
        const DWORD timeout=IsIconic(window)?100:static_cast<DWORD>(std::max(1.,std::ceil(waitMs)));
        MsgWaitForMultipleObjectsEx(0,nullptr,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
    app.savePreferences();timeEndPeriod(1);return 0;
}
