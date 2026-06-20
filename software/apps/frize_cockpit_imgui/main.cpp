// FRIZE Cockpit (Dear ImGui) ― 지휘통제 콕핏 UI, 프로페셔널 다크 재설계.
//
// 실시간 3D 디지털 트윈(메인) + 대원 1인칭(실사 PiP) + 유닛/경보/구조큐/명령 리본.
// 헤드리스(GLFW+OpenGL+Xvfb)로 한 프레임을 오프스크린 렌더 → PNG 캡처.
//   build: build_and_shoot.sh   →  frize_cockpit_imgui.png
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "theme.hpp"
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

using namespace ui;
static int WIN_W=1680, WIN_H=980;

struct Tex { GLuint id=0; int w=0,h=0; };
static Tex load_tex(const char* path){
    Tex t; int n; stbi_set_flip_vertically_on_load(false);
    unsigned char* d=stbi_load(path,&t.w,&t.h,&n,4);
    if(!d){ fprintf(stderr,"tex load fail %s\n",path); return t; }
    glGenTextures(1,&t.id); glBindTexture(GL_TEXTURE_2D,t.id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t.w,t.h,0,GL_RGBA,GL_UNSIGNED_BYTE,d);
    stbi_image_free(d); return t;
}
static void dt(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* s, float sz=0){
    dl->AddText(f, sz>0?sz:f->FontSize, p, c, s);
}
static void dt_r(ImDrawList* dl, ImFont* f, float rx, float y, ImU32 c, const char* s){
    ImGui::PushFont(f); float w=ImGui::CalcTextSize(s).x; ImGui::PopFont();
    dl->AddText(f, f->FontSize, ImVec2(rx-w,y), c, s);
}

// ── 데이터(목업: FRIZE 도메인) ───────────────────────────────────────────────
struct Unit{ const char* id; const char* role; ImU32 st; float batt; const char* k1; const char* v1; const char* k2; const char* v2; };
static Unit UNITS[]={
    {"SCOUT-1","드론 · 정찰",OK,0.71f,"ALT","11.4","SPD","2.1"},
    {"VISOR-1","대원 A · 진입",WARN,0.62f,"O\xE2\x82\x82","62","HR","171"},
    {"VISOR-2","대원 B · 후방",OK,0.88f,"O\xE2\x82\x82","88","HR","112"},
    {"VENT-1","배연포트",OK,1.00f,"POS","OPN","",""},
};
struct Alert{ const char* sev; ImU32 col; const char* msg; };
static Alert ALERTS[]={
    {"CRIT",CRIT,"붕괴 위험 \xE2\x80\x94 2F 동측 천장 처짐 67%"},
    {"HIGH",WARN,"VISOR-1 산소 62% · 심박 171"},
    {"HIGH",WARN,"가스 LEL 11% \xE2\x80\x94 폭발 경계"},
    {"INFO",STEEL,"SCOUT-1 프런티어 4 탐사 중"},
};
struct QItem{ const char* rank; const char* who; const char* note; };
static QItem QUEUE[]={
    {"1","3F-E 요구조자 \xC3\x97""2","37\xE2\x84\x83 · 의식불명"},
    {"2","2F-W 고립대원","SCBA 18%"},
    {"3","1F-A 잔류 확인","열원 감지"},
};

static Tex TW_TWIN, TW_POV;

// ── 패널: 유닛 로스터 ───────────────────────────────────────────────────────
static void rail_units(ImVec2 pos, ImVec2 size){
    ImGui::SetCursorScreenPos(pos);
    panel_begin("units", size, "UNITS", "4 ONLINE");
    ImDrawList* dl=ImGui::GetWindowDrawList();
    float w=ImGui::GetContentRegionAvail().x;
    for(int i=0;i<4;++i){ const Unit& u=UNITS[i];
        ImVec2 p=ImGui::GetCursorScreenPos(); float rh=58; bool sel=(i==1);
        if(sel){ rect_filled(ImVec2(p.x-12,p.y-2),ImVec2(p.x+w,p.y+rh),alpha(ACCENT,0.10f));
                 rect_filled(ImVec2(p.x-12,p.y-2),ImVec2(p.x-9,p.y+rh),ACCENT); }
        dot(ImVec2(p.x+6,p.y+9),4,u.st);
        dt(dl,FONT.ui_b,ImVec2(p.x+20,p.y+1),TEXT,u.id);
        dt_r(dl,FONT.mono_s,p.x+w-8,p.y+3,DIM,"ONLINE");
        dt(dl,FONT.small,ImVec2(p.x+20,p.y+21),DIM,u.role);
        // 배터리 바
        ImGui::SetCursorScreenPos(ImVec2(p.x+20,p.y+42)); microbar(150,u.batt,u.st);
        char pc[8]; snprintf(pc,8,"%d%%",(int)(u.batt*100));
        dt(dl,FONT.mono_s,ImVec2(p.x+178,p.y+39),DIM,pc);
        // 스탯
        float sx=p.x+w-150;
        if(u.k1[0]){ dt(dl,FONT.mono_s,ImVec2(sx,p.y+22),FAINT,u.k1); dt(dl,FONT.mono,ImVec2(sx,p.y+34),TEXT,u.v1); }
        if(u.k2[0]){ dt(dl,FONT.mono_s,ImVec2(sx+62,p.y+22),FAINT,u.k2); dt(dl,FONT.mono,ImVec2(sx+62,p.y+34),TEXT,u.v2); }
        ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+rh));
        dl->AddLine(ImVec2(p.x-12,p.y+rh+4),ImVec2(p.x+w,p.y+rh+4),BORDER_S);
        ImGui::Dummy(ImVec2(0,12));
    }
    panel_end();
}

// ── 패널: 경보 + 구조 큐 ────────────────────────────────────────────────────
static void rail_right(ImVec2 pos, ImVec2 size){
    float h_al=size.y*0.52f;
    ImGui::SetCursorScreenPos(pos);
    panel_begin("alerts", ImVec2(size.x,h_al), "ALERTS", "4");
    ImDrawList* dl=ImGui::GetWindowDrawList(); float w=ImGui::GetContentRegionAvail().x;
    for(auto& a:ALERTS){ ImVec2 p=ImGui::GetCursorScreenPos(); float rh=46;
        rect_filled(ImVec2(p.x-2,p.y),ImVec2(p.x+w,p.y+rh),BG_INSET);
        rect_line(ImVec2(p.x-2,p.y),ImVec2(p.x+w,p.y+rh),BORDER_S);
        rect_filled(ImVec2(p.x-2,p.y),ImVec2(p.x+1,p.y+rh),a.col);
        dt(dl,FONT.mono_s,ImVec2(p.x+10,p.y+6),a.col,a.sev);
        dt(dl,FONT.small,ImVec2(p.x+10,p.y+23),TEXT,a.msg);
        ImGui::Dummy(ImVec2(0,rh+8));
    }
    panel_end();
    ImGui::SetCursorScreenPos(ImVec2(pos.x,pos.y+h_al+10));
    panel_begin("queue", ImVec2(size.x,size.y-h_al-10), "RESCUE QUEUE", "3");
    dl=ImGui::GetWindowDrawList(); w=ImGui::GetContentRegionAvail().x;
    for(auto& q:QUEUE){ ImVec2 p=ImGui::GetCursorScreenPos();
        rect_line(ImVec2(p.x,p.y),ImVec2(p.x+20,p.y+18),ACCENT);
        dt(dl,FONT.mono,ImVec2(p.x+6,p.y+2),ACCENT,q.rank);
        dt(dl,FONT.ui,ImVec2(p.x+32,p.y-1),TEXT,q.who);
        dt(dl,FONT.small,ImVec2(p.x+32,p.y+18),DIM,q.note);
        ImGui::Dummy(ImVec2(0,42));
    }
    panel_end();
}

// ── 중앙: 트윈 뷰포트 + HUD + PiP + 미니맵 ──────────────────────────────────
static void center_twin(ImVec2 pos, ImVec2 size){
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, V(IM_COL32(0x0c,0x0f,0x14,255)));
    ImGui::BeginChild("twin", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p0=ImGui::GetWindowPos(), p1=ImVec2(p0.x+size.x,p0.y+size.y);
    // 트윈 이미지 (cover-fit)
    if(TW_TWIN.id){
        float ir=(float)TW_TWIN.w/TW_TWIN.h, vr=size.x/size.y; ImVec2 a=p0,b=p1;
        if(ir>vr){ float nw=size.y*ir; float off=(nw-size.x)/2; a.x=p0.x-off*0; ImVec2 c=ImVec2(p0.x,p0.y),d=ImVec2(p1.x,p1.y);
            dl->AddImage((ImTextureID)(intptr_t)TW_TWIN.id,c,d,ImVec2((off)/nw,0),ImVec2(1-(off)/nw,1)); }
        else { float nh=size.x/ir; float off=(nh-size.y)/2;
            dl->AddImage((ImTextureID)(intptr_t)TW_TWIN.id,p0,p1,ImVec2(0,off/nh),ImVec2(1,1-off/nh)); }
    }
    // HUD 좌상단
    rect_filled(ImVec2(p0.x+12,p0.y+12),ImVec2(p0.x+392,p0.y+36),alpha(IM_COL32(0,0,0,255),0.55f));
    dt(dl,FONT.ui_b,ImVec2(p0.x+20,p0.y+16),TEXT,"DIGITAL TWIN");
    dt(dl,FONT.mono_s,ImVec2(p0.x+150,p0.y+18),DIM,"TSDF SURFACE  ·  724K TRIS  ·  FRONTIER 118K");
    // HUD 우상단 RECON 게이지
    rect_filled(ImVec2(p1.x-224,p0.y+12),ImVec2(p1.x-12,p0.y+58),alpha(IM_COL32(0,0,0,255),0.55f));
    dt(dl,FONT.mono_s,ImVec2(p1.x-214,p0.y+17),FAINT,"RECON");
    { ImVec2 b0(p1.x-214,p0.y+34); rect_filled(b0,ImVec2(b0.x+150,b0.y+6),alpha(BORDER,0.8f));
      rect_filled(b0,ImVec2(b0.x+150*0.67f,b0.y+6),ACCENT); }
    dt(dl,FONT.mono,ImVec2(p1.x-56,p0.y+30),TEXT,"67%");
    dt(dl,FONT.mono_s,ImVec2(p1.x-214,p0.y+44),FAINT,"EXPLORED VOLUME");
    // 화점 마커
    dt(dl,FONT.mono_s,ImVec2(p0.x+size.x*0.31f,p0.y+size.y*0.55f),IM_COL32(0xe0,0x78,0x46,255),"\xE2\x97\x8F FIRE 620\xE2\x84\x83");
    // PiP (실사 1인칭) 좌하단
    float pw=372,ph=232; ImVec2 q0(p0.x+12,p1.y-ph-12), q1(q0.x+pw,q0.y+ph);
    if(TW_POV.id){ float ir=(float)TW_POV.w/TW_POV.h, vr=pw/ph;
        ImVec2 uv0(0,0),uv1(1,1);
        if(ir>vr){ float off=(1-vr/ir)/2; uv0.x=off; uv1.x=1-off; } else { float off=(1-ir/vr)/2; uv0.y=off; uv1.y=1-off; }
        dl->AddImage((ImTextureID)(intptr_t)TW_POV.id,q0,q1,uv0,uv1);
    } else rect_filled(q0,q1,BG_INSET);
    rect_line(q0,q1,BORDER);
    // PiP 헬멧 HUD 오버레이(절제된 AR)
    rect_filled(ImVec2(q0.x+6,q0.y+6),ImVec2(q0.x+150,q0.y+24),alpha(IM_COL32(0,0,0,255),0.6f));
    dot(ImVec2(q0.x+15,q0.y+15),4,CRIT);
    dt(dl,FONT.mono_s,ImVec2(q0.x+26,q0.y+8),TEXT,"VISOR-1 · POV");
    { // 표적 브래킷
      ImVec2 t0(q0.x+pw*0.46f,q0.y+ph*0.40f),t1(q0.x+pw*0.66f,q0.y+ph*0.78f); float g=9; ImU32 ac=IM_COL32(0xd6,0xb0,0x60,235);
      dl->AddLine(t0,ImVec2(t0.x+g,t0.y),ac,1.5f); dl->AddLine(t0,ImVec2(t0.x,t0.y+g),ac,1.5f);
      dl->AddLine(ImVec2(t1.x,t0.y),ImVec2(t1.x-g,t0.y),ac,1.5f); dl->AddLine(ImVec2(t1.x,t0.y),ImVec2(t1.x,t0.y+g),ac,1.5f);
      dl->AddLine(ImVec2(t0.x,t1.y),ImVec2(t0.x+g,t1.y),ac,1.5f); dl->AddLine(ImVec2(t0.x,t1.y),ImVec2(t0.x,t1.y-g),ac,1.5f);
      dl->AddLine(t1,ImVec2(t1.x-g,t1.y),ac,1.5f); dl->AddLine(t1,ImVec2(t1.x,t1.y-g),ac,1.5f);
      dt(dl,FONT.mono_s,ImVec2(t0.x,t0.y-15),ac,"TGT PERSON \xC3\x97""2"); }
    // 미니맵 우하단
    float mm=150; ImVec2 m0(p1.x-mm-12,p1.y-mm-12), m1(p1.x-12,p1.y-12);
    rect_filled(m0,m1,alpha(IM_COL32(0x10,0x13,0x18,255),0.92f)); rect_line(m0,m1,BORDER);
    dt(dl,FONT.mono_s,ImVec2(m0.x+8,m0.y+6),FAINT,"TACTICAL");
    ImU32 mc[4]={OK,WARN,STEEL,CRIT}; float fx[4]={.3f,.55f,.7f,.2f}, fy[4]={.4f,.6f,.35f,.72f};
    for(int i=0;i<4;++i){ ImVec2 c(m0.x+mm*fx[i],m0.y+mm*fy[i]); rect_filled(ImVec2(c.x-3,c.y-3),ImVec2(c.x+3,c.y+3),mc[i]); }
    ImGui::EndChild(); ImGui::PopStyleColor();
}

// ── 명령 리본 ───────────────────────────────────────────────────────────────
static void command_bar(ImVec2 pos, ImVec2 size){
    ImGui::SetCursorScreenPos(pos);
    panel_begin("cmd", size, "COMMAND", "선택: VISOR-1");
    ImDrawList* dl=ImGui::GetWindowDrawList();
    struct B{ const char* l; ImU32 c; }; B btns[]={{"ADVANCE",ACCENT},{"RETREAT",DIM},{"RECON",DIM},
        {"AIM + SPRAY",DIM},{"FORCE ENTRY",CRIT},{"RALLY",DIM}};
    ImVec2 p=ImGui::GetCursorScreenPos(); float bx=p.x, by=p.y+4;
    for(auto& b:btns){ ImGui::PushFont(FONT.ui_b); float tw=ImGui::CalcTextSize(b.l).x; ImGui::PopFont();
        float bw=tw+34;
        rect_filled(ImVec2(bx,by),ImVec2(bx+bw,by+44),BG_INSET);
        rect_line(ImVec2(bx,by),ImVec2(bx+bw,by+44),b.c==DIM?BORDER:b.c);
        dt(dl,FONT.ui_b,ImVec2(bx+17,by+13),b.c==DIM?TEXT:b.c,b.l); bx+=bw+10;
    }
    // 우측 자원 리드아웃
    float w=ImGui::GetContentRegionAvail().x; float rx=p.x+w;
    struct R{ const char* k; const char* v; ImU32 c; }; R res[]={{"WATER","720L",STEEL},{"O\xE2\x82\x82 MIN","18%",CRIT},{"DRONE","1",STEEL},{"CREW","2/2",STEEL}};
    for(auto& r:res){ char buf[24]; snprintf(buf,24,"%s %s",r.k,r.v);
        ImGui::PushFont(FONT.mono); float tw=ImGui::CalcTextSize(buf).x; ImGui::PopFont(); float cw=tw+24;
        rect_filled(ImVec2(rx-cw,by),ImVec2(rx,by+44),BG_PANEL); rect_line(ImVec2(rx-cw,by),ImVec2(rx,by+44),BORDER);
        dot(ImVec2(rx-cw+12,by+22),3,r.c); dt(dl,FONT.mono,ImVec2(rx-cw+22,by+15),TEXT,buf); rx-=cw+8;
    }
    panel_end();
}

// ── 상단바 / 탭 / 상태바 ────────────────────────────────────────────────────
static void chrome(){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    // 상단바
    rect_filled(ImVec2(0,0),ImVec2(WIN_W,34),BG_APP); dl->AddLine(ImVec2(0,34),ImVec2(WIN_W,34),BORDER_S);
    rect_filled(ImVec2(14,11),ImVec2(26,23),ACCENT);
    dt(dl,FONT.big,ImVec2(34,6),TEXT,"FRIZE");
    dt(dl,FONT.mono_s,ImVec2(104,13),DIM,"COMMAND OS");
    dt(dl,FONT.mono_s,ImVec2(230,13),DIM,"SITE SEOUL-HQ    INCIDENT 2026-0620-014    CONSOLE-1");
    // 우측 상태 클러스터
    float rx=WIN_W-16;
    auto pill=[&](const char* k,const char* v,ImU32 c){ char b[32]; snprintf(b,32,"%s %s",k,v);
        ImGui::PushFont(FONT.mono_s); float tw=ImGui::CalcTextSize(b).x; ImGui::PopFont();
        dl->AddText(FONT.mono_s,FONT.mono_s->FontSize,ImVec2(rx-tw,13),c,b); rx-=tw+18; };
    pill("","09:41:22Z",TEXT); pill("MQTT","OK",OK); pill("GPS","12",OK); pill("CORE","LINKED",OK);
    // 탭
    rect_filled(ImVec2(0,34),ImVec2(WIN_W,64),BG_PANEL); dl->AddLine(ImVec2(0,64),ImVec2(WIN_W,64),BORDER);
    const char* tabs[]={"OVERVIEW","TWIN","UNITS","PLAYBACK","SETTINGS"}; float tx=8;
    for(int i=0;i<5;++i){ ImGui::PushFont(FONT.ui_b); float tw=ImGui::CalcTextSize(tabs[i]).x; ImGui::PopFont(); float bw=tw+26;
        if(i==1){ rect_filled(ImVec2(tx,35),ImVec2(tx+bw,63),BG_HEAD); rect_filled(ImVec2(tx,61),ImVec2(tx+bw,63),ACCENT); }
        dt(dl,FONT.ui_b,ImVec2(tx+13,42),i==1?TEXT:DIM,tabs[i]); tx+=bw; }
    // 상태바
    rect_filled(ImVec2(0,WIN_H-22),ImVec2(WIN_W,WIN_H),BG_APP); dl->AddLine(ImVec2(0,WIN_H-22),ImVec2(WIN_W,WIN_H-22),BORDER_S);
    dt(dl,FONT.mono_s,ImVec2(12,WIN_H-16),DIM,"TWIN: TSDF SURFACE MESH  ·  DRONE + VISOR LIDAR FUSION  ·  COVERAGE 97.8%");
    dt_r(dl,FONT.mono_s,WIN_W-12,WIN_H-16,FAINT,"FRIZE \xE2\x80\x94 WE FREEZE THE CHAOS");
}

static void build_ui(){
    ImGuiViewport* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos); ImGui::SetNextWindowSize(ImVec2(WIN_W,WIN_H));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("root",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus);
    chrome();
    float M=10,G=10; float y0=64+M, y1=WIN_H-22-M;
    float lw=320, rw=332; float Hc=y1-y0;
    float cmd_h=112;
    rail_units(ImVec2(M,y0), ImVec2(lw,Hc));
    rail_right(ImVec2(WIN_W-M-rw,y0), ImVec2(rw,Hc));
    float cx0=M+lw+G, cx1=WIN_W-M-rw-G; float cw=cx1-cx0;
    center_twin(ImVec2(cx0,y0), ImVec2(cw,Hc-cmd_h-G));
    command_bar(ImVec2(cx0,y0+Hc-cmd_h), ImVec2(cw,cmd_h));
    ImGui::End(); ImGui::PopStyleVar();
}

static const ImWchar* korean_ranges(ImFontAtlas* a){
    // 한글 + 자주 쓰는 기호(— ℃ ₂ ● → · × …)를 WQY가 공급하도록 확장
    static ImVector<ImWchar> r;
    if(r.empty()){
        ImFontGlyphRangesBuilder b; b.AddRanges(a->GetGlyphRangesKorean());
        for(ImWchar c : {0x2014,0x2013,0x2103,0x2082,0x2080,0x2081,0x2083,0x25CF,0x25CB,
                         0x2192,0x00B7,0x00D7,0x2026,0x2022,0x00B0,0x2103}) b.AddChar(c);
        b.BuildRanges(&r);
    }
    return r.Data;
}
static void add_korean(ImFontAtlas* a, const char* path, float sz, ImFont* base){
    ImFontConfig cfg; cfg.MergeMode=true; cfg.PixelSnapH=true;
    a->AddFontFromFileTTF(path, sz, &cfg, korean_ranges(a));
}

int main(int argc,char** argv){
    const char* out = argc>1?argv[1]:"frize_cockpit_imgui.png";
    const char* imdir = argc>2?argv[2]:"/tmp/imgui";
    const char* twin = argc>3?argv[3]:"/tmp/twin_hero.png";
    const char* pov  = argc>4?argv[4]:"goggle_pov.jpg";
    if(!glfwInit()){ fprintf(stderr,"glfw init fail\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    GLFWwindow* win=glfwCreateWindow(WIN_W,WIN_H,"frize",nullptr,nullptr);
    if(!win){ fprintf(stderr,"glfw window fail\n"); return 1; }
    glfwMakeContextCurrent(win); glfwSwapInterval(0);

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io=ImGui::GetIO();
    io.IniFilename=nullptr; io.ConfigFlags|=ImGuiConfigFlags_NoMouseCursorChange;
    apply_style();
    // 폰트
    std::string rb=std::string(imdir)+"/misc/fonts/Roboto-Medium.ttf";
    std::string co=std::string(imdir)+"/misc/fonts/Cousine-Regular.ttf";
    const char* KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc";
    FONT.ui   = io.Fonts->AddFontFromFileTTF(rb.c_str(),17); add_korean(io.Fonts,KO,18,FONT.ui);
    FONT.ui_b = io.Fonts->AddFontFromFileTTF(rb.c_str(),17); add_korean(io.Fonts,KO,18,FONT.ui_b);
    FONT.big  = io.Fonts->AddFontFromFileTTF(rb.c_str(),24);
    FONT.small= io.Fonts->AddFontFromFileTTF(rb.c_str(),14); add_korean(io.Fonts,KO,15,FONT.small);
    FONT.mono = io.Fonts->AddFontFromFileTTF(co.c_str(),15);
    FONT.mono_s=io.Fonts->AddFontFromFileTTF(co.c_str(),13);
    io.Fonts->Build();

    ImGui_ImplGlfw_InitForOpenGL(win,true); ImGui_ImplOpenGL3_Init("#version 330");
    TW_TWIN=load_tex(twin); TW_POV=load_tex(pov);

    for(int f=0; f<4; ++f){
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        build_ui();
        ImGui::Render();
        int dw,dh; glfwGetFramebufferSize(win,&dw,&dh);
        glViewport(0,0,dw,dh); ImVec4 cc=V(BG_APP);
        glClearColor(cc.x,cc.y,cc.z,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glFinish();
        if(f==3){
            std::vector<unsigned char> px(dw*dh*4); glReadPixels(0,0,dw,dh,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
            std::vector<unsigned char> flip(dw*dh*4);
            for(int y=0;y<dh;++y) memcpy(&flip[y*dw*4], &px[(dh-1-y)*dw*4], dw*4);
            stbi_write_png(out,dw,dh,4,flip.data(),dw*4);
            printf("[imgui] saved %s (%dx%d)\n",out,dw,dh);
        }
        glfwSwapBuffers(win);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(win); glfwTerminate(); return 0;
}
