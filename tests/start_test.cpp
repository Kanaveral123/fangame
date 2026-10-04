#include "season.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace anemoi;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
void untilSignal(Race& race) {
    for(int n=0;n<120*9&&race.phase==Phase::Countdown;n++)race.tick(FixedStep,{});
    require(race.phase==Phase::Racing&&race.clock>=0&&race.clock<=FixedStep,"Signal did not start the clock at lights-out");
}
void checkLightsAndReactions() {
    float firstHold=0;bool holdVaries=false,reactionVaries=false;
    for(uint32_t attempt=1;attempt<=100;attempt++) {
        Race race;race.start(4,1,20261003,0,Session::Feature,{{0,1,2,3,4}},attempt);
        const auto initial=race.riders;
        require(race.lightsHold>=.1f&&race.lightsHold<=3,"All-red hold is outside 0.1-3 seconds");
        if(attempt==1)firstHold=race.lightsHold;else holdVaries|=race.lightsHold!=firstHold;
        for(int i=1;i<RiderCount;i++) {
            require(initial[i].reactionDelay>=.1f&&initial[i].reactionDelay<=.3f,"AI reaction outside 0.1-0.3 seconds");
            reactionVaries|=initial[i].reactionDelay!=initial[1].reactionDelay;
        }
        for(int frame=1;frame<=120*5;frame++) {
            race.tick(FixedStep,{true,false,true});
            require(race.redLights()==frame/120,"Red lights do not illuminate one per second");
            require(race.clock==0&&race.phase==Phase::Countdown,"Clock started before all-red hold");
            for(int i=0;i<RiderCount;i++)require(race.riders[i].distance==initial[i].distance&&!race.riders[i].started&&race.riders[i].charge==25&&race.riders[i].stamina==100,"A waiting horse moved, charged or consumed stamina");
        }
        const auto frozen=race;race.togglePause();race.tick(.1f,{true,true,true});
        require(race.startElapsed==frozen.startElapsed&&race.riders[0].penaltyTime==0,"Pause advances the lights or accepts a false start");
        race.togglePause();untilSignal(race);
        require(race.redLights()==0&&std::abs(race.startElapsed-(5+race.lightsHold))<.00001,"Lights do not go out together after the random hold");
        std::array<bool,RiderCount> seen{};
        for(int frame=0;frame<48;frame++) {
            race.tick(FixedStep,{true,false,true});
            for(int i=1;i<RiderCount;i++) {
                const auto& rider=race.riders[i];
                if(rider.started&&!seen[i]) {
                    require(race.clock+.00001f>=initial[i].reactionDelay&&race.clock-initial[i].reactionDelay<=FixedStep+.0001f,"AI first movement does not match its sampled reaction time");
                    seen[i]=true;
                }
                require(rider.penaltyTime==0,"AI received a false-start penalty");
            }
        }
        for(int i=1;i<RiderCount;i++)require(seen[i],"AI never starts after lights-out");
        require(!race.riders[0].started&&race.riders[0].distance==-40&&race.riders[0].speed==0&&race.riders[0].charge==25,"Player starts without pressing sprint");
        race.tick(FixedStep,{false,true,false});
        require(race.riders[0].started&&race.riders[0].distance>-40&&race.riders[0].penaltyTime==0,"Manual legal start failed");
        const float distance=race.riders[0].distance;
        for(int n=0;n<120;n++)race.tick(FixedStep,{});
        require(race.riders[0].distance>distance+20&&!race.riders[0].sprinting,"Releasing Shift stopped automatic running");
    }
    require(holdVaries&&reactionVaries,"Signal or AI reaction is not random across attempts");
    Race a,b;a.start(1,0,47,2,Session::Feature,{{3,4,2,1,0}},1);b.start(1,0,47,2,Session::Feature,{{3,4,2,1,0}},2);
    require(a.hurdles==b.hurdles&&a.feathers==b.feathers&&a.lightsHold!=b.lightsHold,"Randomizing a restart changes the course layout");
    require(a.riders[0].distance==-30&&a.riders[a.standings()[0]].character==3,"Non-default qualifying order does not set physical positions");
    std::cout<<"PASS 100 light sequences, random holds, reaction windows, physical grid, pause and manual launch\n";
}
void checkFalseStart() {
    Race race;race.start(2,1,47,0,Session::Feature);
    const auto initial=race.riders;
    race.tick(FixedStep,{false,true,false});
    require(race.riders[0].distance>initial[0].distance&&race.riders[0].penaltyTime==5&&race.clock==0,"Early Shift did not move the player with exactly five penalty seconds");
    for(int n=0;n<120;n++)race.tick(FixedStep,{false,true,true});
    require(race.riders[0].penaltyTime==5&&race.riders[0].skillUses==0,"Holding sprint stacks penalties or fires a countdown skill");
    for(int i=1;i<RiderCount;i++)require(race.riders[i].distance==initial[i].distance,"AI followed a false starter before the signal");
    const float distance=race.riders[0].distance;race.tick(FixedStep,{});
    require(race.riders[0].distance>distance,"Early starter stops automatically when Shift is released");
    for(int n=0;n<120*120&&race.phase!=Phase::Results;n++)race.tick(FixedStep,autopilot(race));
    require(race.phase==Phase::Results&&std::abs(race.riders[0].finishTime-race.riders[0].splits[2]-5)<.0001f,"Final result did not add exactly five seconds");
    race.start(2,1,47,0,Session::Feature);
    require(!race.riders[0].started&&race.riders[0].penaltyTime==0&&race.riders[0].distance==-20&&race.clock==0&&race.startElapsed==0,"Restart retained launch or penalty state");

    // A single update straddling the signal must count only its post-signal part.
    race.startElapsed=5.0+race.lightsHold-.025;
    race.tick(.1f,{false,true,false});
    require(std::abs(race.clock-.075f)<.00001f&&race.riders[0].penaltyTime==5&&race.redLights()==0,"Boundary update loses timing or misses early input");
    for(int i=1;i<RiderCount;i++)require(!race.riders[i].started,"AI reaction ran during the pre-signal part of an update");
    Race legal;legal.start(0,0,47,0,Session::Feature);
    legal.startElapsed=5.0+legal.lightsHold-.025;legal.tick(.1f,{});legal.tick(FixedStep,{false,true,false});
    require(legal.riders[0].started&&legal.riders[0].penaltyTime==0,"First post-signal input was penalized");
    std::cout<<"PASS false-start movement, one-time penalty, finish adjustment, restart and signal boundary\n";
}
void checkScoringAndReset() {
    Race race;race.start(0,0,47,0,Session::Feature);race.phase=Phase::Racing;race.clock=30;
    race.hurdles.clear();race.feathers.clear();
    for(int i=0;i<RiderCount;i++) {
        auto& rider=race.riders[i];rider.started=true;rider.distance=race.length-.2f*(i+1);
        rider.speed=39;rider.splits[0]=10;rider.splits[1]=20;
    }
    race.riders[0].penaltyTime=5;race.tick(.1f,{});
    require(race.phase==Phase::Results&&race.riders[0].splits[2]<race.riders[1].splits[2]&&race.playerRank()==5,"Penalty does not change the finish order");
    Season season;season.begin(0,0);season.completed=2;
    require(season.record(race)&&season.points()[0]==2&&season.history[2].times[0]==race.riders[0].finishTime,"Season scoring did not use the penalized result");
    Season resumed;require(resumed.deserialize(season.serialize())&&resumed.points()==season.points(),"Penalty-adjusted standings do not survive save/load");
    const auto previous=season.serialize();require(!season.begin(1,1)&&season.serialize()==previous,"Unconfirmed reset overwrites an active season");
    require(season.begin(1,1,true)&&season.completed==0&&season.number==2&&season.character==1&&season.points()==std::array<int,RiderCount>{},"Confirmed restart does not clear the season");
    std::cout<<"PASS penalty-adjusted ranking, points, persistence and explicit season reset\n";
}
void checkLegacyStarts() {
    for(auto session:{Session::Single,Session::Practice,Session::Qualifying}) {
        Race race;race.start(4,0,47,0,session);
        require(race.countdown==3&&race.redLights()==0,"Non-feature countdown changed");
        for(int n=0;n<120*2;n++)race.tick(FixedStep,{true,true,true});
        for(const auto& rider:race.riders)require(rider.distance==0&&rider.penaltyTime==0,"Non-feature accepts a false start");
        for(int n=0;n<120*2;n++)race.tick(FixedStep,{});
        for(const auto& rider:race.riders)require(rider.distance>0&&rider.started&&rider.penaltyTime==0,"Non-feature no-input auto-start changed");
    }
    std::cout<<"PASS unchanged single, practice and qualifying starts\n";
}
int main() {
    try {checkLightsAndReactions();checkFalseStart();checkScoringAndReset();checkLegacyStarts();std::cout<<"ALL START TESTS PASSED\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
