#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "game.hpp"
#include "season.hpp"
#include <map>
#include <vector>

namespace anemoi {
constexpr int ScreenW = 960, ScreenH = 540;
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};
namespace ui {
    inline constexpr Rect SeasonMode{571,16,170,31};
    inline constexpr Rect SingleMode{755,16,173,31};
    inline constexpr Rect CoursePrev{33,184,28,28};
    inline constexpr Rect CourseNext{337,184,28,28};
    inline constexpr Rect History{378,184,112,28};
    inline constexpr Rect HistoryPrev{622,466,145,43};
    inline constexpr Rect HistoryNext{783,466,145,43};
    inline constexpr Rect HubStart{625,466,303,43};
    inline constexpr Rect HubBack{32,466,165,43};
    inline constexpr Rect NewSeason{690,404,238,34};
    inline constexpr Rect CancelSeason{285,323,184,41};
    inline constexpr Rect ConfirmSeason{490,323,184,41};
    inline Rect character(int i) { return {32 + i * 182, 250, 168, 139}; }
    inline constexpr Rect Start{690, 452, 238, 47};
    inline constexpr Rect Difficulty{755,59,173,31};
    inline constexpr Rect Sound{834, 20, 44, 28};
    inline constexpr Rect Pause{886, 20, 44, 28};
    inline constexpr Rect Jump{450, 474, 108, 38};
    inline constexpr Rect Sprint{568, 474, 118, 38};
    inline constexpr Rect SkillButton{698, 474, 230, 38};
    inline constexpr Rect Resume{356, 282, 248, 44};
    inline constexpr Rect Back{356, 341, 248, 40};
    inline constexpr Rect Again{496, 451, 306, 39};
    inline constexpr Rect ResultMenu{158, 451, 306, 39};
}
struct View {
    float time = 0;
    int mouseX = -1, mouseY = -1;
    bool mouseDown = false, music = true;
    float best = -1;
    bool newRecord = false, saved = true;
    bool seasonMode = true, seasonDirty = false;
    bool confirmNewSeason = false;
    int selectedCourse = 0, historyPage = 0;
    const Season* season = nullptr;
    std::wstring storageNotice;
};
class Canvas {
public:
    Canvas();
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
    HDC dc() const { return dc_; }
    void clear(uint32_t color);
    void rect(int x, int y, int w, int h, uint32_t color);
    void frame(Rect r, uint32_t color, int size = 1);
    void line(int x1, int y1, int x2, int y2, uint32_t color, int width = 1);
    void poly(std::initializer_list<POINT> points, uint32_t color);
    void ellipse(int x, int y, int w, int h, uint32_t color);
    void text(int x, int y, const std::wstring& s, int size, uint32_t color, bool bold = false);
    void center(int x, int y, const std::wstring& s, int size, uint32_t color, bool bold = false);
    void pixelText(int x, int y, const std::string& s, int scale, uint32_t color);
    void shade(uint32_t color, int alpha);
    void present(HDC target, int width, int height);
    bool savePng(const std::wstring& path);
private:
    HFONT font(int size, bool bold);
    bool resizePresentationBuffer(HDC target, int width, int height);
    HDC dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ oldBitmap_{};
    uint32_t* pixels_{};
    std::map<int, HFONT> fonts_;
    // Full client-size frame, including the letterbox. Only the completed
    // buffer is copied to the visible window, in one BitBlt operation.
    HDC presentationDC_{};
    HBITMAP presentationBitmap_{};
    HGDIOBJ previousPresentationBitmap_{};
    int presentationWidth_=0, presentationHeight_=0;
};
void render(Canvas& canvas, const Race& race, const View& view);
}
