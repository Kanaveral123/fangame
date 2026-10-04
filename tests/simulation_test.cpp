#include "game.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace anemoi;
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
void runToEnd(Race& race,bool skilled,float dt=FixedStep) {
    for(int i=0;i<static_cast<int>(100/dt)&&race.phase!=Phase::Results;i++)race.tick(dt,skilled?autopilot(race):Input{});
    require(race.phase==Phase::Results,"Race did not complete");
}
Race emptyTrack(int character) {
    Race r;r.start(character,1);r.phase=Phase::Racing;r.hurdles.clear();r.feathers.clear();
    r.riders[0].charge=100;r.riders[0].speed=39;return r;
}
void advanceFor(Race& race,float duration,Input input) {
    for(int i=0;i<static_cast<int>(duration/FixedStep+.5f);++i)race.tick(FixedStep,input);
}
void checkSkills() {
    for(int ch=0;ch<RiderCount;ch++) {
        Race locked;locked.start(ch,0);locked.riders[0].charge=100;
        locked.tick(FixedStep,{false,true,true});require(locked.riders[0].skillUses==0,"Skill can fire during countdown");
        locked.phase=Phase::Racing;locked.paused=true;locked.tick(FixedStep,{false,true,true});
        require(locked.riders[0].skillUses==0,"Skill can fire while paused");
        locked.paused=false;locked.riders[0].charge=99;locked.tick(FixedStep,{false,true,true});
        require(locked.riders[0].skillUses==0,"Skill can fire without enough charge");
        locked.riders[0].charge=100;locked.tick(FixedStep,{false,true,true});
        require(locked.riders[0].skillUses==1&&locked.riders[0].charge<1,"Ready skill was not consumed once");
        locked.riders[0].charge=100;locked.tick(FixedStep,{false,true,true});
        require(locked.riders[0].skillUses==1&&locked.riders[0].charge==100,"Active skill consumed charge again");
        const float duration=locked.riders[0].skillTime;locked.paused=true;locked.tick(.1f,{});
        require(locked.riders[0].skillTime==duration,"Pause consumes skill duration");
        locked.start(ch,0);require(locked.riders[0].skillTime==0&&!locked.riders[0].shield&&locked.riders[0].splits[0]<0,"Restart left skill or sectional state");
    }
    Race supply=emptyTrack(0),withoutSupply=emptyTrack(0);
    supply.riders[0].stamina=withoutSupply.riders[0].stamina=45;
    advanceFor(supply,1,{false,true,true});advanceFor(withoutSupply,1,{false,true,false});
    require(supply.riders[0].stamina>withoutSupply.riders[0].stamina+40,"Supply did not refill and reduce sprint cost");
    require(supply.riders[0].distance>withoutSupply.riders[0].distance+3,"Supply did not provide its sustained pace benefit");

    Race courier=emptyTrack(1);courier.hurdles={25,83,141,199};courier.feathers=courier.hurdles;
    courier.tick(FixedStep,{false,true,true});
    require(courier.riders[0].skillTime==3,"Courier boost was not shortened to three seconds");
    for(int i=0;i<120*8;i++) {
        Input input=autopilot(courier);input.skill=false;courier.tick(FixedStep,input);
        if(i==120*3+1)require(courier.riders[0].skillTime==0,"Hurdles still extend courier's three-second boost");
    }
    require(courier.riders[0].jumps==4&&courier.riders[0].skillTime==0,"Courier hurdle coverage or expiration is wrong");
    for(bool active:{false,true}) {
        Race reward=emptyTrack(1);auto& rider=reward.riders[0];
        rider.charge=0;rider.skillTime=active?2.f:0.f;rider.jumpTime=.4f;
        reward.hurdles={.1f};reward.feathers={.1f};reward.tick(FixedStep,{});
        require(rider.jumps==1&&rider.feathers==1&&std::abs(rider.charge-(25+1.3f*FixedStep))<.001f,"Courier still receives an extra hurdle reward");
    }

    Race curious=emptyTrack(2),withoutCuriosity=emptyTrack(2);curious.feathers={20};withoutCuriosity.feathers={20};
    curious.riders[0].stamina=withoutCuriosity.riders[0].stamina=40;
    advanceFor(curious,1,{false,true,true});advanceFor(withoutCuriosity,1,{false,true,false});
    require(curious.riders[0].feathers==1&&withoutCuriosity.riders[0].feathers==0,"Curiosity does not collect a feather from the ground");
    require(curious.riders[0].charge>=20&&curious.riders[0].stamina>withoutCuriosity.riders[0].stamina+9,"Curiosity did not grant doubled pickup rewards");
    require(curious.riders[0].boostTime>0&&curious.riders[0].jumpHeight==0,"Curiosity burst or grounded pickup is wrong");

    Race support=emptyTrack(3);auto& protectedRider=support.riders[0];
    protectedRider.stun=1;protectedRider.stamina=10;protectedRider.exhausted=true;support.hurdles={5,25};
    support.tick(FixedStep,{false,true,true});
    require(protectedRider.stun==0&&!protectedRider.exhausted&&protectedRider.stamina>27,"Support did not recover a tired, stunned rider");
    while(protectedRider.nextHurdle<1)support.tick(FixedStep,{});
    require(protectedRider.saves==1&&protectedRider.hits==0&&protectedRider.stun==0,"Support shield did not prevent the first collision");
    while(protectedRider.nextHurdle<2)support.tick(FixedStep,{});
    require(protectedRider.saves==1&&protectedRider.hits==1&&protectedRider.stun>0,"Support incorrectly protects multiple collisions");
    require(protectedRider.jumps==0,"A protected collision was misreported as a successful jump");

    Race fastGlide=emptyTrack(4),ordinaryGlide=emptyTrack(4);
    advanceFor(fastGlide,1,{false,false,true});advanceFor(ordinaryGlide,1,{false,false,false});
    require(fastGlide.riders[0].speed-ordinaryGlide.riders[0].speed>14.5f&&fastGlide.riders[0].speed-ordinaryGlide.riders[0].speed<15.01f,"Aino glide does not provide its increased 15 m/s speed bonus");
    Race glide=emptyTrack(4);glide.hurdles={60,135};glide.feathers=glide.hurdles;
    advanceFor(glide,4.9f,{false,true,true});
    require(glide.riders[0].jumps==2&&glide.riders[0].feathers==2&&glide.riders[0].hits==0,"Glide cannot cross consecutive obstacles without jumping");
    require(glide.riders[0].skillTime==0&&glide.riders[0].jumpHeight==0,"Glide did not land after expiration");
    Race lateGlide=emptyTrack(4);lateGlide.hurdles={.2f};lateGlide.tick(FixedStep,{false,false,true});
    require(lateGlide.riders[0].hits==1,"Glide incorrectly grants instant hurdle immunity during takeoff");
    std::cout<<"PASS five distinct skills, timing gates, charging, courier balance, pickups, one-hit shield, glide and landing\n";
}
int main() {
    try {
        checkSkills();
        int races=0;
        for(int difficulty=0;difficulty<2;difficulty++)for(int ch=0;ch<5;ch++)for(uint32_t seed:{1u,20261003u,47u}) {
            Race skilled;skilled.start(ch,difficulty,seed);runToEnd(skilled,true);
            require(skilled.riders[0].jumps==8&&skilled.riders[0].hits==0,"A hurdle cannot be cleared");
            require(skilled.riders[0].skillUses>=1,"Character skill did not activate");
            require(skilled.riders[0].feathers==8,"Feathers cannot be collected");
            require(skilled.riders[0].finishTime>20&&skilled.riders[0].finishTime<70,"Unreasonable finish time");
            for(int j=0;j<RiderCount;j++) {
                const auto& r=skilled.riders[j];require(r.stamina>=0&&r.stamina<=100,"Stamina bounds violated");require(r.finishTime>=0,"AI did not finish");
                require(r.splits[0]>0&&r.splits[1]>r.splits[0]&&r.splits[2]>r.splits[1],"Sectional crossing times are not ordered");
                const float sum=skilled.sectionTime(j,0)+skilled.sectionTime(j,1)+skilled.sectionTime(j,2);
                require(std::abs(sum-r.finishTime)<.0001f,"Sectionals do not sum to the runner's finish time");
            }
            Race passive;passive.start(ch,difficulty,seed);runToEnd(passive,false);
            require(skilled.riders[0].finishTime+5<passive.riders[0].finishTime,"Player inputs do not materially improve result");
            require(passive.riders[0].hits==8,"Collision path is not exercised");
            const auto order=skilled.standings();
            for(int j=1;j<5;j++)require(skilled.riders[order[j-1]].finishTime<=skilled.riders[order[j]].finishTime,"Finish ranking is inconsistent");
            std::cout<<"PASS character="<<ch<<" difficulty="<<difficulty<<" seed="<<seed<<" skilled="<<skilled.riders[0].finishTime<<" passive="<<passive.riders[0].finishTime<<" rank="<<skilled.playerRank()<<'\n';races+=2;
        }
        Race pause;pause.start(0,0);pause.togglePause();pause.tick(.1f,{});require(pause.countdown==3,"Paused countdown advanced");
        pause.togglePause();for(int i=0;i<500;i++)pause.tick(FixedStep,{});pause.togglePause();
        const float clock=pause.clock,distance=pause.riders[0].distance;pause.tick(.1f,{true,true,true});
        require(pause.clock==clock&&pause.riders[0].distance==distance,"Paused race advanced");
        Race tired;tired.start(0,0);for(int i=0;i<120*12;i++)tired.tick(FixedStep,{false,true,false});
        require(tired.riders[0].stamina>=0&&tired.riders[0].stamina<=100,"Holding sprint underflows stamina");
        for(int i=0;i<120*8;i++)tired.tick(FixedStep,{});require(!tired.riders[0].exhausted&&tired.riders[0].stamina>90,"Stamina recovery failed");
        Race fixedA,fixedB;fixedA.start(0,0);fixedB.start(0,0);runToEnd(fixedA,true);runToEnd(fixedB,true);
        require(fixedA.riders[0].finishTime==fixedB.riders[0].finishTime,"Simulation is not deterministic");
        fixedA.start(0,0);require(fixedA.clock==0&&fixedA.riders[0].distance==0&&fixedA.riders[0].hits==0,"Restart leaves stale state");
        Race thirty,sixty;thirty.start(2,0);sixty.start(2,0);runToEnd(thirty,true,1.f/30);runToEnd(sixty,true,1.f/60);
        require(std::abs(thirty.riders[0].finishTime-sixty.riders[0].finishTime)<.45,"Simulation is excessively timestep sensitive");
        std::cout<<"PASS pause, recovery, restart, deterministic replay, timestep robustness\n";
        std::cout<<"ALL TESTS PASSED ("<<races<<" full races plus boundary checks)\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
