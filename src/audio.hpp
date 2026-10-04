#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace anemoi {
// An original, quiet pentatonic loop, synthesized once. No external music assets.
class Audio {
    std::vector<uint8_t> wav_;
    bool playing_ = false;
public:
    ~Audio() { setPlaying(false); }
    void setPlaying(bool enabled) {
        if(enabled==playing_)return;
        if(enabled) {
            if(wav_.empty())build();
            playing_=PlaySoundW(reinterpret_cast<LPCWSTR>(wav_.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_LOOP|SND_NODEFAULT)!=FALSE;
        } else {PlaySoundW(nullptr,nullptr,0);playing_=false;}
    }
private:
    void build() {
        constexpr int rate=22050;
        constexpr double beat=.25;
        constexpr int steps=64;
        const int samples=static_cast<int>(rate*beat*steps);
        wav_.resize(44+samples*2);
        auto put=[&](int p,uint32_t n,int bytes){for(int i=0;i<bytes;i++)wav_[p+i]=static_cast<uint8_t>(n>>(8*i));};
        std::memcpy(wav_.data(),"RIFF",4);put(4,36+samples*2,4);std::memcpy(wav_.data()+8,"WAVEfmt ",8);
        put(16,16,4);put(20,1,2);put(22,1,2);put(24,rate,4);put(28,rate*2,4);put(32,2,2);put(34,16,2);
        std::memcpy(wav_.data()+36,"data",4);put(40,samples*2,4);
        constexpr int melody[32]={76,0,79,81,79,0,76,74,72,0,74,76,79,0,76,0,74,0,76,79,76,0,74,72,69,0,72,74,72,0,0,0};
        constexpr int bass[8]={48,48,53,53,57,57,55,55};
        for(int i=0;i<samples;i++) {
            const double t=i/static_cast<double>(rate),local=std::fmod(t,beat);
            const int step=static_cast<int>(t/beat),note=melody[step%32];
            double value=0;
            if(note) {
                const double freq=440*std::pow(2.,(note-69)/12.);
                const double envelope=std::min(local/.012,1.)*std::exp(-local*12.);
                value+=(std::sin(t*freq*6.2831853)+.22*std::sin(t*freq*12.5663706))*envelope*.065;
            }
            const double chordLocal=std::fmod(t,1.);
            const double base=440*std::pow(2.,(bass[(step/8)%8]-69)/12.);
            const double chordEnv=std::min(chordLocal/.03,1.)*std::exp(-chordLocal*3.);
            value+=std::sin(t*base*6.2831853)*chordEnv*.045;
            value+=std::sin(t*base*1.5*6.2831853)*chordEnv*.018;
            const double fade=std::min({1.,t/.015,(beat*steps-t)/.04});
            const int16_t sample=static_cast<int16_t>(std::clamp(value*fade,-1.,1.)*32767);
            put(44+i*2,static_cast<uint16_t>(sample),2);
        }
    }
};
}
