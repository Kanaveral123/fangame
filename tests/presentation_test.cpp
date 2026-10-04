#include "render.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Surface {
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    uint32_t* pixels{};
    int width,height;
    Surface(int w,int h):width(w),height(h) {
        dc=CreateCompatibleDC(nullptr);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        if(!dc||!bitmap)throw std::runtime_error("Cannot create test surface");
        previous=SelectObject(dc,bitmap);
    }
    ~Surface(){SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);}
    std::vector<uint32_t> snapshot() const {
        GdiFlush();std::vector<uint32_t> out(pixels,pixels+width*height);
        for(auto& pixel:out)pixel&=0xFFFFFF;
        return out;
    }
};
Surface* visible=nullptr;
std::vector<uint32_t> previousFrame,nextFrame;
int partialFrames=0,visibleWrites=0;

void observe(HDC destination) {
    if(!visible||destination!=visible->dc)return;
    ++visibleWrites;
    const auto frame=visible->snapshot();
    // A compositor may observe the window after any GDI operation. It must see
    // either the previous complete image or the next one, never a cleared frame.
    if(frame!=previousFrame&&frame!=nextFrame)++partialFrames;
}
int WINAPI trackedFillRect(HDC dc,const RECT* rect,HBRUSH brush) {
    const int result=::FillRect(dc,rect,brush);observe(dc);return result;
}
BOOL WINAPI trackedStretchBlt(HDC dst,int x,int y,int w,int h,HDC src,int sx,int sy,int sw,int sh,DWORD rop) {
    const BOOL result=::StretchBlt(dst,x,y,w,h,src,sx,sy,sw,sh,rop);observe(dst);return result;
}
BOOL WINAPI trackedBitBlt(HDC dst,int x,int y,int w,int h,HDC src,int sx,int sy,DWORD rop) {
    const BOOL result=::BitBlt(dst,x,y,w,h,src,sx,sy,rop);observe(dst);return result;
}
void require(bool pass,const char* message){if(!pass)throw std::runtime_error(message);}
}

// Intercept the real renderer's GDI writes at the display boundary. The product
// build has no interception, test callbacks, or test-specific rendering paths.
#define FillRect trackedFillRect
#define StretchBlt trackedStretchBlt
#define BitBlt trackedBitBlt
#include "../src/render.cpp"
#undef FillRect
#undef StretchBlt
#undef BitBlt

int main() {
    try {
        using namespace anemoi;
        Canvas canvas;
        const DWORD handlesBefore=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for(const auto& size:std::vector<std::pair<int,int>>{{960,540},{1280,720},{1024,768},{1200,500},{800,480},{1,1}}) {
            Surface front(size.first,size.second),expected(size.first,size.second);
            std::fill(front.pixels,front.pixels+size.first*size.second,0xAC4678u);
            for(int frame=0;frame<6;frame++) {
                canvas.clear(frame%2?0x307FAC:0xEBD7A1);
                canvas.rect(0,0,960,12,0xFFFFFF);canvas.rect(0,528,960,12,0x315244);
                canvas.rect(32+frame*70,50,121,300,0x743674);
                GdiFlush();
                const double scale=std::min(size.first/960.,size.second/540.);
                const int w=std::max(1,static_cast<int>(960*scale)),h=std::max(1,static_cast<int>(540*scale));
                RECT all{0,0,size.first,size.second};
                SetDCBrushColor(expected.dc,RGB(0x24,0x3D,0x39));
                ::FillRect(expected.dc,&all,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
                SetStretchBltMode(expected.dc,COLORONCOLOR);
                ::StretchBlt(expected.dc,(size.first-w)/2,(size.second-h)/2,w,h,canvas.dc(),0,0,960,540,SRCCOPY);
                previousFrame=front.snapshot();nextFrame=expected.snapshot();visible=&front;
                canvas.present(front.dc,size.first,size.second);
                visible=nullptr;
                require(front.snapshot()==nextFrame,"Presented image or letterbox geometry is wrong");
                if(partialFrames){std::cerr<<"Observed "<<partialFrames<<" incomplete visible frame(s) after "<<visibleWrites<<" display writes\n";throw std::runtime_error("Visible surface was cleared before the new frame was ready");}
            }
            std::cout<<"PASS complete frames and letterbox at "<<size.first<<'x'<<size.second<<'\n';
        }
        {
            Surface front(1280,720);visible=&front;previousFrame=front.snapshot();nextFrame=previousFrame;
            const int writesBefore=visibleWrites;
            canvas.present(front.dc,0,720);canvas.present(front.dc,1280,0);canvas.present(front.dc,-1,720);
            require(visibleWrites==writesBefore,"Minimized dimensions unexpectedly modify the display");visible=nullptr;
            for(int i=0;i<150;i++)canvas.present(front.dc,800+(i%3)*120,480+(i%2)*120);
        }
        GdiFlush();
        const DWORD handlesAfter=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        require(handlesAfter<=handlesBefore+3,"Resizing leaks GDI resources");
        std::cout<<"PASS minimized windows and 150 resize transitions; GDI handles "<<handlesBefore<<" -> "<<handlesAfter<<'\n';
        std::cout<<"ALL PRESENTATION CHECKS PASSED; incomplete frames="<<partialFrames<<'\n';return 0;
    }catch(const std::exception& e){visible=nullptr;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
