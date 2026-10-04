#include "season.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace anemoi {
namespace {
bool validResult(const SessionResult& result) {
    auto sorted=result.order;std::sort(sorted.begin(),sorted.end());
    if(sorted!=std::array<int,RiderCount>{{0,1,2,3,4}})return false;
    float previous=0;bool dnf=false;
    for(int character:result.order) {
        const float time=result.times[character];
        if(!std::isfinite(time)||(time!=-1&&(time<=0||time>=3600)))return false;
        if(time<0)dnf=true;
        else {if(dnf||time<previous)return false;previous=time;}
    }
    return true;
}
bool validRecord(const SeasonRecord& season) {
    return season.number>=1&&season.number<=100000&&season.character>=0&&season.character<RiderCount&&
           season.difficulty>=0&&season.difficulty<=1&&season.completed>=0&&season.completed<=SeasonSessions;
}
void writeResults(std::ostream& out,const SeasonRecord& season) {
    for(int i=0;i<season.completed;i++) {
        for(float time:season.history[i].times)out<<time<<' ';out<<'\n';
        for(int player:season.history[i].order)out<<player<<' ';out<<'\n';
    }
}
bool readResults(std::istream& in,SeasonRecord& season) {
    for(int i=0;i<season.completed;i++) {
        for(float& time:season.history[i].times)if(!(in>>time))return false;
        for(int& player:season.history[i].order)if(!(in>>player))return false;
        if(!validResult(season.history[i]))return false;
    }
    return true;
}
}
int Season::round() const {return std::clamp(completed/3,0,CourseCount-1);}
Session Season::nextSession() const {return static_cast<Session>(1+completed%3);}
bool Season::begin(int player,int level,bool replaceActive) {
    if(active&&!finished()&&!replaceActive)return false;
    if(active&&number>=100000)return false;
    const int nextNumber=active?number+1:1;
    auto previous=std::move(past);
    if(active&&!selecting)previous.push_back(static_cast<const SeasonRecord&>(*this));
    *this=Season{};active=true;number=nextNumber;
    past=std::move(previous);
    character=std::clamp(player,0,RiderCount-1);difficulty=std::clamp(level,0,1);return true;
}
bool Season::prepareNext() {
    if(!active||selecting||number>=100000)return false;
    const int player=character,level=difficulty;
    if(!begin(player,level,true))return false;
    selecting=true;return true;
}
bool Season::confirmSelection(int player,int level) {
    if(!active||!selecting||completed!=0)return false;
    character=std::clamp(player,0,RiderCount-1);difficulty=std::clamp(level,0,1);
    selecting=false;return true;
}
std::array<int,RiderCount> Season::grid() const {
    if(active&&!finished()&&nextSession()==Session::Feature)return history[round()*3+1].order;
    return {{0,1,2,3,4}};
}
bool Season::startRace(Race& race,uint32_t startSeed) const {
    if(!active||selecting||finished())return false;
    race.start(character,difficulty,20261003u+static_cast<uint32_t>(round()*997),round(),nextSession(),grid(),startSeed);return true;
}
bool Season::record(const Race& race) {
    if(!active||selecting||finished()||race.phase!=Phase::Results||race.course!=round()||race.session!=nextSession()||
       race.selected!=character||race.difficulty!=difficulty)return false;
    SessionResult result;std::array<bool,RiderCount> seen{};
    for(const auto& rider:race.riders) {
        if(rider.character<0||rider.character>=RiderCount||seen[rider.character])return false;
        seen[rider.character]=true;result.times[rider.character]=rider.finishTime;
    }
    const auto order=race.standings();
    for(int i=0;i<RiderCount;i++)result.order[i]=race.riders[order[i]].character;
    if(!validResult(result))return false;
    history[completed++]=result;return true;
}
int SeasonRecord::roundPoints(int index,int player) const {
    if(index<0||index>=CourseCount||player<0||player>=RiderCount||index*3+2>=completed)return 0;
    const auto& result=history[index*3+2];
    if(result.times[player]<0)return 0;
    const auto pos=std::find(result.order.begin(),result.order.end(),player);
    return RacePoints[static_cast<size_t>(pos-result.order.begin())];
}
std::array<int,RiderCount> SeasonRecord::points() const {
    std::array<int,RiderCount> total{};
    for(int roundIndex=0;roundIndex<CourseCount;roundIndex++)for(int player=0;player<RiderCount;player++)total[player]+=roundPoints(roundIndex,player);
    return total;
}
int SeasonRecord::wins(int player) const {
    int count=0;for(int roundIndex=0;roundIndex<CourseCount;roundIndex++)if(roundPoints(roundIndex,player)==RacePoints[0])++count;return count;
}
std::array<int,RiderCount> SeasonRecord::standings() const {
    std::array<int,RiderCount> order{{0,1,2,3,4}};const auto total=points();
    std::array<std::array<int,RiderCount>,RiderCount> counts{};std::array<float,RiderCount> times{};
    for(int index=2;index<completed;index+=3) {
        for(int rank=0;rank<RiderCount;rank++) {
            const int player=history[index].order[rank];const float time=history[index].times[player];
            if(time>=0)++counts[player][rank];times[player]+=time>=0?time:3600.f;
        }
    }
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){
        if(total[a]!=total[b])return total[a]>total[b];
        if(counts[a]!=counts[b])return counts[a]>counts[b];
        return times[a]<times[b];
    });return order;
}
int SeasonRecord::playerRank() const {const auto order=standings();return static_cast<int>(std::find(order.begin(),order.end(),character)-order.begin())+1;}
int Season::pastSeasonCount() const {return static_cast<int>(past.size())+(finished()?1:0);}
const SeasonRecord* Season::pastSeason(int index) const {
    if(index<0||index>=pastSeasonCount())return nullptr;
    if(finished()) {if(index==0)return this;--index;}
    return &past[past.size()-1-static_cast<size_t>(index)];
}
std::string Season::serialize() const {
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);
    out<<"ANEMOI_SEASON 3\n"<<active<<' '<<number<<' '<<character<<' '<<difficulty<<' '<<completed<<' '<<selecting<<'\n';
    writeResults(out,*this);
    out<<"HISTORY "<<past.size()<<'\n';
    for(const auto& old:past) {
        out<<old.number<<' '<<old.character<<' '<<old.difficulty<<' '<<old.completed<<'\n';
        writeResults(out,old);
    }
    return out.str();
}
bool Season::deserialize(const std::string& text) {
    if(text.size()>SeasonSaveLimit)return false;
    std::istringstream in(text);in.imbue(std::locale::classic());Season candidate;std::string magic;int version=0,enabled=0;
    if(!(in>>magic>>version)||magic!="ANEMOI_SEASON"||version<1||version>3)return false;
    if(!(in>>enabled>>candidate.number>>candidate.character>>candidate.difficulty>>candidate.completed))return false;
    int selecting=0;if(version>=2&&(!(in>>selecting)||selecting<0||selecting>1))return false;
    candidate.selecting=selecting!=0;
    if(enabled<0||enabled>1||!validRecord(candidate))return false;
    candidate.active=enabled!=0;if(!candidate.active&&candidate.completed!=0)return false;
    if(candidate.selecting&&(!candidate.active||candidate.completed!=0))return false;
    if(!readResults(in,candidate))return false;
    if(version==3) {
        std::string tag;int count=0,lastNumber=0;
        if(!(in>>tag>>count)||tag!="HISTORY"||count<0||count>=candidate.number||(!candidate.active&&count>0))return false;
        for(int i=0;i<count;i++) {
            SeasonRecord old;
            if(!(in>>old.number>>old.character>>old.difficulty>>old.completed)||!validRecord(old)||
               old.number<=lastNumber||old.number>=candidate.number||!readResults(in,old))return false;
            lastNumber=old.number;candidate.past.push_back(old);
        }
    }
    in>>std::ws;if(!in.eof())return false;
    *this=candidate;return true;
}
}
