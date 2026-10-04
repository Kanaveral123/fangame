#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace anemoi {
constexpr int RiderCount = 5;
constexpr int CourseCount = 5;
constexpr float FixedStep = 1.0f / 120.0f;
constexpr float GridSpacing = 10.f;
constexpr float FalseStartPenalty = 5.f;
enum class Phase { Menu, SeasonHub, SeasonFinal, SeasonHistory, Countdown, Racing, Finishing, Results };
enum class Session { Single, Practice, Qualifying, Feature };
enum class Landscape { Meadow, Coast, Forest, Hills, Night };
struct CourseZone { float begin, end, speed, drain; const wchar_t* label; };
struct Course {
    const wchar_t* name;
    const wchar_t* subtitle;
    const wchar_t* strategy;
    Landscape landscape;
    float length;
    uint32_t sky, horizon, ground, track, accent;
    std::vector<float> hurdles;
    std::vector<CourseZone> zones;
};
extern const std::array<Course, CourseCount> Courses;
const wchar_t* sessionName(Session session);
enum class Appearance { StrawHat, RedCap, WhiteHair, PurpleRibbon, PeachHair };
enum class Skill { Supply, Courier, Curiosity, Support, Glide };
struct Character {
    const wchar_t* name;
    const wchar_t* roman;
    const wchar_t* trait;
    const wchar_t* description;
    const wchar_t* detail;
    const wchar_t* passive;
    const wchar_t* tip;
    const wchar_t* lore;
    Skill skill;
    Appearance appearance;
    uint32_t accent, hair, hairShade;
    float speedBonus, recoveryBonus, jumpBonus, skillDuration;
};
extern const std::array<Character, RiderCount> Characters;
struct Input {
    bool jump = false;
    bool sprint = false;
    bool skill = false;
};
struct Rider {
    int character = 0;
    int lane = 0;
    int gridSlot = 0;
    float reactionDelay = 0;
    bool started = true;
    float penaltyTime = 0;
    float distance = 0, speed = 0, stamina = 100;
    float jumpTime = -1, jumpHeight = 0, stun = 0, skillTime = 0;
    float boostTime = 0, glideStartHeight = 0;
    float charge = 25, finishTime = -1; // Final classification time, including penalties.
    std::array<float, 3> splits{{-1, -1, -1}}; // Cumulative times at remaining 800 / 400 / 0 m.
    int nextHurdle = 0, nextFeather = 0;
    int jumps = 0, hits = 0, feathers = 0, skillUses = 0, saves = 0;
    bool shield = false;
    bool exhausted = false, sprinting = false;
    bool aiSprint = true;
};
struct Race {
    Phase phase = Phase::Menu;
    int selected = 0, difficulty = 0;
    int course = 0;
    Session session = Session::Single;
    bool paused = false;
    uint32_t seed = 20261003;
    float clock = 0, countdown = 3, finishWait = 0;
    double startElapsed = 0;
    float lightsHold = 0;
    float length = 1200;
    std::array<Rider, RiderCount> riders{}; // Player is always index zero.
    std::vector<float> hurdles, feathers;
    std::wstring notice;
    float noticeTime = 0;

    void start(int character, int level, uint32_t courseSeed = 20261003, int track = 0,
               Session event = Session::Single, std::array<int,RiderCount> grid = {{0,1,2,3,4}}, uint32_t startSeed = 0);
    void tick(float dt, const Input& input);
    void togglePause();
    void returnToMenu();
    bool inTailwind(float distance) const;
    const CourseZone* zoneAt(float distance) const;
    float splitDistance(int section) const;
    float nextHurdleDistance(int index = 0) const;
    float jumpDuration(int character) const;
    bool skillReady(int index = 0) const;
    bool skillRecommended(int index = 0) const;
    std::wstring skillStatus(int index = 0) const;
    float sectionTime(int index, int section) const;
    std::array<int, RiderCount> standings() const;
    int playerRank() const;
    int score() const;
    void notify(const wchar_t* message);
    int redLights() const;
private:
    void advance(int index, float dt, Input input);
    void activateSkill(int index);
};
Input autopilot(const Race& race); // Deterministic validation/demo controller.
std::wstring formatTime(float seconds);
std::wstring formatSeconds(float seconds);
}
