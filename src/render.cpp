#include "render.hpp"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <utility>

namespace anemoi {
namespace {
constexpr uint32_t Ink=0x294A47, Muted=0x678179, Paper=0xFCF9ED, Cream=0xF3EEDC;
constexpr uint32_t Green=0x4F8F76, Gold=0xDEB35E, White=0xFFFDF3;
COLORREF colorRef(uint32_t c) { return RGB((c>>16)&255,(c>>8)&255,c&255); }
uint32_t blend(uint32_t a,uint32_t b,int t) {
    const auto ch=[&](int s){ return ((((a>>s)&255)*(255-t)+((b>>s)&255)*t)/255)<<s; };
    return ch(16)|ch(8)|ch(0);
}
int imod(int x,int period) { return (x%period+period)%period; }
}
Canvas::Canvas() {
    dc_=CreateCompatibleDC(nullptr);
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=ScreenW; info.bmiHeader.biHeight=-ScreenH;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels_),nullptr,0);
    oldBitmap_=SelectObject(dc_,bitmap_); SetBkMode(dc_,TRANSPARENT);
}
Canvas::~Canvas() {
    GdiFlush();
    if(presentationDC_) {
        if(presentationBitmap_){SelectObject(presentationDC_,previousPresentationBitmap_);DeleteObject(presentationBitmap_);}
        DeleteDC(presentationDC_);
    }
    for (const auto& pair:fonts_) DeleteObject(pair.second);
    SelectObject(dc_,oldBitmap_); DeleteObject(bitmap_); DeleteDC(dc_);
}
HFONT Canvas::font(int size,bool bold) {
    const int key=size*2+(bold?1:0);
    if (!fonts_.count(key)) fonts_[key]=CreateFontW(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
    return fonts_[key];
}
void Canvas::clear(uint32_t c) { GdiFlush(); std::fill(pixels_,pixels_+ScreenW*ScreenH,c); }
void Canvas::rect(int x,int y,int w,int h,uint32_t c) {
    RECT r{x,y,x+w,y+h}; SetDCBrushColor(dc_,colorRef(c)); FillRect(dc_,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
void Canvas::frame(Rect r,uint32_t c,int s) {
    rect(r.x,r.y,r.w,s,c);rect(r.x,r.y+r.h-s,r.w,s,c);rect(r.x,r.y,s,r.h,c);rect(r.x+r.w-s,r.y,s,r.h,c);
}
void Canvas::line(int x1,int y1,int x2,int y2,uint32_t c,int w) {
    HPEN pen=CreatePen(PS_SOLID,w,colorRef(c)); auto old=SelectObject(dc_,pen);
    MoveToEx(dc_,x1,y1,nullptr);LineTo(dc_,x2,y2);SelectObject(dc_,old);DeleteObject(pen);
}
void Canvas::poly(std::initializer_list<POINT> p,uint32_t c) {
    SetDCBrushColor(dc_,colorRef(c));auto oldB=SelectObject(dc_,GetStockObject(DC_BRUSH));
    auto oldP=SelectObject(dc_,GetStockObject(NULL_PEN)); Polygon(dc_,p.begin(),static_cast<int>(p.size()));
    SelectObject(dc_,oldP);SelectObject(dc_,oldB);
}
void Canvas::ellipse(int x,int y,int w,int h,uint32_t c) {
    SetDCBrushColor(dc_,colorRef(c));auto ob=SelectObject(dc_,GetStockObject(DC_BRUSH));
    auto op=SelectObject(dc_,GetStockObject(NULL_PEN));Ellipse(dc_,x,y,x+w,y+h);SelectObject(dc_,op);SelectObject(dc_,ob);
}
void Canvas::text(int x,int y,const std::wstring& s,int size,uint32_t c,bool bold) {
    auto old=SelectObject(dc_,font(size,bold));SetTextColor(dc_,colorRef(c));TextOutW(dc_,x,y,s.c_str(),static_cast<int>(s.size()));SelectObject(dc_,old);
}
void Canvas::center(int x,int y,const std::wstring& s,int size,uint32_t c,bool bold) {
    auto old=SelectObject(dc_,font(size,bold));SIZE sizePx{};GetTextExtentPoint32W(dc_,s.c_str(),static_cast<int>(s.size()),&sizePx);SelectObject(dc_,old);
    text(x-sizePx.cx/2,y,s,size,c,bold);
}
void Canvas::pixelText(int x,int y,const std::string& str,int scale,uint32_t c) {
    static const char* keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-!./";
    static const uint8_t glyphs[][7]={
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
        {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,25,21,19,19,17},{14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},{14,17,19,21,25,17,14},
        {4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},
        {31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
        {0,0,0,31,0,0,0},{4,4,4,4,4,0,4},{0,0,0,0,0,0,4},{1,2,2,4,8,8,16}
    };
    for(char cch:str) {
        const char* p=std::strchr(keys,cch);
        if(p) for(int gy=0;gy<7;gy++) for(int gx=0;gx<5;gx++) if(glyphs[p-keys][gy]&(1<<(4-gx))) rect(x+gx*scale,y+gy*scale,scale,scale,c);
        x+=scale*6;
    }
}
void Canvas::shade(uint32_t c,int a) { GdiFlush();for(int i=0;i<ScreenW*ScreenH;i++)pixels_[i]=blend(pixels_[i],c,a); }
bool Canvas::resizePresentationBuffer(HDC target,int w,int h) {
    if(presentationBitmap_&&w==presentationWidth_&&h==presentationHeight_)return true;
    if(!presentationDC_)presentationDC_=CreateCompatibleDC(target);
    if(!presentationDC_)return false;
    HBITMAP next=CreateCompatibleBitmap(target,w,h);
    if(!next)return false;
    HGDIOBJ previous=SelectObject(presentationDC_,next);
    if(!previous||previous==HGDI_ERROR){DeleteObject(next);return false;}
    if(presentationBitmap_)DeleteObject(presentationBitmap_);
    else previousPresentationBitmap_=previous;
    presentationBitmap_=next;presentationWidth_=w;presentationHeight_=h;
    return true;
}
void Canvas::present(HDC target,int w,int h) {
    if(w<=0||h<=0||!resizePresentationBuffer(target,w,h))return;
    const double scale=std::min(w/static_cast<double>(ScreenW),h/static_cast<double>(ScreenH));
    const int dw=std::max(1,static_cast<int>(ScreenW*scale)),dh=std::max(1,static_cast<int>(ScreenH*scale));
    // Never clear the front buffer. Windows can display the window between
    // separate GDI calls, which exposed a dark frame in the original renderer.
    RECT all{0,0,w,h};
    SetDCBrushColor(presentationDC_,colorRef(0x243D39));
    FillRect(presentationDC_,&all,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    SetStretchBltMode(presentationDC_,COLORONCOLOR);
    if(!StretchBlt(presentationDC_,(w-dw)/2,(h-dh)/2,dw,dh,dc_,0,0,ScreenW,ScreenH,SRCCOPY))return;
    BitBlt(target,0,0,w,h,presentationDC_,0,0,SRCCOPY);
    GdiFlush();
}
bool Canvas::savePng(const std::wstring& path) {
    GdiFlush();
    Gdiplus::GdiplusStartupInput startup;ULONG_PTR token;
    if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)return false;
    bool ok=false;
    { Gdiplus::Bitmap bmp(ScreenW,ScreenH,ScreenW*4,PixelFormat32bppRGB,reinterpret_cast<BYTE*>(pixels_));
      const CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
      ok=bmp.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok; }
    Gdiplus::GdiplusShutdown(token);return ok;
}

namespace {
void panel(Canvas& c,Rect r,uint32_t fill=Paper,uint32_t edge=0xBDD0B8,int border=1) {
    c.rect(r.x+3,r.y+4,r.w,r.h,0x8AA494);c.rect(r.x,r.y,r.w,r.h,fill);c.frame(r,edge,border);
    c.rect(r.x,r.y,3,3,edge);c.rect(r.x+r.w-3,r.y,3,3,edge);
}
void button(Canvas& c,Rect r,const std::wstring& label,const View& v,bool primary=false,bool disabled=false) {
    const bool hot=r.contains(v.mouseX,v.mouseY)&&!disabled;
    uint32_t bg=primary?(hot?0x356E5C:Green):(hot?0xE4EBD9:Paper);
    if(disabled)bg=0xDBE3D4;
    panel(c,r,bg,primary?0x376955:0xA4BAA2);
    c.center(r.x+r.w/2,r.y+(r.h-17)/2,label,16,primary?White:(disabled?0x8FA394:Ink),true);
}
void cloud(Canvas& c,int x,int y,int s) {
    c.rect(x+8*s,y,22*s,5*s,0xF8FBEF);c.rect(x,y+5*s,45*s,6*s,0xF8FBEF);
    c.rect(x-8*s,y+11*s,60*s,5*s,0xEFF7E7);c.rect(x+4*s,y+16*s,37*s,3*s,0xDFEEE0);
}
void flower(Canvas& c,int x,int y,uint32_t petal,int size=2) {
    c.rect(x,y,2,8,0x5B986D);c.rect(x-size,y-size,3*size,size,petal);c.rect(x,y-2*size,size,3*size,petal);c.rect(x,y-size,size,size,0xE4B754);
}
void windmill(Canvas& c,int x,int base,int height,float t) {
    const int y=base-height;
    c.poly({{x-4,base},{x+5,base},{x+2,y},{x-2,y}},0xE6EDD9);
    c.rect(x+2,y,2,height,0xAFC9B7);
    for(int i=0;i<3;i++) {
        const float angle=t*.32f+i*2.0943951f;
        const int dx=static_cast<int>(std::cos(angle)*height*.53f),dy=static_cast<int>(std::sin(angle)*height*.53f);
        c.poly({{x-2,y-2},{x+2,y+2},{x+dx,y+dy},{x+dx-static_cast<int>(dy*.1f),y+dy+static_cast<int>(dx*.1f)}},0xFAFBE9);
        c.line(x,y,x+dx,y+dy,0xB3CBBB);
    }
    c.rect(x-3,y-3,6,6,0xD4E4D1);c.rect(x-1,y-1,2,2,White);
}
void scenery(Canvas& c,float scroll,float time,bool menu,int course=0) {
    const auto& track=Courses[course];
    c.clear(0xDDF1E9);
    for(int y=0;y<200;y+=8)c.rect(0,y,960,8,blend(track.sky,track.horizon,y*255/200));
    if(track.landscape==Landscape::Night) {
        for(int i=0;i<40;i++){const int x=imod(i*137+33,960),y=20+(i*43)%118;c.rect(x,y,i%4?2:3,2,0xF7EDC8);}
        c.rect(797,53,28,29,0xF6EFCC);c.rect(789,61,44,13,0xF6EFCC);c.rect(813,50,21,25,blend(track.sky,track.horizon,78));
    } else {c.rect(789,53,36,32,0xFAF3CB);c.rect(783,61,48,17,0xFAF3CB);}
    for(int i=0;i<6;i++)cloud(c,imod(i*193-static_cast<int>(scroll*.09f+time*3),1150)-90,48+(i%3)*22,1+i%2);
    for(int i=-1;i<8;i++) {
        int x=i*170-imod(static_cast<int>(scroll*.13f),170);
        c.poly({{x,178},{x+25,154},{x+41,154},{x+69,135},{x+82,135},{x+110,159},{x+139,162},{x+176,185},{x+176,215},{x,215}},blend(track.ground,track.horizon,150));
        c.poly({{x+25,186},{x+50,171},{x+88,177},{x+120,168},{x+172,197},{x+172,223},{x,223}},blend(track.ground,track.accent,70));
    }
    if(track.landscape==Landscape::Coast) {
        c.rect(0,168,960,45,0x87BBD1);c.rect(0,204,960,15,0xA9D3D6);
        for(int i=0;i<27;i++){const int x=imod(i*79-static_cast<int>(scroll*.25f+time*6),1000)-20;c.rect(x,175+(i*17)%31,21+i%4*7,2,0xD4E9E1);}
        const int lighthouse=imod(860-static_cast<int>(scroll*.15f),1160)-40;
        c.rect(lighthouse-10,131,20,75,Paper);c.rect(lighthouse-10,157,20,13,0xBA8273);c.rect(lighthouse-14,130,28,6,Ink);
        c.rect(lighthouse-8,117,16,13,0xE9D68D);c.poly({{lighthouse-15,117},{lighthouse,108},{lighthouse+15,117}},0xBE8871);
        for(int i=0;i<3;i++){const int bx=imod(450+i*277-static_cast<int>(scroll*.12f),1100)-30;c.poly({{bx,192},{bx+38,192},{bx+28,199},{bx+8,199}},0xEDF1DC);c.poly({{bx+17,168},{bx+17,189},{bx+32,189}},Paper);}
    }
    c.rect(0,213,960,51,track.ground);
    for(int i=-1;i<7;i++) {
        const int x=i*220-imod(static_cast<int>(scroll*.3f),220);
        c.poly({{x,218},{x+35,209},{x+74,209},{x+110,219},{x+174,215},{x+225,231},{x+225,260},{x,260}},blend(track.ground,track.accent,75));
    }
    if(track.landscape==Landscape::Forest) {
        for(int i=-1;i<13;i++) {
            const int x=i*95-imod(static_cast<int>(scroll*.26f),95),y=129+(i+13)%3*13;
            c.rect(x+14,y+25,7,78,0x887D5E);c.rect(x-6,y+11,48,27,0x789D7B);c.rect(x+2,y,31,58,0x648D6C);
            c.rect(x-4,y+23,44,17,0x8EAD7A);c.rect(x+5,y+3,24,7,0xA6C48B);
        }
        c.rect(0,207,960,8,0x9CCBC4);for(int i=0;i<24;i++)c.rect(imod(i*53-static_cast<int>(scroll*.35f),980),210,19,2,0xD6E9D8);
    } else if(track.landscape!=Landscape::Coast) {
        windmill(c,imod(663-static_cast<int>(scroll*.22f),1160)-50,210,86,time);
        windmill(c,imod(882-static_cast<int>(scroll*.22f),1160)-50,204,55,time+.8f);
        windmill(c,imod(1160-static_cast<int>(scroll*.22f),1160)-50,210,75,time+2.f);
        if(track.landscape==Landscape::Hills)for(int i=0;i<5;i++) {
            const int x=imod(350+i*233-static_cast<int>(scroll*.2f),1180)-60;
            c.rect(x,187,38,26,0xD9BC91);c.poly({{x-5,187},{x+19,172},{x+44,187}},0xA87965);c.rect(x+14,198,8,15,0x746D56);
        }
        if(track.landscape==Landscape::Night)for(int i=0;i<13;i++) {
            const int x=imod(i*88-static_cast<int>(scroll*.45f),1050)-45;
            c.rect(x,201,3,36,0x617773);c.rect(x-5,193,13,11,0xD5B985);c.rect(x-3,195,9,7,0xF7E5A7);
        }
    }
    for(int i=0;i<135;i++) {
        const int x=imod(i*71-static_cast<int>(scroll*.55f),980)-10;
        const int y=213+(i*17)%36;
        c.rect(x,y,2+(i%3),4+(i%5),i%3==0?0xD8D693:0x86B776);
        if(i%6==0)flower(c,x,y,0xF4E9C1,1);
    }
    if(menu) {
        c.rect(0,246,960,38,0xB1CD91);
        c.rect(0,269,960,11,0x8BAE77);
        for(int i=0;i<17;i++) {
            const int x=i*63-imod(static_cast<int>(scroll*.8f),63);
            c.rect(x,226,4,35,0x91A278);c.rect(x,227,3,32,0xF2ECC9);
        }
        c.rect(0,235,960,3,0xEAE6C2);c.rect(0,244,960,3,0xF1E9C9);
        for(int i=0;i<25;i++)flower(c,18+i*39,269+(i%2)*5,i%3?White:0xE4ADC0);
    }
    for(int i=0;i<7;i++) {
        const int x=imod(i*143+static_cast<int>(time*24),1000)-20,y=124+(i*37)%92;
        c.line(x,y,x+16,y,0xF8F8DA,2);c.rect(x+16,y-2,7,2,0xF8F8DA);
    }
}

void head(Canvas& c,int x,int y,int s,int id,bool riding) {
    const auto& ch=Characters[id];
    const Appearance appearance=ch.appearance;
    auto p=[&](int a,int b,int w,int h,uint32_t col){c.rect(x+a*s,y+b*s,w*s,h*s,col);};
    // Stepped silhouettes and a deliberately small palette mirror the supplied chibis.
    p(-7,-6,14,14,ch.hairShade);p(-6,-8,12,14,ch.hair);p(-8,-4,2,15,ch.hair);
    p(6,-4,2,14,ch.hairShade);p(-5,-2,10,9,0xFFF1C4);p(-4,6,8,2,0xF6DCAF);
    p(-6,-5,12,4,ch.hair);p(-6,-2,3,3,ch.hair);p(-1,-3,3,3,ch.hair);p(4,-2,2,3,ch.hair);
    const uint32_t eye=appearance==Appearance::RedCap?0xD9A529:(appearance==Appearance::PurpleRibbon?0x8D76BE:ch.accent);
    p(-3,1,2,4,eye);p(3,1,2,4,eye);p(-3,1,1,1,White);p(3,1,1,1,White);
    p(-5,5,2,1,0xF4C7AB);p(4,5,2,1,0xF4C7AB);
    if(appearance==Appearance::StrawHat) { // Straw hat, checked ribbon.
        p(-7,-10,13,3,0xF2E8CB);p(-10,-7,20,2,0xF7F0D7);
        for(int n=-6;n<7;n+=4)p(n,-8,2,1,0x695F4F);
        p(-8,9,2,3,ch.hair);p(6,9,2,3,ch.hair);
    } else if(appearance==Appearance::RedCap) {
        p(-5,-10,12,3,0xED4E54);p(-7,-7,17,2,0xDC474B);p(2,-5,10,2,0x303F3D);
        p(-9,-8,2,4,Gold);p(-10,-6,3,2,0xE75553);p(4,-8,2,1,0xBBDF96);
    } else if(appearance==Appearance::WhiteHair) {
        p(-8,-2,2,2,0x8EDBD0);p(7,-2,2,2,0x8EDBD0);p(-9,7,2,2,0x8EDBD0);p(8,6,2,2,0x8EDBD0);
        p(-9,10,2,3,0xF7F8ED);p(8,10,2,3,0xF7F8ED);
    } else if(appearance==Appearance::PurpleRibbon) {
        p(-6,-9,11,1,0xB274BE);p(-8,-7,2,4,0xA565B5);p(7,-5,2,3,0xAF74BC);
        p(-9,7,3,6,0x4B385D);p(7,7,3,6,0x4B385D);p(-10,12,3,3,0xB379C9);p(8,12,3,3,0xB379C9);
        p(3,-2,3,2,ch.hair);p(5,-5,2,6,ch.hair);
    } else {
        p(-6,-9,11,1,0x574C44);p(-5,-8,3,2,0x7B7564);p(1,-8,3,2,0x7B7564);
        p(6,-5,3,3,0xE784AF);p(8,-3,2,2,0xF1A7B1);p(-7,8,3,3,ch.hair);
    }
    if(!riding){p(-4,8,8,6,White);p(-5,9,10,2,0x29475D);p(-1,10,2,5,ch.accent);}
}

void standing(Canvas& c,int x,int ground,int s,int id,float time) {
    int bob=static_cast<int>(std::sin(time*3+id)*.8f)*s;
    int y=ground-22*s+bob;
    c.ellipse(x-9*s,ground-2*s,18*s,3*s,0xCDD6BB);
    head(c,x,y,s,id,false);
    c.rect(x-6*s,y+14*s,12*s,4*s,Characters[id].appearance==Appearance::StrawHat?0xFCF5DC:0x315675);
    c.rect(x-5*s,y+18*s,3*s,4*s,White);c.rect(x+2*s,y+18*s,3*s,4*s,White);
    c.rect(x-5*s,y+21*s,3*s,2*s,0x3C5350);c.rect(x+2*s,y+21*s,3*s,2*s,0x3C5350);
    c.rect(x-7*s,y+10*s,2*s,5*s,White);c.rect(x+5*s,y+10*s,2*s,5*s,White);
}

void horse(Canvas& c,int x,int y,int s,int id,float anim,float jump,bool sprint,bool player) {
    static const uint32_t coats[]={0xC79868,0xE0CDB0,0xEDE7D8,0x8B746D,0xBA8165};
    const uint32_t coat=coats[id],dark=blend(coat,0x403B36,120),light=blend(coat,White,78);
    c.ellipse(x-18*s,y-2*s,38*s,5*s,0xA6AF7B);
    y-=static_cast<int>(jump);
    const int gait=static_cast<int>(anim*10)%4;
    const int bob=(jump>0?0:(gait%2)*s);
    auto p=[&](int a,int b,int w,int h,uint32_t col){c.rect(x+a*s,y+b*s-bob,w*s,h*s,col);};
    auto leg=[&](int root,int phase,bool rear){
        const int swing=jump>0?3:((gait+phase)%4-1)*2;
        p(root,-9,3,5,rear?dark:coat);p(root+swing,-5,3,4,rear?dark:light);p(root+swing-1,-2,4,2,0x555044);
    };
    if(sprint)for(int k=0;k<3;k++)c.rect(x-(25+k*7)*s,y-(2+k*2)*s,3*s,s,0xF2DEA5);
    leg(-10,0,true);leg(6,2,true);
    p(-17,-18,5,4,dark);p(-20,-16,4,8,dark);p(-23,-12,5,4,dark);
    p(-13,-17,23,10,dark);p(-14,-16,25,7,coat);p(-11,-17,20,3,light);p(-10,-9,20,2,coat);
    p(7,-23,7,13,coat);p(9,-25,12,7,coat);p(17,-23,7,5,light);
    p(9,-29,3,5,dark);p(14,-28,2,4,coat);p(7,-25,3,12,dark);
    p(14,-23,2,2,0x344037);p(21,-21,1,1,dark);p(17,-19,6,1,dark);
    p(15,-20,2,4,0xE7D6AB);p(10,-17,8,1,0x5B5C46);
    leg(-8,2,false);leg(8,0,false);
    p(-7,-19,12,5,Characters[id].accent);p(-7,-19,12,1,White);p(-4,-16,2,7,0x344B5B);
    c.text(x-5*s,y-18*s-bob,std::to_wstring(id+1),s*4,White,true);
    p(-4,-23,9,5,White);p(-4,-24,8,2,0x385970);p(0,-22,2,4,Characters[id].accent);
    p(4,-23,5,2,White);p(8,-22,3,2,0xF8DFB4);p(10,-20,5,1,0x6D6950);
    head(c,x-2*s,y-30*s-bob,s,id,true);
    if(player) {
        const int ay=y-45*s-bob;
        c.poly({{x-5,ay},{x+5,ay},{x,ay+6}},Gold);
    }
}

void skillIcon(Canvas& c,int x,int y,Skill skill,uint32_t accent,int scale=1) {
    auto p=[&](int a,int b,int w,int h,uint32_t col){c.rect(x+a*scale,y+b*scale,w*scale,h*scale,col);};
    switch(skill) {
        case Skill::Supply:
            p(-8,-7,17,3,0xB58253);p(-7,-4,15,3,Gold);p(-5,-1,11,3,0xF1D583);
            p(-3,2,7,3,Gold);p(-1,5,3,3,0xF1D583);p(-4,-3,3,3,0xCC7660);p(2,0,3,3,0xCC7660);break;
        case Skill::Courier:
            p(-10,-6,21,14,accent);p(-9,-5,19,11,Paper);p(-7,-4,3,2,accent);p(-4,-2,3,2,accent);
            p(-1,0,3,2,accent);p(2,-2,3,2,accent);p(5,-4,3,2,accent);p(6,-3,3,3,Gold);break;
        case Skill::Curiosity:
            p(-2,-9,4,18,accent);p(-6,-5,12,10,accent);p(-9,-2,18,4,accent);
            p(-2,-5,4,10,Gold);p(-5,-2,10,4,Gold);p(-1,-2,3,3,Paper);break;
        case Skill::Support:
            p(-8,-8,16,13,accent);p(-6,5,12,3,accent);p(-3,8,6,3,accent);
            p(-2,-5,4,12,Paper);p(-6,-1,12,4,Paper);break;
        case Skill::Glide:
            p(-11,-1,23,3,accent);p(-8,2,17,2,accent);p(-5,4,10,2,accent);
            p(-3,-6,4,7,accent);p(1,-4,3,5,accent);p(-10,-3,3,3,Gold);p(-2,-1,8,2,Paper);break;
    }
}
void numberBadge(Canvas& c,int x,int y,int id,bool selected=false) {
    c.rect(x,y,21,20,selected?Ink:Characters[id].accent);
    c.center(x+10,y+1,std::to_wstring(id+1),14,White,true);
}
void seasonHub(Canvas& c,const Race&,const View& v) {
    if(!v.season)return;
    const auto& season=*v.season;const auto& course=Courses[season.round()];
    const auto totals=season.points();const auto order=season.standings();const bool finished=season.finished();
    scenery(c,v.time*3,v.time,false,season.round());c.shade(Paper,92);
    panel(c,{24,20,912,67},Paper,course.accent);
    c.text(41,30,L"风野巡回赛 · 第 "+std::to_wstring(season.number)+L" 赛季",24,Ink,true);
    c.text(43,62,finished?L"五站旅程已经完成 · 感谢这一路与你相伴的风":L"每站：练习赛 → 排位赛 → 正赛   /   正赛积分 10 · 8 · 6 · 4 · 2",12,Muted);
    c.text(669,33,std::wstring(Characters[season.character].name)+L"  /  "+(season.difficulty?L"竞速":L"悠风"),17,Ink,true);
    c.text(669,61,L"已完成 "+std::to_wstring(season.completed)+L" / 15 个环节",12,Muted);
    for(int i=0;i<CourseCount;i++) {
        const int x=24+i*184;const auto& map=Courses[i];const bool current=!finished&&i==season.round();
        panel(c,{x,104,176,100},current?0xF9F7DC:Paper,current?map.accent:0xB9CDB5,current?2:1);
        c.rect(x+2,106,172,36,map.sky);c.text(x+10,114,L"0"+std::to_wstring(i+1),17,Ink,true);
        for(int h=0;h<3;h++)c.poly({{x+70+h*32,139},{x+91+h*24,115+h*5},{x+120+h*22,139}},blend(map.ground,map.accent,100));
        if(map.landscape==Landscape::Coast){c.rect(x+88,129,77,9,0x86B5C9);c.rect(x+143,113,8,24,Paper);}
        if(map.landscape==Landscape::Forest)for(int t=0;t<3;t++){const int xx=x+82+t*28;c.rect(xx,125,4,16,0x7C805D);c.poly({{xx-10,128},{xx+2,109},{xx+13,128}},0x6E946C);}
        if(map.landscape==Landscape::Night){c.rect(x+143,111,11,11,0xF2DFB1);c.rect(x+148,110,8,7,map.sky);}
        c.text(x+10,148,map.name,15,Ink,true);c.text(x+11,169,std::to_wstring(static_cast<int>(map.length))+L" m · "+std::to_wstring(map.hurdles.size())+L" 栏",10,Muted);
        for(int stage=0;stage<3;stage++) {
            const int cursor=i*3+stage;const uint32_t color=cursor<season.completed?Green:cursor==season.completed?Gold:0xCDD7C1;
            c.rect(x+11+stage*53,190,7,7,color);c.text(x+23+stage*53,186,stage==0?L"练习":stage==1?L"排位":L"正赛",10,Muted);
        }
    }
    panel(c,{24,221,390,222},Paper,course.accent);
    if(finished) {
        c.text(42,237,L"赛季冠军",14,Muted,true);c.text(42,261,Characters[order[0]].name,27,Ink,true);
        c.text(42,301,L"你的赛季排名  #"+std::to_wstring(season.playerRank()),20,Ink,true);
        standing(c,82,419,3,season.character,v.time);
        c.text(137,350,L"总积分  "+std::to_wstring(totals[season.character]),18,Ink,true);
        c.text(137,381,L"分站冠军  "+std::to_wstring(season.wins(season.character))+L" 次",14,Muted);
        c.text(137,411,L"下一季可以重新选择搭档与难度。",11,Muted);
    } else {
        const auto session=season.nextSession();
        c.text(42,237,L"第 "+std::to_wstring(season.round()+1)+L" 站 / "+sessionName(session),22,Ink,true);
        c.text(42,272,course.subtitle,13,Muted);c.text(42,296,course.strategy,12,Ink);
        const wchar_t* first=session==Session::Practice?L"完成整条路线，熟悉障碍、风向与技能时机。":session==Session::Qualifying?L"按本场实际完赛用时，排定正赛起始位。":L"五盏红灯熄灭后，按 Shift 起跑。";
        const wchar_t* second=session==Session::Practice?L"练习不计积分，完成后进入本站排位赛。":session==Session::Qualifying?L"相邻起始位相距 10 米，首尾相距 40 米。":L"提前起跑加罚 5 秒，最终成绩决定积分。";
        c.text(42,325,first,12,Ink);c.text(42,348,second,12,Muted);
        if(session==Session::Feature) {
            c.text(42,378,L"本站起始位 · 相邻 10 米",11,Muted,true);const auto grid=season.grid();
            for(int i=0;i<RiderCount;i++){const int x=43+i*71;numberBadge(c,x,403,grid[i],grid[i]==season.character);c.text(x+26,406,L"P"+std::to_wstring(i+1),11,Muted);}
        } else if(session==Session::Qualifying) {
            c.text(42,388,L"你的练习成绩  "+formatTime(season.history[season.round()*3].times[season.character]),15,Ink,true);
            c.text(42,413,L"同一站的三个环节使用相同路线。",11,Muted);
        } else {
            c.text(42,388,L"空格 越栏 · Shift 冲刺 · E 专属技能",13,Ink,true);
            c.text(42,413,L"中途返回总览可重跑当前环节。",11,Muted);
        }
    }
    panel(c,{430,221,506,222},Paper,0xB9CDB5);
    c.text(448,233,finished?L"最终积分榜":L"赛季积分榜",18,Ink,true);
    c.text(448,261,L"位",10,Muted);c.text(481,261,L"马号 / 搭档",10,Muted);
    for(int i=0;i<CourseCount;i++)c.center(601+i*45,261,L"第"+std::to_wstring(i+1)+L"站",10,Muted);
    c.center(888,261,L"总分",10,Muted);
    for(int n=0;n<RiderCount;n++) {
        const int y=284+n*26,id=order[n];
        if(id==season.character)c.rect(445,y-2,477,25,0xE8EED8);
        c.text(450,y,std::to_wstring(n+1),14,Muted,true);numberBadge(c,478,y,id,id==season.character);
        c.text(507,y+2,Characters[id].name,13,Ink,true);
        for(int i=0;i<CourseCount;i++)c.center(601+i*45,y+3,i*3+2<season.completed?std::to_wstring(season.roundPoints(i,id)):L"—",12,Muted);
        c.center(888,y,std::to_wstring(totals[id]),17,Ink,true);
    }
    c.text(448,422,L"同分依次比较高名次次数、正赛总用时。",10,Muted);
    button(c,ui::HubBack,L"返回菜单 / ESC",v);
    button(c,ui::HubStart,finished?L"返回主页 / ENTER":v.seasonDirty?L"重试保存 / ENTER":std::wstring(L"开始")+sessionName(season.nextSession())+L" / ENTER",v,true);
    c.text(219,479,v.seasonDirty?L"进度尚未保存，请检查 data 目录后重试":finished?L"15 / 15 完成 · 本季成绩已记录":L"完成环节后自动保存",11,v.seasonDirty?0xB96E58:Muted,true);
}
void menu(Canvas& c,const Race& r,const View& v) {
    const bool selecting=v.seasonMode&&v.season&&v.season->selecting;
    const bool locked=v.seasonMode&&v.season&&v.season->active&&!selecting&&!v.season->finished();
    const int course=locked?v.season->round():v.seasonMode?0:v.selectedCourse;
    scenery(c,v.time*5,v.time,true,course);
    const auto& selected=Characters[r.selected];
    c.text(32,19,L"ANEMOI  /  SUMMER RACING CLUB",11,Muted,true);
    button(c,ui::SeasonMode,L"生涯",v,v.seasonMode);
    button(c,ui::SingleMode,L"练习",v,!v.seasonMode);
    c.pixelText(35,57,"ANEMOI",7,0xACCAAB);
    c.pixelText(32,53,"ANEMOI",7,Ink);
    c.text(34,116,L"风野杯  ·  像素赛马",25,Ink,true);
    c.text(34,153,selecting?L"第 "+std::to_wstring(v.season->number)+L" 赛季 · 请选择搭档与难度":v.seasonMode?L"五站旅程 · 每站练习、排位、正赛":L"自由选择赛道，练习你的专属节奏。",14,Muted);
    c.rect(33,183,333,30,Paper);
    c.center(199,190,std::wstring(Courses[course].name)+L" / "+std::to_wstring(static_cast<int>(Courses[course].length))+L" m",12,Ink,true);
    if(!v.seasonMode){button(c,ui::CoursePrev,L"<",v);button(c,ui::CourseNext,L">",v);}
    if(v.seasonMode)button(c,ui::History,L"历史赛季",v);
    c.text(34,218,v.seasonMode?(!v.storageNotice.empty()?v.storageNotice:L"练习熟悉路线 · 排位决定起始位 · 正赛争夺积分"):L"Q / E 切换地图 · 每张地图独立记录最佳时间",11,Muted);
    horse(c,550,235,2,(r.selected+1)%5,v.time,0,false,false);
    horse(c,670,234,3,r.selected,v.time,0,false,true);
    horse(c,835,236,2,(r.selected+4)%5,v.time+.4f,0,false,false);
    c.rect(0,239,960,301,Cream);c.rect(0,239,960,2,0xD9DFC2);
    for(int i=0;i<RiderCount;i++) {
        Rect box=ui::character(i);const auto& ch=Characters[i];
        const bool active=r.selected==i,hot=box.contains(v.mouseX,v.mouseY);
        panel(c,box,active?0xEDF3DD:(hot?0xFFFFF5:Paper),active?ch.accent:0xC9D2B9,active?2:1);
        c.rect(box.x+1,box.y+1,box.w-2,4,ch.accent);
        numberBadge(c,box.x+9,box.y+12,i,active);
        standing(c,box.x+box.w/2,box.y+78,2,i,v.time);
        c.center(box.x+box.w/2,box.y+82,ch.name,17,Ink,true);
        c.center(box.x+box.w/2,box.y+104,ch.roman,10,Muted,true);
        c.center(box.x+box.w/2,box.y+119,ch.trait,12,ch.accent,true);
    }
    button(c,ui::Difficulty,r.difficulty?L"难度：竞速":L"难度：悠风",v,false,locked);
    panel(c,{32,403,638,98},Paper,selected.accent);
    skillIcon(c,58,425,selected.skill,selected.accent);
    c.text(82,411,std::wstring(L"E  ")+selected.trait,18,Ink,true);
    c.text(231,416,selected.lore,12,Muted);
    c.text(47,441,selected.description,13,Ink);
    c.text(47,461,selected.detail,12,Ink);
    c.text(47,482,selected.passive,11,Muted);
    if(locked)button(c,ui::NewSeason,L"开始新赛季",v);
    const std::wstring action=!v.seasonMode?L"开始练习":selecting?L"确认选角":locked?L"继续":v.season&&v.season->finished()?L"下一赛季":L"开始生涯";
    button(c,ui::Start,action+L" / ENTER",v,true);
    c.text(32,518,locked?L"本季搭档与难度已锁定 · 可切换单站自由练习":L"1–5 选角 · 空格 跳跃 · Shift 冲刺 · E 专属技能",11,Muted);
    c.text(493,518,v.seasonMode?L"SEASON TOUR · 5 站 / 15 个环节":v.best>0?L"本站最佳  "+formatTime(v.best):L"准备开启本站的第一场比赛。",11,Muted);
    c.text(829,518,v.music?L"M 音乐：开":L"M 音乐：关",11,Muted);
}
void seasonHistory(Canvas& c,const View& v) {
    const int count=v.season?v.season->pastSeasonCount():0;
    const int page=std::clamp(v.historyPage,0,std::max(0,count-1));
    const SeasonRecord* record=v.season?v.season->pastSeason(page):nullptr;
    scenery(c,v.time*3,v.time,false,0);c.shade(Paper,100);
    panel(c,{24,20,912,73},Paper,Green);
    c.text(42,31,L"历史赛季",26,Ink,true);
    c.text(43,67,L"往季积分与排名 · 按左右方向键切换赛季",12,Muted);
    c.text(724,42,L"已保存 "+std::to_wstring(count)+L" 个赛季",15,Muted,true);
    if(!record) {
        panel(c,{24,110,912,332},Paper,0xB9CDB5);
        c.center(480,212,L"暂无历史赛季",25,Ink,true);
        c.center(480,257,L"完成赛季，或确认开始新赛季后，可在这里查看记录。",15,Muted);
        c.center(480,290,L"历史记录会保留各站积分、总排名和你的搭档。",13,Muted);
    } else {
        const auto totals=record->points(),order=record->standings();
        const bool complete=record->completed==SeasonSessions;
        panel(c,{24,110,912,74},Paper,Characters[record->character].accent);
        c.text(42,120,L"第 "+std::to_wstring(record->number)+L" 赛季 · "+(complete?L"已完成":L"提前结束"),22,Ink,true);
        c.text(644,125,std::wstring(Characters[record->character].name)+L" / "+(record->difficulty?L"竞速":L"悠风"),17,Ink,true);
        c.text(43,158,record->completed<3?L"尚无正赛成绩":std::wstring(complete?L"你的最终排名  #":L"结束时顺位  #")+std::to_wstring(record->playerRank()),12,Muted,true);
        c.text(301,158,L"总积分  "+std::to_wstring(totals[record->character]),12,Muted,true);
        c.text(475,158,L"分站冠军  "+std::to_wstring(record->wins(record->character)),12,Muted,true);
        c.text(685,158,L"已完成 "+std::to_wstring(record->completed/3)+L" / 5 场正赛",12,Muted);
        panel(c,{24,201,912,241},Paper,0xB9CDB5);
        c.text(43,217,L"排名",11,Muted);c.text(88,217,L"马号 / 搭档",11,Muted);
        for(int round=0;round<CourseCount;round++)c.center(360+round*90,217,Courses[round].name,11,Muted);
        c.center(860,217,L"总积分",12,Muted,true);
        for(int rank=0;rank<RiderCount;rank++) {
            const int y=249+rank*34,id=order[rank];const bool you=id==record->character;
            if(you)c.rect(39,y-2,882,31,0xE8EED8);
            c.text(51,y+2,record->completed<3?L"—":std::to_wstring(rank+1),17,rank==0&&complete?0xB28C40:Muted,true);
            numberBadge(c,88,y+3,id,you);c.text(123,y+2,Characters[id].name,16,Ink,true);
            if(you)c.text(252,y+5,L"YOU",11,Green,true);
            for(int round=0;round<CourseCount;round++)c.center(360+round*90,y+4,round*3+2<record->completed?std::to_wstring(record->roundPoints(round,id)):L"—",15,Muted);
            c.center(860,y,std::to_wstring(totals[id]),21,Ink,true);
        }
        c.text(42,423,L"未举行的正赛显示 —；排名按积分、高名次次数和正赛总用时计算。",11,Muted);
    }
    button(c,ui::HubBack,L"返回主页 / ESC",v);
    c.center(408,480,count?std::to_wstring(page+1)+L" / "+std::to_wstring(count):L"尚无记录",14,Muted,true);
    button(c,ui::HistoryPrev,L"← 较新赛季",v,false,page==0);
    button(c,ui::HistoryNext,L"较早赛季 →",v,false,page+1>=count);
}
void feather(Canvas& c,int x,int y,float t) {
    y+=static_cast<int>(std::sin(t*4)*3);
    c.rect(x-1,y-7,4,4,0xF9F6D7);c.rect(x-5,y-3,7,4,0xFFFBEB);c.rect(x-7,y+1,7,3,0xD7F0D9);
    c.line(x-7,y+7,x+3,y-6,0x8DBBA8,1);c.rect(x+6,y-4,2,2,Gold);
}
void hurdle(Canvas& c,int x,int y,bool behind) {
    const uint32_t wood=behind?0xC19E6A:0xC99062;
    c.rect(x-10,y-26,4,28,0x947859);c.rect(x+11,y-26,4,28,0x947859);
    c.rect(x-11,y-25,29,5,wood);c.rect(x-11,y-25,29,2,0xF8EBC1);
    c.rect(x-10,y-14,26,4,0xE7CEA1);c.rect(x-9,y,6,3,0xA1855C);c.rect(x+10,y,6,3,0xA1855C);
}
void bar(Canvas& c,int x,int y,int width,float amount,uint32_t col) {
    c.rect(x,y,width,10,0xD8DFCA);c.rect(x,y,std::clamp(static_cast<int>(amount*width/100),0,width),10,col);
    for(int i=1;i<10;i++)c.rect(x+i*width/10,y,1,10,Paper);
}
void raceHud(Canvas& c,const Race& r,const View& v) {
    const auto& p=r.riders[0];const auto& ch=Characters[p.character];
    const int remaining=std::max(0,static_cast<int>(std::ceil(r.length-p.distance)));
    const auto& track=Courses[r.course];
    const bool finalSection=p.distance>=r.length-400;
    const bool gridCountdown=r.session==Session::Feature&&r.phase==Phase::Countdown;
    panel(c,{16,12,928,73},Paper,0xBCD0BB);
    c.rect(16,12,184,73,Ink);
    c.pixelText(30,26,"R"+std::to_string(r.course+1),3,0xEAD9A6);c.text(82,20,r.session==Session::Single?L"风野杯":sessionName(r.session),24,White,true);
    c.text(30,55,std::wstring(track.name)+L" / "+std::to_wstring(static_cast<int>(r.length))+L"m",11,0xC9DBC3);
    c.text(215,22,gridCountdown?L"起始位":L"顺位",11,Muted);c.text(214,40,std::to_wstring(gridCountdown?p.gridSlot+1:r.playerRank()),29,Ink,true);c.text(242,55,L"/ 5",12,Muted);
    c.text(303,22,finalSection?L"最后 400 m":L"距终点",11,finalSection?0xB67A32:Muted,true);
    c.text(302,42,std::to_wstring(remaining)+L" m",25,finalSection?0xA57735:Ink,true);
    c.text(472,22,p.penaltyTime>0?L"比赛时间 · 罚时 +5s":L"比赛时间",11,p.penaltyTime>0?0xBA584C:Muted);c.text(470,43,formatTime(p.finishTime>=0?p.finishTime:r.clock),22,Ink,true);
    const wchar_t* pace=!p.started?L"待起跑":p.stun>0?L"受阻":p.exhausted?L"恢复":p.skillTime>0?L"技能中":p.sprinting?L"冲刺":L"控速";
    c.text(649,22,std::wstring(L"节奏 · ")+pace,11,Muted);
    wchar_t speed[32];std::swprintf(speed,32,L"%.1f m/s",p.speed);c.text(648,44,speed,20,Ink,true);
    button(c,ui::Sound,v.music?L"♪":L"静",v);button(c,ui::Pause,L"Ⅱ",v);
    c.text(837,60,track.landscape==Landscape::Night?L"夜  /  马场：良":L"晴  /  马场：良",11,Muted);
    const auto order=r.standings();const auto& leader=r.riders[order[0]];
    panel(c,{16,95,190,191},Paper,0xBCD0BB);
    c.text(27,101,L"实时顺位",11,Muted,true);
    c.text(146,101,L"距领先",10,Muted);
    for(int n=0;n<RiderCount;n++) {
        const auto& rd=r.riders[order[n]];const int y=122+n*32;const bool you=order[n]==0;
        if(you)c.rect(20,y-2,182,30,0xE4EDD7);
        c.text(26,y+3,std::to_wstring(n+1),12,Muted,true);numberBadge(c,44,y+1,rd.character,you);
        c.text(71,y+3,Characters[rd.character].name,12,Ink,true);
        std::wstring gap;
        if(gridCountdown&&!rd.started)gap=L"P"+std::to_wstring(rd.gridSlot+1);
        else if(rd.penaltyTime>0&&rd.finishTime<0)gap=L"+5s 罚";
        else if(n==0)gap=rd.finishTime>=0?L"头马":L"领先";
        else if(rd.finishTime>=0)gap=L"+"+formatSeconds(rd.finishTime-leader.finishTime);
        else {
            const int meters=static_cast<int>(std::round(leader.distance-rd.distance));
            gap=meters==0?L"并行":L"-"+std::to_wstring(meters)+L"m";
        }
        c.center(173,y+5,gap,10,rd.penaltyTime>0?0xBA584C:you?Green:Muted,true);
    }
    // Place each sectional time directly over its span of the total course bar.
    panel(c,{218,95,726,80},Paper,0xBCD0BB);
    const int mx=236,my=143,mw=688;
    c.rect(mx,my,mw,7,0xDFE5CE);
    c.rect(mx,my,std::clamp(static_cast<int>(p.distance/r.length*mw),0,mw),7,0xA7C2A0);
    for(const auto& zone:track.zones)
        c.rect(mx+static_cast<int>(zone.begin/r.length*mw),my,static_cast<int>((zone.end-zone.begin)/r.length*mw),7,zone.speed<0?0xD2A184:zone.speed>0?0x8FBDA3:0x8DBABD);
    for(float hurdleAt:r.hurdles)c.rect(mx+static_cast<int>(hurdleAt/r.length*mw),my+9,2,3,0xB58F64);
    for(int n=4;n>=0;n--){const auto& rd=r.riders[order[n]];const int xx=mx+std::clamp(static_cast<int>(rd.distance/r.length*mw),0,mw);c.rect(xx-2,my-4,order[n]==0?7:4,order[n]==0?15:9,order[n]==0?Ink:Characters[rd.character].accent);}
    for(int n=0;n<4;n++){const float distance=n==0?0:r.splitDistance(n-1);const int x=mx+static_cast<int>(distance/r.length*mw);c.rect(x,my-2,1,11,Green);c.text(x-(n?16:0),158,n==0?L"起点":std::to_wstring(static_cast<int>(distance)),10,Muted);}
    for(int n=0;n<3;n++) {
        const float begin=n==0?0:r.splitDistance(n-1),end=r.splitDistance(n);
        const int center=mx+static_cast<int>((begin+end)*.5f/r.length*mw);
        c.center(center,102,n==0?L"前 "+std::to_wstring(static_cast<int>(r.length-800))+L" m":n==1?L"中 400 m":L"末 400 m",10,Muted);
        c.center(center,119,formatSeconds(r.sectionTime(0,n)),14,n==2&&finalSection?0xA57735:Ink,true);
    }
    const bool signalVisible=r.session==Session::Feature&&(r.phase==Phase::Countdown||(r.phase==Phase::Racing&&r.clock<.8f));
    auto raceNotice=[&](const std::wstring& message,uint32_t color) {
        c.rect(218,443,726,20,Paper);c.center(581,446,message,12,color,true);
    };
    if(!signalVisible&&r.session==Session::Feature&&r.phase==Phase::Racing&&!p.started)raceNotice(L"灯已灭 · 按 Shift 起跑",Green);
    else if(!signalVisible&&r.noticeTime>0&&r.phase!=Phase::Countdown)raceNotice(r.notice,Ink);
    else if(!signalVisible&&r.phase==Phase::Racing) {
        const float d=r.nextHurdleDistance();
        if(d<80)raceNotice(L"下一道木栏 "+std::to_wstring(std::max(0,static_cast<int>(d)))+L" m · 空格起跳",Ink);
        else if(const auto* zone=r.zoneAt(p.distance))raceNotice(std::wstring(zone->label)+(zone->speed<0?L" · 留意体力":zone->speed>0?L" · 借势加速":L" · 体力恢复加快"),Green);
        else if(finalSection)raceNotice(L"最后直路 · 留意体力，把握冲刺时机",Ink);
    }
    c.rect(0,463,960,77,Paper);c.rect(0,463,960,2,0xC7D1B2);
    c.text(20,476,L"体力",12,Muted,true);c.text(157,476,std::to_wstring(static_cast<int>(p.stamina))+L"%",12,Muted);
    bar(c,20,499,172,p.stamina,p.exhausted?0xD9896B:Green);
    c.text(20,517,p.exhausted?L"恢复到 24% 后可冲刺":L"松开 Shift 蓄力",10,Muted);
    c.text(217,476,L"风之力",12,Muted,true);c.text(390,476,std::to_wstring(static_cast<int>(p.charge))+L"%",12,Muted);
    bar(c,217,499,211,p.charge,r.skillReady()?Gold:ch.accent);
    c.text(217,517,p.skillTime>0?L"技能持续中 · 可继续蓄能":L"蓄满 100% 发动专属技能",10,Muted);
    button(c,ui::Jump,L"空格 跳跃",v,p.jumpHeight>0);
    button(c,ui::Sprint,p.started?L"Shift 冲刺":L"Shift 起跑",v,p.sprinting,p.exhausted);
    button(c,ui::SkillButton,std::wstring(L"E  ")+ch.trait,v,p.skillTime>0||r.skillRecommended(),!r.skillReady()&&p.skillTime<=0);
    c.text(450,518,r.skillStatus(),11,r.skillRecommended()?Green:Muted,r.skillReady());
}
void raceScene(Canvas& c,const Race& r,const View& v) {
    const Rider& player=r.riders[0];
    const auto& track=Courses[r.course];
    // Leave room for the vertical leaderboard and show all five physical slots.
    const int origin=r.session==Session::Feature?430:270;
    const float camera=std::max(0.f,player.distance*4.f+origin-430.f);
    scenery(c,camera,v.time,false,r.course);
    for(int lane=0;lane<5;lane++) {
        const int y=218+lane*45;
        c.rect(0,y,960,45,blend(track.track,lane==player.lane?White:track.ground,lane==player.lane?30:lane%2?40:0));
        c.rect(0,y,960,2,0xF1E9BA);
        for(int k=0;k<38;k++) {
            int x=imod(k*33+lane*11-static_cast<int>(camera),990)-15;
            c.rect(x,y+13+(k*13)%21,3+(k%4)*2,2,k%3==0?0xB7C18F:0xE0DEAF);
        }
        c.rect(0,y+43,960,2,0xABB889);
        const int base=y+40;
        for(const auto& zone:track.zones) {
            const int sx=static_cast<int>(zone.begin*4-camera)+origin;
            for(int a=0;a<static_cast<int>((zone.end-zone.begin)*4/42);a++) {
                const int xx=sx+a*42;
                const uint32_t tint=zone.speed<0?0xCE967F:zone.speed>0?0xF5F1CC:0xC1E1D2;
                if(xx>-15&&xx<980) {
                    if(zone.speed==0){c.rect(xx,base-9,3,12,tint);c.rect(xx-4,base-5,11,3,tint);}
                    else {const int direction=zone.speed>0?1:-1;c.line(xx,base-8,xx+direction*7,base-4,tint,2);c.line(xx+direction*7,base-4,xx,base,tint,2);}
                }
            }
        }
        const int end=static_cast<int>(r.length*4-camera)+origin;
        for(int yy=y;yy<y+45;yy+=8)for(int xx=0;xx<2;xx++)c.rect(end+xx*8,yy,8,8,((yy-y)/8+xx)%2?White:Ink);
        const int start=origin-static_cast<int>(camera);
        c.rect(start,y,3,45,0xFCF4D4);
        int riderIndex=0;
        for(int i=0;i<RiderCount;i++)if(r.riders[i].lane==lane)riderIndex=i;
        const auto& horseData=r.riders[riderIndex];
        for(size_t k=0;k<r.hurdles.size();k++) {
            const int x=static_cast<int>(r.hurdles[k]*4-camera)+origin;
            if(x>-40&&x<1000)hurdle(c,x,base,false);
            if(static_cast<int>(k)>=horseData.nextFeather&&x>-40&&x<1000)feather(c,x,base-49,v.time+lane*.2f);
        }
        const int rx=static_cast<int>(horseData.distance*4-camera)+origin;
        if(rx>-90&&rx<1030) {
            const float t=!horseData.started||r.paused?0:r.clock+static_cast<float>(r.startElapsed);
            const auto& ch=Characters[horseData.character];
            if(horseData.skillTime>0) {
                const int lift=static_cast<int>(horseData.jumpHeight);
                for(int a=0;a<4;a++)c.line(rx-70-a*8,base-15-a*8-lift,rx-35-a*8,base-15-a*8-lift,blend(ch.accent,White,170),2);
                skillIcon(c,rx-61,base-59-lift,ch.skill,ch.accent);
                if(ch.skill==Skill::Glide) {
                    c.poly({{rx-47,base-32-lift},{rx+9,base-28-lift},{rx-13,base-13-lift}},Paper);
                    c.line(rx-47,base-32-lift,rx-13,base-13-lift,Gold,2);
                }
                if(horseData.shield)c.frame({rx-48,base-82-lift,96,86},ch.accent,2);
            }
            horse(c,rx,base,2,horseData.character,t*(horseData.speed/30),horseData.jumpHeight,horseData.sprinting,riderIndex==0);
            if(r.session==Session::Feature&&r.phase==Phase::Countdown)c.text(rx+30,base-67,L"P"+std::to_wstring(horseData.gridSlot+1),12,Ink,true);
            if(horseData.stun>0) {c.pixelText(rx-8,base-85,"!",2,0xB87555);}
        } else {
            const int markerX=rx<0?16:936;
            c.rect(markerX-8,base-26,18,18,Characters[horseData.character].accent);
            c.center(markerX+1,base-25,std::to_wstring(horseData.character+1),12,White,true);
        }
    }
    c.rect(0,443,960,20,blend(track.ground,track.accent,90));
    for(int i=0;i<47;i++)flower(c,imod(i*23-static_cast<int>(camera*1.1f),985)-12,454+(i%3)*2,i%4==0?0xD4AAD1:0xF4F0C9,1);
    // Draw player identification after all lanes so foreground lanes cannot erase it.
    const int playerX=static_cast<int>(player.distance*4-camera)+origin;
    const int playerY=218+player.lane*45+40;
    c.rect(playerX-21,playerY+3,42,14,Ink);
    c.center(playerX,playerY+3,L"YOU",10,White,true);
    c.rect(10,playerY-32,39,18,Green);c.center(29,playerY-30,L"YOU",10,White,true);
    c.line(50,playerY-24,67,playerY-24,Green,2);
    const int finishX=static_cast<int>(r.length*4-camera)+origin;
    if(finishX>-45&&finishX<1000) {
        c.rect(finishX-3,159,5,60,Ink);c.rect(finishX,161,58,19,Paper);c.text(finishX+5,163,L"FINISH",10,Ink,true);
    }
    raceHud(c,r,v);
    if(r.session==Session::Feature&&(r.phase==Phase::Countdown||(r.phase==Phase::Racing&&r.clock<.8f))) {
        panel(c,{565,182,367,86},Paper,0xCAD4B7);
        c.center(749,188,L"正赛起始位 P"+std::to_wstring(player.gridSlot+1)+L" · 等待五灯熄灭",12,Muted,true);
        for(int n=0;n<5;n++) {
            const int x=651+n*40;
            c.ellipse(x,207,28,28,0x354A42);
            c.ellipse(x+4,211,20,20,n<r.redLights()?0xE24F4B:0x68736A);
            if(n<r.redLights())c.rect(x+9,215,6,3,0xFFB49A);
        }
        c.center(749,246,player.penaltyTime>0?L"抢跑已记录 · 最终成绩 +5 秒":r.phase==Phase::Countdown?L"每秒亮一灯 · 灯灭后按 Shift 起跑":L"灯灭！按 Shift 起跑",12,player.penaltyTime>0?0xBA584C:Green,true);
    } else if(r.phase==Phase::Countdown) {
        panel(c,{350,183,260,68},Paper,0xCAD4B7);
        c.center(480,189,L"准备出闸 · 空格越栏 / E 技能",12,Muted);
        c.pixelText(470,212,std::to_string(std::max(1,static_cast<int>(std::ceil(r.countdown)))),4,Green);
    }
    if(r.phase==Phase::Finishing) {
        panel(c,{305,183,350,64},Paper,Gold);
        c.center(480,191,L"抵达终点！  "+formatTime(player.finishTime),20,Ink,true);
        c.center(480,222,L"等待其他选手冲线…",12,Muted);
    }
}
void results(Canvas& c,const Race& r,const View& v) {
    raceScene(c,r,v);c.shade(0x284D42,122);
    panel(c,{125,30,710,477},Paper,0xC1D2B8,2);
    c.text(158,49,std::wstring(Courses[r.course].name)+L" / "+sessionName(r.session),11,Muted,true);c.text(666,49,std::to_wstring(static_cast<int>(r.length))+L" m · 草地 · 良",11,Muted);
    c.pixelText(401,49,"FINISH!",3,Green);
    const std::wstring title=r.session==Session::Practice?L"练习完成 · 下一步，争取更好的起始位":r.session==Session::Qualifying?L"排位赛完成 · 正赛起始位 P"+std::to_wstring(r.playerRank()):r.playerRank()==1?L"冠军！风也在为你喝彩。":L"越过终点，又收藏了一阵风。";
    c.center(480,86,title,22,Ink,true);
    const auto& player=r.riders[0];
    c.center(480,119,player.penaltyTime>0?L"冲线 "+formatTime(player.splits[2])+L" + 抢跑罚时 5s = 最终 "+formatTime(player.finishTime):L"你的成绩  "+formatTime(player.finishTime)+(v.newRecord?L"    ·    NEW BEST!":L""),15,player.penaltyTime>0?0xBA584C:Muted,true);
    c.rect(156,149,647,1,0xD9DFC7);
    c.text(161,159,L"着顺",11,Muted);c.text(212,159,L"马号 / 搭档",11,Muted);
    c.text(432,159,L"末 400 m",11,Muted);c.text(535,159,L"最终成绩",11,Muted);c.text(647,159,L"罚时",11,Muted);c.text(725,159,L"距头马",11,Muted);
    const auto order=r.standings();
    for(int n=0;n<5;n++) {
        const auto& rd=r.riders[order[n]];const int yy=184+n*32;
        if(order[n]==0)c.rect(156,yy-3,647,29,0xE6EED7);
        c.text(172,yy,std::to_wstring(n+1),17,n==0?0xB28C40:Muted,true);
        numberBadge(c,214,yy,rd.character,order[n]==0);
        c.text(252,yy,Characters[rd.character].name,16,Ink,true);
        if(order[n]==0)c.text(378,yy+3,L"YOU",11,Green,true);
        c.text(432,yy+2,formatSeconds(r.sectionTime(order[n],2)),14,Muted);
        c.text(535,yy,rd.finishTime<0?L"未完赛":formatTime(rd.finishTime),16,Ink);
        c.text(647,yy+2,rd.penaltyTime>0?L"+5.00s":L"—",13,rd.penaltyTime>0?0xBA584C:Muted,true);
        c.text(725,yy+2,rd.finishTime<0?L"--":n==0?L"--":L"+"+formatSeconds(rd.finishTime-r.riders[order[0]].finishTime),13,Muted);
    }
    c.rect(156,349,647,1,0xD9DFC7);
    for(int n=0;n<3;n++) {
        const int x=158+n*221;
        const int begin=n==0?0:static_cast<int>(r.splitDistance(n-1));const int end=static_cast<int>(r.splitDistance(n));
        c.rect(x,360,203,40,Cream);c.text(x+12,365,std::to_wstring(begin)+L"–"+std::to_wstring(end)+L" m",10,Muted);
        c.text(x+121,370,formatSeconds(r.sectionTime(0,n)),16,Ink,true);
    }
    const auto& p=r.riders[0];
    const std::wstring reward=r.session==Session::Feature?L"   ·   本站 +"+std::to_wstring(v.season?v.season->roundPoints(r.course,p.character):0)+L" 分":r.session==Session::Single?L"   ·   得分 "+std::to_wstring(r.score()):L"   ·   本环节不计赛季积分";
    c.center(480,409,L"越栏 "+std::to_wstring(p.jumps)+L"/"+std::to_wstring(r.hurdles.size())+L"   ·   风羽 "+std::to_wstring(p.feathers)+L"   ·   技能 "+std::to_wstring(p.skillUses)+L" 次"+reward,13,Ink,true);
    c.center(480,432,v.saved?(r.session==Session::Single?L"本站个人最佳已保存在本地":L"本环节成绩已记录 · 赛季进度已保存"):L"本次记录未能写入 · 请检查 data 目录权限",10,Muted);
    button(c,ui::ResultMenu,r.session==Session::Single?L"返回牧场 / ESC":L"生涯进度 / ESC",v);
    const std::wstring next=r.session==Session::Single?L"再跑一场":v.season&&v.season->finished()?L"赛季结算":r.session==Session::Feature?L"下一站练习赛":r.session==Session::Practice?L"进入排位赛":L"进入正赛";
    button(c,ui::Again,next+L" / ENTER",v,true);
}
void pause(Canvas& c,const Race& r,const View& v) {
    c.shade(0x264E44,145);panel(c,{300,147,360,252},Paper,0xBACCB1,2);
    c.pixelText(393,171,"PAUSED",4,Green);c.center(480,214,L"让风等一会儿。",18,Ink,true);
    c.center(480,248,L"空格 跳跃  ·  Shift 冲刺  ·  E 专属技能",12,Muted);
    button(c,ui::Resume,L"继续比赛 / P",v,true);button(c,ui::Back,r.session==Session::Single?L"返回牧场 / ESC":L"返回生涯，重跑本环节 / ESC",v);
}
void newSeasonConfirmation(Canvas& c,const View& v) {
    c.shade(0x264E44,160);panel(c,{243,159,474,226},Paper,0xBACCB1,2);
    c.center(480,181,L"开始新赛季？",25,Ink,true);
    c.center(480,226,L"当前赛季的成绩与积分将重置，并跳转到下一赛季",13,Ink);
    c.center(480,253,L"下一赛季可重新选择搭档与难度。",14,Muted,true);
    c.center(480,288,!v.saved?v.storageNotice:L"确认后进入选角；取消可继续当前进度。",11,!v.saved?0xBA584C:Muted);
    button(c,ui::CancelSeason,L"取消 / ESC",v);
    button(c,ui::ConfirmSeason,L"确认开始 / ENTER",v,true);
}
}
void render(Canvas& c,const Race& r,const View& v) {
    if(r.phase==Phase::Menu)menu(c,r,v);
    else if(r.phase==Phase::SeasonHistory)seasonHistory(c,v);
    else if(r.phase==Phase::SeasonHub||r.phase==Phase::SeasonFinal)seasonHub(c,r,v);
    else if(r.phase==Phase::Results)results(c,r,v);
    else raceScene(c,r,v);
    if(r.paused)pause(c,r,v);
    if(v.confirmNewSeason)newSeasonConfirmation(c,v);
}
}
