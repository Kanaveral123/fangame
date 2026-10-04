#include "season.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>

using namespace anemoi;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
void finishRace(Race& race,bool controlled=true) {
    for(int frame=0;frame<120*120&&race.phase!=Phase::Results;frame++)race.tick(FixedStep,controlled?autopilot(race):Input{});
    require(race.phase==Phase::Results,"Session never completed");
    require(race.riders[0].finishTime>0,"Player did not finish");
    for(int i=0;i<RiderCount;i++) {
        const auto& rider=race.riders[i];require(rider.finishTime>0,"An AI did not finish");
        require(std::abs(race.sectionTime(i,0)+race.sectionTime(i,1)+race.sectionTime(i,2)+rider.penaltyTime-rider.finishTime)<.0002f,"Map sectionals plus penalty do not sum to final time");
        require(rider.stamina>=0&&rider.stamina<=100&&rider.charge>=0&&rider.charge<=100,"Map effect violates resource bounds");
    }
}
void checkSeasonSelection() {
    Season season;
    require(season.deserialize("ANEMOI_SEASON 1\n1 7 2 1 1\n31 32 33 34 35\n0 1 2 3 4\n"),"Existing version-one season cannot be loaded");
    require(season.number==7&&season.completed==1&&!season.selecting&&season.history[0].times[2]==33,"Legacy progress changed during migration");
    require(season.prepareNext()&&season.active&&season.selecting&&season.number==8&&season.completed==0,"Mid-season replacement does not enter next-season selection");
    require(season.points()==std::array<int,RiderCount>{}&&season.history[0].times[2]<0,"Next season retained old points or results");
    Race race;require(!season.startRace(race),"A season starts before character selection is confirmed");
    const auto pending=season.serialize();
    require(!season.prepareNext()&&season.serialize()==pending,"Pending selection increments the season twice");
    Season restored;require(restored.deserialize(pending)&&restored.selecting&&restored.number==8,"Pending character selection is not persistent");
    require(!restored.deserialize("ANEMOI_SEASON 2\n1 8 2 1 0 2\n")&&restored.serialize()==pending,"Invalid selection state overwrites a valid season");
    require(!restored.deserialize("ANEMOI_SEASON 2\n0 8 2 1 0 1\n"),"Inactive season can have pending selection");
    require(!restored.deserialize("ANEMOI_SEASON 2\n1 8 2 1 1 1\n31 32 33 34 35\n0 1 2 3 4\n"),"Played season can have pending selection");
    require(restored.confirmSelection(4,0)&&restored.number==8&&!restored.selecting&&restored.character==4&&restored.difficulty==0,"Confirming selection does not apply the new identity to the same season");
    require(!restored.confirmSelection(0,1)&&restored.character==4,"Confirmed season remains editable");
    require(restored.startRace(race)&&race.selected==4&&race.difficulty==0&&race.session==Session::Practice,"Newly selected season cannot start its first practice");
    std::cout<<"PASS legacy saves, next-season selection, persistence, validation and new partner lock\n";
}
void recordFixture(Season& season,int sessions) {
    for(int n=0;n<sessions;n++) {
        Race race;require(season.startRace(race),"Cannot launch history fixture");race.phase=Phase::Results;
        for(auto& rider:race.riders)rider.finishTime=30.f+(rider.character+season.number)%RiderCount;
        require(season.record(race),"Cannot record history fixture");
    }
}
void checkSeasonHistory() {
    Season season;require(season.pastSeasonCount()==0&&season.pastSeason(0)==nullptr,"Empty history invents a record");
    season.begin(2,1);recordFixture(season,SeasonSessions);
    const auto original=static_cast<const SeasonRecord&>(season);
    require(season.pastSeasonCount()==1&&season.pastSeason(0)->number==1,"Completed current season is missing before next season starts");
    require(season.prepareNext()&&season.pastSeasonCount()==1&&season.past.size()==1,"New season duplicates or discards the completed season");
    require(season.pastSeason(0)->points()==original.points()&&season.pastSeason(0)->standings()==original.standings()&&season.pastSeason(0)->character==2,"Archived totals, tie-break ranking or player identity changed");
    require(season.confirmSelection(4,0),"Cannot select second season partner");recordFixture(season,6);
    const auto partial=static_cast<const SeasonRecord&>(season);
    require(season.pastSeasonCount()==1,"Ongoing season is listed as historical");
    require(season.prepareNext()&&season.pastSeasonCount()==2,"Early restart did not archive the ended season");
    require(season.pastSeason(0)->number==2&&season.pastSeason(0)->completed==6&&season.pastSeason(0)->points()==partial.points(),"Newest history entry loses partial results");
    require(season.pastSeason(1)->number==1&&season.pastSeason(1)->completed==15&&season.pastSeason(1)->roundPoints(4,0)==original.roundPoints(4,0),"Older season changed after another restart");
    require(season.pastSeason(-1)==nullptr&&season.pastSeason(2)==nullptr,"History bounds are unsafe");
    Season loaded;const auto saved=season.serialize();
    require(loaded.deserialize(saved)&&loaded.serialize()==saved&&loaded.pastSeasonCount()==2&&loaded.selecting,"History does not survive save/load together with pending selection");
    for(const auto& invalid:{
        "ANEMOI_SEASON 3\n1 3 4 0 0 1\nHISTORY 2\n1 2 1 0\n1 4 0 0\n",
        "ANEMOI_SEASON 3\n1 3 4 0 0 1\nHISTORY 1\n3 2 1 0\n",
        "ANEMOI_SEASON 3\n1 3 4 0 0 1\nHISTORY -1\n",
        "ANEMOI_SEASON 3\n1 3 4 0 0 1\nHISTORY 1\n1 2 1 1\n30 31 32 33 34\n0 0 2 3 4\n"})
        require(!loaded.deserialize(invalid)&&loaded.serialize()==saved,"Malformed historical record overwrites valid history");
    require(!loaded.deserialize(saved.substr(0,saved.size()-20))&&loaded.serialize()==saved,"Truncated historical data is accepted");
    require(loaded.deserialize("ANEMOI_SEASON 2\n1 9 1 0 0 1\n")&&loaded.pastSeasonCount()==0&&loaded.selecting,"Version-two migration invents or rejects old history");
    std::cout<<"PASS completed and partial season history, newest-first browsing, score preservation, persistence and invalid archives\n";
}
int main() {
    try {
        checkSeasonSelection();
        checkSeasonHistory();
        for(int map=0;map<CourseCount;map++) {
            const auto& course=Courses[map];require(course.hurdles.size()==static_cast<size_t>(8+map),"Unexpected course hurdle count");
            require(std::is_sorted(course.hurdles.begin(),course.hurdles.end()),"Hurdles are not in travel order");
            for(const auto& zone:course.zones)require(zone.begin>=0&&zone.end<=course.length&&zone.begin<zone.end,"Invalid terrain zone");
            Race passive;passive.start(0,1,20261003,map);finishRace(passive,false);
            require(passive.riders[0].hits==static_cast<int>(course.hurdles.size()),"Map collision path not exercised");
            // Verify that every obstacle can be cleared by ordinary inputs,
            // independently of a glide or shield compensating for course timing.
            for(int player=0;player<RiderCount;player++)for(int level=0;level<2;level++) {
                Race jumping;jumping.start(player,level,20261003,map);
                for(int frame=0;frame<120*120&&jumping.phase!=Phase::Results;frame++) {
                    auto in=autopilot(jumping);in.skill=false;jumping.tick(FixedStep,in);
                }
                require(jumping.phase==Phase::Results,"Jump-only map run never completed");
                require(jumping.riders[0].hits==0&&jumping.riders[0].jumps==static_cast<int>(course.hurdles.size()),"An obstacle cannot be cleared without a special skill");
                require(jumping.riders[0].feathers==static_cast<int>(course.hurdles.size()),"A map feather is unreachable");
            }
        }
        int sessionCount=0;
        for(int character=0;character<RiderCount;character++)for(int difficulty=0;difficulty<2;difficulty++) {
            Season season;require(season.begin(character,difficulty),"Cannot begin season");
            require(!season.begin((character+1)%5,difficulty),"An active season can be overwritten");
            for(int event=0;event<SeasonSessions;event++) {
                require(season.completed==event&&season.round()==event/3,"Season skipped a station");
                const auto priorPoints=season.points();Race race;require(season.startRace(race),"Cannot launch expected session");
                require(static_cast<int>(race.session)==1+event%3&&race.course==event/3,"Practice / qualifying / feature order broken");
                require(race.selected==character&&race.difficulty==difficulty,"Season character or difficulty changed");
                Race restarted;require(season.startRace(restarted)&&restarted.hurdles==race.hurdles,"Retry changes the course layout");
                require(!season.record(race),"Unfinished session was scored");
                if(race.session==Session::Feature) {
                    const auto grid=season.grid();
                    const auto order=race.standings();
                    for(int place=0;place<RiderCount;place++)
                        require(race.riders[order[place]].character==grid[place],"Pre-race standings do not match the qualifying grid");
                    for(const auto& rider:race.riders) {
                        const int position=static_cast<int>(std::find(grid.begin(),grid.end(),rider.character)-grid.begin());
                        require(rider.distance==-position*GridSpacing&&rider.gridSlot==position&&!rider.started,"Qualifying order does not control physical starting positions");
                    }
                }
                finishRace(race);
                const auto& player=race.riders[0];
                require(player.hits+player.jumps+player.saves==static_cast<int>(race.hurdles.size()),"A course obstacle was silently skipped");
                require(season.record(race),"Valid session was rejected");
                require(!season.record(race),"Same completed session awarded points twice");
                if(event%3!=2)require(season.points()==priorPoints,"Practice or qualifying awarded championship points");
                else {
                    const auto order=race.standings();
                    for(int place=0;place<RiderCount;place++)require(season.roundPoints(event/3,race.riders[order[place]].character)==RacePoints[place],"Points are assigned to rider index instead of character");
                }
                Season resumed;require(resumed.deserialize(season.serialize()),"Cannot resume saved season");
                require(resumed.serialize()==season.serialize()&&resumed.points()==season.points()&&resumed.grid()==season.grid(),"Resume changes history, scoring or grid");
                season=resumed;++sessionCount;
            }
            require(season.finished()&&season.playerRank()>=1&&season.playerRank()<=5,"Final championship is unavailable");
            Race extra;require(!season.startRace(extra),"A sixth station was created");
            const auto points=season.points();require(std::accumulate(points.begin(),points.end(),0)==150,"Five-station championship points total is wrong");
            const auto old=season.serialize();require(!season.deserialize(old.substr(0,old.size()/2))&&season.serialize()==old,"Truncated save overwrites good state");
            require(!season.deserialize("ANEMOI_SEASON 1\n1 1 0 0 16\n"),"Out-of-range progress is accepted");
            require(!season.deserialize("ANEMOI_SEASON 1\n1 1 0 0 1\n1 2 3 4 5\n0 0 2 3 4\n"),"Duplicate grid identity is accepted");
            require(!season.deserialize("ANEMOI_SEASON 1\n1 1 0 0 0\ntrailing"),"Malformed trailing data is accepted");
            require(season.begin((character+1)%5,difficulty)&&season.completed==0&&season.number==2,"New season did not reset history");
            std::cout<<"PASS complete season character="<<character<<" difficulty="<<difficulty<<"; 15 sessions, persistence, grid, scoring, champion\n";
        }
        Season dnf;dnf.begin(0,0);
        for(int event=0;event<3;event++) {
            Race race;dnf.startRace(race);race.phase=Phase::Results;
            for(auto& rider:race.riders)rider.finishTime=30.f+rider.character;
            race.riders[0].finishTime=-1;require(dnf.record(race),"DNF session cannot be recorded");
        }
        require(dnf.roundPoints(0,0)==0&&dnf.points()[1]==10,"DNF incorrectly earns points or displaces winner");
        std::cout<<"ALL SEASON TESTS PASSED ("<<sessionCount<<" sessions, 50 jump-only map runs, 5 passive maps, physical grid, DNF, invalid saves)\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
