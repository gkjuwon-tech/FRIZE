// theme.hpp ― FRIZE 콕핏 디자인 시스템 (Dear ImGui)
//
// 지휘통제(C2) 톤: 그래파이트 단색조 + 단일 앰버 액센트. 샤프 코너, 1px 보더,
// 8px 그리드, Roboto(UI) + Cousine(데이터) + 한글. 네온/라운드/그라데이션 금지.
#pragma once
#include "imgui.h"
#include <string>
#include <cmath>

namespace ui {

// ── 팔레트 (signal-on-graphite) ──────────────────────────────────────────────
constexpr ImU32 BG_APP    = IM_COL32(0x0b,0x0e,0x12,255);
constexpr ImU32 BG_PANEL  = IM_COL32(0x13,0x17,0x1e,255);
constexpr ImU32 BG_HEAD   = IM_COL32(0x1a,0x20,0x29,255);
constexpr ImU32 BG_INSET  = IM_COL32(0x0e,0x12,0x17,255);
constexpr ImU32 BORDER    = IM_COL32(0x25,0x2d,0x39,255);
constexpr ImU32 BORDER_S  = IM_COL32(0x1b,0x22,0x2b,255);
constexpr ImU32 TEXT      = IM_COL32(0xc8,0xd0,0xda,255);
constexpr ImU32 DIM       = IM_COL32(0x7e,0x88,0x95,255);
constexpr ImU32 FAINT     = IM_COL32(0x50,0x5a,0x66,255);
constexpr ImU32 ACCENT    = IM_COL32(0xdf,0xa8,0x3a,255);   // 앰버 ― 유일 액센트
constexpr ImU32 ACCENT_D  = IM_COL32(0x6a,0x53,0x22,255);
constexpr ImU32 OK        = IM_COL32(0x55,0x9d,0x70,255);
constexpr ImU32 WARN      = IM_COL32(0xc8,0x92,0x3a,255);
constexpr ImU32 CRIT      = IM_COL32(0xc4,0x46,0x3a,255);
constexpr ImU32 STEEL     = IM_COL32(0x5f,0x7c,0x90,255);

inline ImVec4 V(ImU32 c){ return ImGui::ColorConvertU32ToFloat4(c); }
inline ImU32 alpha(ImU32 c, float a){ ImVec4 v=V(c); v.w=a; return ImGui::ColorConvertFloat4ToU32(v); }

// 폰트 핸들(main에서 채움)
struct Fonts { ImFont* ui; ImFont* ui_b; ImFont* big; ImFont* mono; ImFont* mono_s; ImFont* small; };
inline Fonts FONT;

// ── 스타일 적용 ──────────────────────────────────────────────────────────────
inline void apply_style() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding=0; s.ChildRounding=0; s.FrameRounding=0; s.PopupRounding=0;
    s.ScrollbarRounding=0; s.GrabRounding=0; s.TabRounding=0;
    s.WindowBorderSize=1; s.ChildBorderSize=1; s.FrameBorderSize=1; s.PopupBorderSize=1;
    s.WindowPadding=ImVec2(0,0); s.FramePadding=ImVec2(10,6); s.ItemSpacing=ImVec2(8,8);
    s.ItemInnerSpacing=ImVec2(6,5); s.CellPadding=ImVec2(8,5); s.ScrollbarSize=10;
    s.GrabMinSize=8; s.WindowMenuButtonPosition=ImGuiDir_None;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]=V(BG_APP); c[ImGuiCol_ChildBg]=V(BG_PANEL); c[ImGuiCol_PopupBg]=V(BG_HEAD);
    c[ImGuiCol_Border]=V(BORDER); c[ImGuiCol_BorderShadow]=ImVec4(0,0,0,0);
    c[ImGuiCol_Text]=V(TEXT); c[ImGuiCol_TextDisabled]=V(FAINT);
    c[ImGuiCol_FrameBg]=V(BG_INSET); c[ImGuiCol_FrameBgHovered]=V(BG_HEAD); c[ImGuiCol_FrameBgActive]=V(BG_HEAD);
    c[ImGuiCol_Button]=V(BG_INSET); c[ImGuiCol_ButtonHovered]=V(BG_HEAD); c[ImGuiCol_ButtonActive]=V(alpha(ACCENT,0.18f));
    c[ImGuiCol_Header]=V(alpha(ACCENT,0.14f)); c[ImGuiCol_HeaderHovered]=V(alpha(ACCENT,0.10f)); c[ImGuiCol_HeaderActive]=V(alpha(ACCENT,0.20f));
    c[ImGuiCol_Separator]=V(BORDER_S); c[ImGuiCol_SeparatorHovered]=V(BORDER);
    c[ImGuiCol_ScrollbarBg]=V(BG_INSET); c[ImGuiCol_ScrollbarGrab]=V(BORDER); c[ImGuiCol_ScrollbarGrabHovered]=V(DIM);
    c[ImGuiCol_PlotHistogram]=V(ACCENT); c[ImGuiCol_PlotLines]=V(STEEL);
}

// ── 드로잉 헬퍼 ──────────────────────────────────────────────────────────────
inline void rect_filled(ImVec2 a, ImVec2 b, ImU32 col){ ImGui::GetWindowDrawList()->AddRectFilled(a,b,col); }
inline void rect_line(ImVec2 a, ImVec2 b, ImU32 col, float t=1.f){ ImGui::GetWindowDrawList()->AddRect(a,b,col,0,0,t); }
inline void dot(ImVec2 c, float r, ImU32 col){ ImGui::GetWindowDrawList()->AddCircleFilled(c,r,col,16); }

inline void text_c(ImFont* f, ImU32 col, const char* s){ ImGui::PushFont(f); ImGui::PushStyleColor(ImGuiCol_Text,V(col)); ImGui::TextUnformatted(s); ImGui::PopStyleColor(); ImGui::PopFont(); }

// 우측정렬 텍스트(현재 라인 영역 우측 끝 기준)
inline void text_right(ImFont* f, ImU32 col, const char* s, float right_x){
    ImGui::PushFont(f); float w=ImGui::CalcTextSize(s).x;
    ImVec2 p=ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(ImVec2(right_x-w,p.y),col,s);
    ImGui::PopFont();
}

// 섹션 패널 시작/끝 ― 헤더바(제목 + 우측메타) + 보더
inline bool panel_begin(const char* id, ImVec2 size, const char* title, const char* meta=nullptr){
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V(BG_PANEL));
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImVec2 p=ImGui::GetCursorScreenPos(); float w=ImGui::GetContentRegionAvail().x;
    rect_filled(p, ImVec2(p.x+w, p.y+26), BG_HEAD);
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x,p.y+26),ImVec2(p.x+w,p.y+26),BORDER);
    rect_filled(p, ImVec2(p.x+3, p.y+26), alpha(ACCENT,0.85f));   // 좌측 액센트 탭
    ImGui::PushFont(FONT.ui_b);
    ImGui::GetWindowDrawList()->AddText(ImVec2(p.x+12,p.y+6), TEXT, title);
    ImGui::PopFont();
    if(meta){ ImGui::PushFont(FONT.mono_s); float mw=ImGui::CalcTextSize(meta).x;
        ImGui::GetWindowDrawList()->AddText(ImVec2(p.x+w-mw-10,p.y+8), DIM, meta); ImGui::PopFont(); }
    ImGui::Dummy(ImVec2(0,32));
    ImGui::Indent(12);
    return true;
}
inline void panel_end(){ ImGui::Unindent(12); ImGui::EndChild(); ImGui::PopStyleColor(); }

// 가는 막대(배터리/게이지)
inline void microbar(float w, float frac, ImU32 col){
    ImVec2 p=ImGui::GetCursorScreenPos();
    rect_filled(p, ImVec2(p.x+w,p.y+5), alpha(BORDER,0.7f));
    rect_filled(p, ImVec2(p.x+w*(frac<0?0:frac>1?1:frac),p.y+5), col);
    ImGui::Dummy(ImVec2(w,5));
}

// 심각도 태그(작은 박스 라벨)
inline void sev_tag(const char* s, ImU32 col){
    ImVec2 p=ImGui::GetCursorScreenPos(); ImGui::PushFont(FONT.mono_s);
    float tw=ImGui::CalcTextSize(s).x;
    rect_line(p, ImVec2(p.x+tw+10,p.y+16), col, 1.f);
    ImGui::GetWindowDrawList()->AddText(ImVec2(p.x+5,p.y+2), col, s);
    ImGui::PopFont(); ImGui::Dummy(ImVec2(tw+10,16));
}

}  // namespace ui
