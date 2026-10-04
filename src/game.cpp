#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <numeric>
#include <random>

namespace anemoi {
const std::array<Course, CourseCount> Courses{{
    {L"青空牧场", L"开幕站 · 风车与草甸", L"熟悉八道木栏，利用两段顺风接续冲刺。", Landscape::Meadow,1200,
     0xB3DDD8,0xEEF5DD,0xB6D593,0xD5D8A0,0x5C977C,
     {155,290,438,584,727,882,1040,1140}, {{320,425,3,0,L"顺风草甸"},{755,870,3,0,L"顺风草甸"}}},
    {L"潮风海岸", L"第二站 · 海湾与灯塔", L"海堤逆风会拖慢脚步，把体力留给沿海顺风段。", Landscape::Coast,1400,
     0x9CD3E3,0xE8F3E6,0xA8D3C3,0xD8D2AD,0x529CB2,
     {140,292,450,603,760,918,1080,1238,1340},
     {{320,450,-2,2,L"海堤逆风"},{670,815,4,0,L"沿海顺风"},{1100,1240,4,0,L"潮汐长廊"}}},
    {L"木漏日林道", L"第三站 · 林荫与溪流", L"树林遮风，密集木栏后有回复体力的林荫区。", Landscape::Forest,1600,
     0xA6C9BC,0xE2EDCC,0x8DB583,0xBCCA99,0x6E9870,
     {145,275,430,590,740,875,1040,1200,1360,1500},
     {{460,580,0,-5,L"林荫休整"},{930,1060,0,-5,L"溪畔休整"},{1250,1370,3,0,L"林隙来风"}}},
    {L"黄昏丘陵", L"第四站 · 落日与长坡", L"上坡更耗体，下坡借势加速；提前安排补给技能。", Landscape::Hills,1800,
     0xDEB29D,0xF4E2BC,0xBFBE82,0xD7C09A,0xB38465,
     {150,308,466,620,784,945,1094,1244,1402,1560,1700},
     {{340,540,-3,4,L"缓坡上行"},{560,750,4,0,L"落日下坡"},{1120,1310,-3,4,L"山脊上行"},{1330,1530,4,0,L"山坡疾行"}}},
    {L"星灯风车原", L"最终站 · 星光与灯火", L"最长的决胜路线，保存体力跨过最后的连续木栏。", Landscape::Night,2000,
     0x526783,0xB0BCC6,0x8EA58D,0xB0BCA6,0x8785B4,
     {155,310,465,620,780,944,1100,1258,1412,1568,1730,1900},
     {{360,510,4,0,L"星原顺风"},{820,970,-2,2,L"风车逆流"},{1280,1415,4,0,L"夜航来风"},{1750,1880,5,0,L"终章长风"}}}
}};
const wchar_t* sessionName(Session session) {
    switch(session) {
        case Session::Practice:return L"练习赛";
        case Session::Qualifying:return L"排位赛";
        case Session::Feature:return L"正赛";
        default:return L"单站练习";
    }
}
const std::array<Character, RiderCount> Characters{{
    {L"スピカ", L"SPICA", L"窑火补给", L"立即回复 32 体力；6 秒内冲刺耗体减少 60%。",
     L"技能期间速度 +6 m/s，适合中段持续推进。", L"田间耐力：平跑时每秒额外恢复 3 体力。",
     L"体力较低时发动，接续长距离冲刺。", L"披萨窑的主人，熟悉田间的生活。", Skill::Supply,
     Appearance::WhiteHair, 0x5BAEAC, 0xF4F2E9, 0xC9D4D5, 0.f, 3.f, 0.f, 6.f},
    {L"小詠", L"KOYOMI", L"小鸟速递", L"3 秒内速度 +14 m/s，冲刺耗体减少 25%。",
     L"短时速递：把握直路或终点前的冲刺时机。", L"轻快步调：基础速度 +0.4 m/s。",
     L"留一些体力，在直路或终点前发动。", L"一心成为独当一面的邮递员。", Skill::Courier,
     Appearance::RedCap, 0x389ABB, 0x57C8E3, 0x358FBC, .4f, 0.f, 0.f, 3.f},
    {L"陽彩", L"HIIRO", L"奇妙采集", L"6 秒自动收集经过的风羽，充能与补给翻倍。",
     L"速度 +7 m/s；每片风羽再加速 5 m/s，持续 1.8 秒。", L"发现之眼：风之力自然恢复额外 +0.7/s。",
     L"靠近风羽时发动；仍需起跳越栏。", L"热衷寻找漂亮、奇妙的事物与 UMA。", Skill::Curiosity,
     Appearance::PeachHair, 0xD58E80, 0xF4BB99, 0xD7947D, 0.f, 0.f, .08f, 6.f},
    {L"六花", L"RIKKA", L"万全应援", L"回复 18 体力、解除失速；6 秒内速度 +8 m/s。",
     L"期间抵挡一次撞栏，冲刺耗体减少 30%。", L"细心照料：撞栏造成的失速时间缩短 25%。",
     L"失速或体力不足时发动，护航一次。", L"为哥哥奔走、可靠又周到的妹妹。", Skill::Support,
     Appearance::PurpleRibbon, 0x9778B5, 0x333241, 0x212231, 0.f, 0.f, 0.f, 6.f},
    {L"愛乃", L"AINO", L"青空滑翔", L"起飞滑翔 4.5 秒，自动越过足够高度下的木栏。",
     L"速度 +15 m/s，冲刺耗体减半；起降需留余量。", L"向风而行：顺风草甸额外增加 2 m/s。",
     L"木栏前 15–30 m 发动，借风滑翔。", L"憧憬天空、追逐飞行梦想的少女。", Skill::Glide,
     Appearance::StrawHat, 0x77AA70, 0xD6BF8F, 0xB4976D, 0.f, 0.f, 0.f, 4.5f}
}};

void Race::start(int character, int level, uint32_t courseSeed, int track, Session event, std::array<int,RiderCount> grid, uint32_t startSeed) {
    selected = std::clamp(character, 0, RiderCount - 1);
    difficulty = std::clamp(level, 0, 1);
    seed = courseSeed;
    course = std::clamp(track,0,CourseCount-1);session=event;length=Courses[course].length;
    phase = Phase::Countdown;
    paused = false; clock = 0; countdown = 3; finishWait = 0;
    std::mt19937 random(startSeed ? startSeed : courseSeed);
    startElapsed = 0;
    lightsHold = event == Session::Feature ? std::uniform_real_distribution<float>(.1f,3.f)(random) : 0;
    if (event == Session::Feature) countdown = 5 + lightsHold;
    notice.clear(); noticeTime = 0;
    hurdles.clear(); feathers.clear();
    const int offset = static_cast<int>(seed % 13) - 6;
    for (float d : Courses[course].hurdles)
        hurdles.push_back(d + static_cast<float>(offset));
    // Feathers are suspended above hurdles, rewarding the same well-timed jump.
    for (float d : hurdles) feathers.push_back(d);
    auto sortedGrid=grid;std::sort(sortedGrid.begin(),sortedGrid.end());
    if(sortedGrid!=std::array<int,RiderCount>{{0,1,2,3,4}})grid={{0,1,2,3,4}};
    for (int i = 0; i < RiderCount; ++i) {
        riders[i] = Rider{};
        riders[i].character = (selected + i) % RiderCount;
        constexpr int lanes[] = {2, 0, 4, 1, 3};
        riders[i].lane = lanes[i];
        riders[i].gridSlot=static_cast<int>(std::find(grid.begin(),grid.end(),riders[i].character)-grid.begin());
        if(event==Session::Feature) {
            riders[i].distance=-riders[i].gridSlot*GridSpacing;
            riders[i].started=false;
            if(i!=0)riders[i].reactionDelay=std::uniform_real_distribution<float>(.1f,.3f)(random);
        }
    }
}

void Race::returnToMenu() { phase = Phase::Menu; paused = false; }
void Race::togglePause() {
    if (phase == Phase::Countdown || phase == Phase::Racing || phase == Phase::Finishing)
        paused = !paused;
}
void Race::notify(const wchar_t* message) { notice = message; noticeTime = 1.7f; }
int Race::redLights() const {
    return session==Session::Feature&&phase==Phase::Countdown ? std::clamp(static_cast<int>(startElapsed+1e-6),0,5) : 0;
}
const CourseZone* Race::zoneAt(float d) const {
    for(const auto& zone:Courses[course].zones)if(d>=zone.begin&&d<zone.end)return &zone;
    return nullptr;
}
bool Race::inTailwind(float d) const {const auto* zone=zoneAt(d);return zone&&zone->speed>0;}
float Race::splitDistance(int section) const {return length-800+std::clamp(section,0,2)*400;}
float Race::jumpDuration(int c) const { return .94f + Characters[c].jumpBonus; }
float Race::nextHurdleDistance(int index) const {
    const Rider& r = riders[index];
    return r.nextHurdle < static_cast<int>(hurdles.size()) ? hurdles[r.nextHurdle] - r.distance : 9999.f;
}

bool Race::skillReady(int index) const {
    const Rider& r = riders[index];
    return r.charge >= 99.99f && r.skillTime <= 0 && r.finishTime < 0;
}
bool Race::skillRecommended(int index) const {
    if (!skillReady(index)) return false;
    const Rider& r = riders[index];
    const float d = nextHurdleDistance(index);
    if (length - r.distance < 160) return true;
    switch (Characters[r.character].skill) {
        case Skill::Supply: return r.stamina <= 45;
        case Skill::Courier: return r.stamina >= 32;
        case Skill::Curiosity: return d < 65 && d > 0;
        case Skill::Support: return r.stun > 0 || r.stamina <= 45 || (d < 35 && d > 8);
        case Skill::Glide: return d >= 15 && d <= 30;
    }
    return false;
}
std::wstring Race::skillStatus(int index) const {
    const Rider& r = riders[index];
    const Character& c = Characters[r.character];
    if (r.finishTime >= 0) return L"已完赛";
    if (r.skillTime > 0) {
        std::wstring state = std::wstring(c.trait) + L" · " + formatSeconds(r.skillTime);
        if (c.skill == Skill::Support) state += r.shield ? L" · 护航可用" : L" · 已抵挡";
        if (c.skill == Skill::Curiosity && r.boostTime > 0) state += L" · 采集加速";
        return state;
    }
    if (skillReady(index)) return std::wstring(skillRecommended(index) ? L"时机合适！E · " : L"E 已就绪 · ") + c.tip;
    return L"越栏 +15、风羽 +10 风之力";
}
float Race::sectionTime(int index, int section) const {
    if (section < 0 || section >= 3) return -1;
    const auto& splits = riders[index].splits;
    if (splits[section] < 0) return -1;
    return section == 0 ? splits[0] : (splits[section-1] >= 0 ? splits[section] - splits[section-1] : -1);
}
void Race::activateSkill(int index) {
    Rider& r = riders[index];
    const Character& c = Characters[r.character];
    r.charge = 0; r.skillTime = c.skillDuration; ++r.skillUses;
    switch (c.skill) {
        case Skill::Supply: r.stamina = std::min(100.f, r.stamina + 32); break;
        case Skill::Support:
            r.stamina = std::min(100.f, r.stamina + 18); r.stun = 0; r.shield = true; break;
        case Skill::Glide: r.glideStartHeight = r.jumpHeight; r.jumpTime = -1; break;
        default: break;
    }
    if (index == 0) { notice = std::wstring(c.name) + L" · " + c.trait + L"！"; noticeTime = 1.7f; }
}

void Race::advance(int i, float dt, Input input) {
    Rider& r = riders[i];
    if (r.finishTime >= 0) { r.speed = std::max(0.f, r.speed - dt * 14); return; }
    if(!r.started) {
        if(i==0) {
            if(!input.sprint)return;
            r.started=true;
            if(phase==Phase::Countdown) {
                r.penaltyTime=FalseStartPenalty;
                notify(L"抢跑！最终成绩加罚 5 秒");
            }
        } else {
            if(phase==Phase::Countdown)return;
            const float held=std::min(dt,r.reactionDelay);
            r.reactionDelay-=held;dt-=held;
            if(dt<=0)return;
            r.started=true;
        }
    }
    const Character& c = Characters[r.character];
    if (i != 0) {
        if (r.stamina < 12) r.aiSprint = false;
        if (r.stamina > 70) r.aiSprint = true;
        input.sprint = r.aiSprint;
        const float lead = r.speed * jumpDuration(r.character) * .46f;
        const uint32_t roll = (seed + i * 19u + r.nextHurdle * 37u) % 17u;
        input.jump = nextHurdleDistance(i) < lead && nextHurdleDistance(i) > 0 && roll > (difficulty ? 0u : 2u);
        input.skill = skillRecommended(i);
    }
    r.stun = std::max(0.f, r.stun - dt);
    r.skillTime = std::max(0.f, r.skillTime - dt);
    r.boostTime = std::max(0.f, r.boostTime - dt);
    if (r.skillTime <= 0) r.shield = false;
    r.charge = std::min(100.f, r.charge + dt * (c.skill == Skill::Curiosity ? 2.f : 1.3f));
    if (phase != Phase::Countdown && input.skill && skillReady(i)) activateSkill(i);
    const bool active = r.skillTime > 0;
    if (r.exhausted && r.stamina >= 24) r.exhausted = false;
    r.sprinting = input.sprint && !r.exhausted && r.stun == 0;
    const auto* zone=zoneAt(r.distance);
    float drain = 22 + (zone?zone->drain:0);
    if (active) {
        if (c.skill == Skill::Supply) drain *= .4f;
        if (c.skill == Skill::Courier) drain *= .75f;
        if (c.skill == Skill::Support) drain *= .7f;
        if (c.skill == Skill::Glide) drain *= .5f;
    }
    if (r.sprinting) r.stamina -= dt * drain;
    else r.stamina += dt * (13 + c.recoveryBonus - (zone?std::min(0.f,zone->drain):0.f));
    r.stamina = std::clamp(r.stamina, 0.f, 100.f);
    if (r.stamina <= 0 && !r.exhausted) {
        r.exhausted = true; r.sprinting = false;
        if (i == 0) notify(L"体力耗尽 · 松开 Shift 恢复体力");
    }
    if (active && c.skill == Skill::Glide) {
        const float ascent = std::min(1.f, (c.skillDuration - r.skillTime + dt) / .25f);
        r.jumpHeight = (r.glideStartHeight + (48 - r.glideStartHeight) * ascent) * std::min(1.f, r.skillTime / .3f);
    } else {
        if (input.jump && r.jumpTime < 0 && r.stun <= 0) r.jumpTime = 0;
        if (r.jumpTime < 0) r.jumpHeight = 0;
    }
    if (r.jumpTime >= 0) {
        r.jumpTime += dt;
        if (r.jumpTime >= jumpDuration(r.character)) { r.jumpTime = -1; r.jumpHeight = 0; }
        else r.jumpHeight = std::sin(r.jumpTime / jumpDuration(r.character) * 3.14159265f) * 46;
    }
    float target = 27.f + c.speedBonus + (r.sprinting ? 12.f : 0.f);
    if (i != 0) target -= difficulty ? .1f + i * .12f : 1.0f + i * .22f;
    if (zone) target += zone->speed + (zone->speed>0&&c.skill==Skill::Glide?2.f:0.f);
    if (active) {
        switch (c.skill) {
            case Skill::Supply: target += 6; break;
            case Skill::Courier: target += 14; break;
            case Skill::Curiosity: target += 7; break;
            case Skill::Support: target += 8; break;
            case Skill::Glide: target += 15; break;
        }
    }
    if (r.boostTime > 0) target += 5;
    if (r.exhausted) target -= 4;
    if (r.stun > 0) target = 13;
    r.speed += (target - r.speed) * std::min(1.f, dt * 4.f);
    const float before = r.distance;
    r.distance += r.speed * dt;
    // Crossing tests, rather than overlap-only tests, prevent tunnelling.
    while (r.nextHurdle < static_cast<int>(hurdles.size()) && r.distance >= hurdles[r.nextHurdle]) {
        if (r.jumpHeight >= (difficulty ? 23.f : 18.f)) {
            ++r.jumps; r.charge = std::min(100.f, r.charge + 15.f);
            if (i == 0) notify(L"漂亮越栏！  +15 风之力");
        } else if (r.shield) {
            r.shield = false; ++r.saves;
            if (i == 0) notify(L"万全应援 · 抵挡撞栏，保持步调！");
        } else {
            ++r.hits; r.stun = c.skill == Skill::Support ? .825f : 1.1f; r.speed *= .55f;
            if (i == 0) notify(L"碰到木栏 · 提前按空格起跳");
        }
        ++r.nextHurdle;
    }
    while (r.nextFeather < static_cast<int>(feathers.size()) && r.distance >= feathers[r.nextFeather]) {
        const bool gathering = active && c.skill == Skill::Curiosity;
        if (r.jumpHeight >= 24 || gathering) {
            ++r.feathers; r.charge = std::min(100.f, r.charge + (gathering ? 20.f : 10.f));
            r.stamina = std::min(100.f, r.stamina + (gathering ? 10.f : 5.f));
            if (gathering) r.boostTime = 1.8f;
            if (i == 0) {
                if (gathering) notify(L"奇妙采集 · 双倍风羽，发现新速度！");
                else notify(L"完美越栏 + 风羽！  +25 风之力");
            }
        }
        ++r.nextFeather;
    }
    // Every runner gets independently interpolated sectionals, including the finish.
    for (int section = 0; section < 3; ++section) {
        const float line = splitDistance(section);
        if (phase != Phase::Countdown && r.splits[section] < 0 && before <= line && r.distance >= line) {
            const float fraction = (line - before) / std::max(.00001f, r.distance - before);
            r.splits[section] = clock - dt + std::clamp(fraction, 0.f, 1.f) * dt;
        }
    }
    if (r.distance >= length) {
        r.finishTime = r.splits[2] + r.penaltyTime;
        r.distance = length; r.jumpTime = -1; r.jumpHeight = 0; r.sprinting = false;
    }
}

void Race::tick(float dt, const Input& input) {
    if (paused || (phase!=Phase::Countdown&&phase!=Phase::Racing&&phase!=Phase::Finishing) || dt<=0 || !std::isfinite(dt)) return;
    dt = std::min(dt, .1f);
    if (phase == Phase::Countdown) {
        if(session!=Session::Feature) {
            countdown -= dt;
            if (countdown <= 0) { countdown = 0; phase = Phase::Racing; notify(L"出发！让风带你向前"); }
            return;
        }
        // Split a step at lights-out: no pre-signal time enters the race clock
        // or an AI's reaction delay. A held sprint before this boundary is early.
        const double remaining=5.0+lightsHold-startElapsed;
        const float waiting=static_cast<float>(std::min(static_cast<double>(dt),remaining));
        if(waiting>0)advance(0,waiting,input);
        startElapsed+=waiting;
        countdown=static_cast<float>(std::max(0.0,5.0+lightsHold-startElapsed));
        if(countdown>1e-6f)return;
        countdown=0;phase=Phase::Racing;
        if(riders[0].penaltyTime>0)notify(L"灯灭！抢跑罚时 +5 秒已记录");
        else notify(L"灯灭！按 Shift 起跑");
        dt-=waiting;
        if(dt<=0)return;
    }
    clock += dt;
    noticeTime = std::max(0.f, noticeTime - dt);
    for (int i = 0; i < RiderCount; ++i) advance(i, dt, input);
    if (phase == Phase::Racing && riders[0].finishTime >= 0) { phase = Phase::Finishing; finishWait = 0; }
    if (phase == Phase::Finishing) {
        finishWait += dt;
        const bool allFinished = std::all_of(riders.begin(), riders.end(), [](const Rider& r) { return r.finishTime >= 0; });
        if (allFinished || finishWait >= 15) phase = Phase::Results;
    }
}

std::array<int, RiderCount> Race::standings() const {
    std::array<int, RiderCount> order{{0,1,2,3,4}};
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const Rider& ra = riders[a]; const Rider& rb = riders[b];
        if (ra.finishTime >= 0 && rb.finishTime >= 0) return ra.finishTime < rb.finishTime;
        if (ra.finishTime >= 0) return true;
        if (rb.finishTime >= 0) return false;
        if (session == Session::Feature && ra.distance == rb.distance) return ra.gridSlot < rb.gridSlot;
        return ra.distance > rb.distance;
    });
    return order;
}
int Race::playerRank() const {
    const auto order = standings();
    return static_cast<int>(std::find(order.begin(), order.end(), 0) - order.begin()) + 1;
}
int Race::score() const {
    return std::max(0, (6 - playerRank()) * 200 + riders[0].jumps * 100 + riders[0].feathers * 50 - riders[0].hits * 60);
}
Input autopilot(const Race& race) {
    const Rider& p = race.riders[0];
    if(race.session==Session::Feature&&race.phase==Phase::Countdown)return {};
    // Follow the courier's advice: bank stamina before a ready delivery. Without
    // this, the demo controller hovered at 5% and never found an activation window.
    const bool prepareDelivery = Characters[p.character].skill == Skill::Courier && race.skillReady() && p.stamina < 32;
    const bool sprint = !p.exhausted && p.stamina > 5 && !prepareDelivery;
    const float lead = p.speed * race.jumpDuration(p.character) * .46f;
    return {race.nextHurdleDistance() < lead && race.nextHurdleDistance() > 0 && p.jumpTime < 0, sprint, race.skillRecommended()};
}
std::wstring formatTime(float seconds) {
    if (seconds < 0) return L"--:--.--";
    const int centis = static_cast<int>(seconds * 100.f + .5f);
    wchar_t buf[32];
    std::swprintf(buf, 32, L"%02d:%02d.%02d", centis / 6000, (centis / 100) % 60, centis % 100);
    return buf;
}
std::wstring formatSeconds(float seconds) {
    if (seconds < 0) return L"--.--s";
    wchar_t buf[32]; std::swprintf(buf, 32, L"%.2fs", seconds); return buf;
}
}
