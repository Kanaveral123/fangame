#pragma once
#include "game.hpp"
#include <cstddef>

namespace anemoi {
constexpr int SeasonSessions = CourseCount * 3;
constexpr std::size_t SeasonSaveLimit = 128u * 1024u * 1024u;
inline constexpr std::array<int,RiderCount> RacePoints{{10,8,6,4,2}};
struct SessionResult {
    std::array<float,RiderCount> times{{-1,-1,-1,-1,-1}}; // Keyed by character, not rider index.
    std::array<int,RiderCount> order{{0,1,2,3,4}};
};
struct SeasonRecord {
    int number=1,character=0,difficulty=0,completed=0;
    std::array<SessionResult,SeasonSessions> history{};

    std::array<int,RiderCount> points() const;
    std::array<int,RiderCount> standings() const;
    int roundPoints(int roundIndex,int player) const;
    int wins(int player) const;
    int playerRank() const;
};
struct Season : SeasonRecord {
    bool active=false;
    bool selecting=false; // Next season exists, but its partner and difficulty are not locked yet.
    std::vector<SeasonRecord> past; // Oldest first; saved atomically with the current season.

    bool finished() const {return active&&completed==SeasonSessions;}
    int round() const;
    Session nextSession() const;
    bool begin(int player,int level,bool replaceActive=false);
    bool prepareNext();
    bool confirmSelection(int player,int level);
    bool startRace(Race& race,uint32_t startSeed=0) const;
    bool record(const Race& race); // Commits once and advances the cursor.
    std::array<int,RiderCount> grid() const;
    int pastSeasonCount() const;
    const SeasonRecord* pastSeason(int newestFirstIndex) const;
    std::string serialize() const;
    bool deserialize(const std::string& text); // Transactional; leaves *this untouched on failure.
};
}
