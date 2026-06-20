// FRIZE Cockpit (Dear ImGui) ― 실 GCS 톤. 풀블리드 트윈 + 박스 없는 플랫 오버레이.
//
// QGroundControl Fly View / ATAK 처럼: 3D 트윈이 화면 전체 배경, 계기·리드아웃은
// 가장자리 스크림 위에 보더 없이 떠 있다. control_map.json 단일원천이 UI를 구동 —
// 화면 모든 컨트롤 = CONSOLE-1 물리버튼 = 키보드. 카드/박스 일절 없음.
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
#include "json.hpp"
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
#include <deque>

using namespace ui;
using json = nlohmann::json;
static int WIN_W=1860, WIN_H=1000;

struct Tex { GLuint id=0; int w=0,h=0; };
static Tex load_tex(const char* p){ Tex t; int n; unsigned char* d=stbi_load(p,&t.w,&t.h,&n,4);
    if(!d){fprintf(stderr,"tex fail %s\n",p);return t;}
    glGenTextures(1,&t.id); glBindTexture(GL_TEXTURE_2D,t.id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t.w,t.h,0,GL_RGBA,GL_UNSIGNED_BYTE,d); stbi_image_free(d); return t; }

static ImDrawList* DL;
static void T (ImFont* f, float x, float y, ImU32 c, const char* s){ DL->AddText(f,f->FontSize,ImVec2(x,y),c,s); }
static void TR(ImFont* f, float rx, float y, ImU32 c, const char* s){ ImGui::PushFont(f); float w=ImGui::CalcTextSize(s).x; ImGui::PopFont(); DL->AddText(f,f->FontSize,ImVec2(rx-w,y),c,s); }
static float TW(ImFont* f,const char* s){ ImGui::PushFont(f); float w=ImGui::CalcTextSize(s).x; ImGui::PopFont(); return w; }
static void hline(float x0,float x1,float y,ImU32 c){ DL->AddLine(ImVec2(x0,y),ImVec2(x1,y),c); }
static void vline(float x,float y0,float y1,ImU32 c){ DL->AddLine(ImVec2(x,y0),ImVec2(x,y1),c); }
static ImU32 color_of(const std::string& k){ if(k=="red")return CRIT; if(k=="amber"||k=="orange")return ACCENT;
    if(k=="cyan")return STEEL; if(k=="green")return OK; return DIM; }

static Tex TW_TWIN, TW_POV; static json CM;
static int sel_unit=1; static std::deque<std::string> EV; static std::string pending;
static void ev(const std::string& s){ EV.push_front(s); if(EV.size()>16) EV.pop_back(); }

static std::string target_type(){ std::string t=CM["clusters"]["select"]["controls"][sel_unit].value("target","");
    if(t.rfind("scout",0)==0)return"scout"; if(t.rfind("visor",0)==0)return"visor";
    if(t.rfind("vent",0)==0)return"vent"; return "all"; }

// 가장자리 스크림(맵 위 가독성). 보더 없음 ― 알파 그라데이션만.
static void scrim_l(float x0,float w,float y0,float y1){ DL->AddRectFilledMultiColor(ImVec2(x0,y0),ImVec2(x0+w,y1),
    alpha(BG_APP,0.92f),alpha(BG_APP,0.0f),alpha(BG_APP,0.0f),alpha(BG_APP,0.92f)); }
static void scrim_r(float x1,float w,float y0,float y1){ DL->AddRectFilledMultiColor(ImVec2(x1-w,y0),ImVec2(x1,y1),
    alpha(BG_APP,0.0f),alpha(BG_APP,0.92f),alpha(BG_APP,0.92f),alpha(BG_APP,0.0f)); }
static void scrim_v(float x0,float x1,float y0,float h,bool top){ ImU32 a=alpha(BG_APP,0.94f),z=alpha(BG_APP,0.0f);
    if(top) DL->AddRectFilledMultiColor(ImVec2(x0,y0),ImVec2(x1,y0+h),a,a,z,z);
    else    DL->AddRectFilledMultiColor(ImVec2(x0,y0),ImVec2(x1,y0+h),z,z,a,a); }

// 작은 키 칩(보더 최소 ― 모노 글자 + 옅은 밑줄)
static void keychip(float x,float y,const char* k,ImU32 c){ float w=TW(FONT.mono_s,k);
    DL->AddRectFilled(ImVec2(x-2,y-1),ImVec2(x+w+3,y+13),alpha(c,0.14f));
    T(FONT.mono_s,x+1,y,c,k); }

int hovered_cmd=-1;

int main(int argc,char** argv){
    const char* out=argc>1?argv[1]:"frize_cockpit_imgui.png";
    const char* imdir=argc>2?argv[2]:"/tmp/imgui";
    const char* twin=argc>3?argv[3]:"/tmp/twin_hero.png";
    const char* pov=argc>4?argv[4]:"goggle_pov.jpg";
    const char* mapf=argc>5?argv[5]:"control_map.json";
    { std::ifstream f(mapf); if(!f){fprintf(stderr,"no %s\n",mapf);return 1;} f>>CM; }
    ev("CORE LINKED · 4 units · UWB 3 anchors live");
    ev("SCOUT  recon_zone  자율 프런티어 탐사");
    ev("SCOUT  deploy_anchor  앵커 A3 투하 → 측위망 완성");
    ev("VISOR-1  ar_arrow  진입로 → 화살표 표시");

    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    GLFWwindow* win=glfwCreateWindow(WIN_W,WIN_H,"frize",nullptr,nullptr); if(!win)return 1;
    glfwMakeContextCurrent(win);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io=ImGui::GetIO(); io.IniFilename=nullptr;
    apply_style();
    std::string rb=std::string(imdir)+"/misc/fonts/Roboto-Medium.ttf", co=std::string(imdir)+"/misc/fonts/Cousine-Regular.ttf";
    const char* KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc";
    auto ko=[&](float s){ ImFontConfig c; c.MergeMode=true; static ImVector<ImWchar> r; if(r.empty()){ ImFontGlyphRangesBuilder b;
        b.AddRanges(io.Fonts->GetGlyphRangesKorean()); for(ImWchar w:{0x2014,0x2103,0x2082,0x25CF,0x2192,0x00B7,0x00D7,0x2693,0x2191,0x25B8}) b.AddChar(w); b.BuildRanges(&r);} io.Fonts->AddFontFromFileTTF(KO,s,&c,r.Data); };
    FONT.ui=io.Fonts->AddFontFromFileTTF(rb.c_str(),16); ko(17);
    FONT.ui_b=io.Fonts->AddFontFromFileTTF(rb.c_str(),16); ko(17);
    FONT.big=io.Fonts->AddFontFromFileTTF(rb.c_str(),22);
    FONT.small=io.Fonts->AddFontFromFileTTF(rb.c_str(),13); ko(14);
    FONT.mono=io.Fonts->AddFontFromFileTTF(co.c_str(),14);
    FONT.mono_s=io.Fonts->AddFontFromFileTTF(co.c_str(),12);
    io.Fonts->Build();
    ImGui_ImplGlfw_InitForOpenGL(win,true); ImGui_ImplOpenGL3_Init("#version 330");
    TW_TWIN=load_tex(twin); TW_POV=load_tex(pov);

    for(int frame=0;frame<4;++frame){
        glfwPollEvents(); ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ImGuiViewport* vp=ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos); ImGui::SetNextWindowSize(ImVec2(WIN_W,WIN_H));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::Begin("root",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus);
        DL=ImGui::GetWindowDrawList();
        const float TOP=30, BOT=72, LW=272, RW=300;

        // ── 풀블리드 트윈(배경) ──
        if(TW_TWIN.id){ float ir=(float)TW_TWIN.w/TW_TWIN.h, vr=(float)WIN_W/WIN_H;
            ImVec2 a(0,0),b(WIN_W,WIN_H); ImVec2 u0(0,0),u1(1,1);
            if(ir>vr){float o=(1-vr/ir)/2;u0.x=o;u1.x=1-o;}else{float o=(1-ir/vr)/2;u0.y=o;u1.y=1-o;}
            DL->AddImage((ImTextureID)(intptr_t)TW_TWIN.id,a,b,u0,u1); }
        // 좌우 스크림(리드아웃 가독성)
        scrim_l(0,LW+40,TOP,WIN_H-BOT); scrim_r(WIN_W,RW+40,TOP,WIN_H-BOT);
        scrim_v(0,WIN_W,0,TOP+6,true); scrim_v(0,WIN_W,WIN_H-BOT-8,BOT+8,false);

        // ── 맵 마커(대원 UWB 위치 + 앵커 + 화점) ── 박스 없이 점/라벨만
        struct M{float fx,fy;ImU32 c;const char*t;bool anchor;};
        M ms[]={{0.34f,0.56f,ACCENT,"VISOR-1",0},{0.54f,0.42f,OK,"VISOR-2",0},{0.66f,0.60f,OK,"VISOR-3",0},
                {0.30f,0.52f,CRIT,"FIRE 620℃",0},{0.42f,0.34f,STEEL,"A1",1},{0.60f,0.33f,STEEL,"A2",1},{0.50f,0.66f,STEEL,"A3",1}};
        for(auto&m:ms){ float x=m.fx*WIN_W,y=m.fy*WIN_H;
            if(m.anchor){ DL->AddNgonFilled(ImVec2(x,y),5,STEEL,3); T(FONT.mono_s,x+8,y-6,STEEL,m.t); }
            else{ DL->AddCircle(ImVec2(x,y),7,alpha(m.c,0.5f),16,1.5f); DL->AddCircleFilled(ImVec2(x,y),3,m.c);
                  T(FONT.mono_s,x+9,y-6,m.c,m.t); } }

        // ── 상단 바: 인라인 인디케이터(박스 없음, 구분선만) ──
        DL->AddRectFilled(ImVec2(14,9),ImVec2(25,21),ACCENT);
        T(FONT.big,33,4,TEXT,"FRIZE");
        T(FONT.mono_s,108,11,DIM,"COMMAND OS");
        vline(190,7,23,BORDER); T(FONT.mono_s,202,11,DIM,"SITE SEOUL-HQ");
        vline(330,7,23,BORDER); T(FONT.mono_s,342,11,DIM,"INCIDENT 2026-0620-014");
        { float rx=WIN_W-16; auto ind=[&](const char* k,const char* v,ImU32 c){ TR(FONT.mono_s,rx,11,c,v);
            rx-=TW(FONT.mono_s,v)+6; TR(FONT.mono_s,rx,11,FAINT,k); rx-=TW(FONT.mono_s,k)+10; vline(rx,7,23,BORDER); rx-=12; };
          ind("","09:41:22Z",TEXT); ind("UWB ","3 ANCH",OK); ind("MESH ","OK",OK); ind("CORE ","LINKED",OK); }
        hline(0,WIN_W,TOP,BORDER_S);

        // ── 좌측: 유닛 리스트(플랫, 하이라이트=좌측 액센트선) ──
        { float x=16,y=TOP+18; T(FONT.mono_s,x,y,FAINT,"UNITS"); TR(FONT.mono_s,LW,y,FAINT,"[1-7]"); y+=8;
          hline(x,LW,y+8,BORDER_S); y+=18;
          auto& cs=CM["clusters"]["select"]["controls"];
          for(int i=0;i<(int)cs.size();++i){ auto& c=cs[i]; bool s=(i==sel_unit); float rh=44;
            if(s){ DL->AddRectFilledMultiColor(ImVec2(x-6,y-4),ImVec2(LW,y+rh-8),alpha(ACCENT,0.10f),alpha(ACCENT,0.0f),alpha(ACCENT,0.0f),alpha(ACCENT,0.10f));
                   DL->AddRectFilled(ImVec2(x-6,y-4),ImVec2(x-3,y+rh-8),ACCENT); }
            ImU32 led=color_of(c.value("led","green"));
            DL->AddCircleFilled(ImVec2(x+4,y+8),3.5f,led);
            T(FONT.ui_b,x+16,y-1,s?TEXT:alpha(TEXT,0.82f),c["label"].get<std::string>().c_str());
            T(FONT.small,x+16,y+18,DIM,c.value("sub","").c_str());
            char k[6]; snprintf(k,6,"%s",c["key"].get<std::string>().c_str());
            TR(FONT.mono_s,LW-2,y+1,s?ACCENT:FAINT,k);
            // 미니 상태(데모): 신호/배터리 바
            float bx=x+120,bw=LW-2-bx; float batt=(i==1?0.62f:i==0?0.71f:0.9f);
            DL->AddRectFilled(ImVec2(bx,y+24),ImVec2(bx+bw,y+27),alpha(BORDER,0.8f));
            DL->AddRectFilled(ImVec2(bx,y+24),ImVec2(bx+bw*batt,y+27),led);
            ImGui::SetCursorScreenPos(ImVec2(x-6,y-4)); ImGui::InvisibleButton(("u"+std::to_string(i)).c_str(),ImVec2(LW-x+6,rh-4));
            if(ImGui::IsItemClicked()){ sel_unit=i; ev("SEL  "+c["label"].get<std::string>()); }
            hline(x,LW,y+rh-6,BORDER_S); y+=rh; }
        }

        // ── 좌상단 트윈 HUD + 우상단 탐사 게이지(오버레이, 박스 없음) ──
        { float gx=WIN_W-RW-24; T(FONT.mono_s,LW+30,TOP+18,alpha(TEXT,0.9f),"DIGITAL TWIN");
          T(FONT.mono_s,LW+30,TOP+34,DIM,"TSDF SURFACE · 724K TRIS · COVERAGE 97.8%");
          // 우상단: 탐사율 게이지(아크)
          float cx=gx, cy=TOP+44; DL->AddText(FONT.mono_s,12,ImVec2(cx-34,cy-40),FAINT,"EXPLORE");
          DL->PathArcTo(ImVec2(cx,cy),26,3.4f,3.4f+5.2f*0.78f,40); DL->PathStroke(ACCENT,0,4);
          DL->PathArcTo(ImVec2(cx,cy),26,3.4f+5.2f*0.78f,3.4f+5.2f,40); DL->PathStroke(alpha(BORDER,0.8f),0,4);
          T(FONT.ui_b,cx-15,cy-9,TEXT,"78%"); }

        // ── PiP: 선택 유닛 1인칭(얇은 프레임만) ──
        std::string tgt=CM["clusters"]["select"]["controls"][sel_unit]["label"];
        { float pw=300,ph=186, px=LW+30, py=WIN_H-BOT-ph-18;
          if(TW_POV.id){ float ir=(float)TW_POV.w/TW_POV.h,vr=pw/ph; ImVec2 u0(0,0),u1(1,1);
            if(ir>vr){float o=(1-vr/ir)/2;u0.x=o;u1.x=1-o;}else{float o=(1-ir/vr)/2;u0.y=o;u1.y=1-o;}
            DL->AddImage((ImTextureID)(intptr_t)TW_POV.id,ImVec2(px,py),ImVec2(px+pw,py+ph),u0,u1); }
          DL->AddRect(ImVec2(px,py),ImVec2(px+pw,py+ph),alpha(TEXT,0.25f));
          DL->AddCircleFilled(ImVec2(px+10,py+11),3,CRIT);
          T(FONT.mono_s,px+18,py+5,TEXT,("POV · "+tgt).c_str()); TR(FONT.mono_s,px+pw-6,py+5,ACCENT,"Tab"); }

        // ── 우측: 텔레메트리 + 로그(플랫) ──
        { float x=WIN_W-RW+8,y=TOP+18,rr=WIN_W-14;
          T(FONT.mono_s,x,y,FAINT,("TELEMETRY · "+tgt).c_str()); hline(x,rr,y+16,BORDER_S); y+=28;
          auto bar=[&](const char* k,const char* v,float f,ImU32 c){ T(FONT.small,x,y,DIM,k); TR(FONT.mono_s,rr,y,TEXT,v);
            y+=16; DL->AddRectFilled(ImVec2(x,y),ImVec2(rr,y+4),alpha(BORDER,0.8f)); DL->AddRectFilled(ImVec2(x,y),ImVec2(x+(rr-x)*f,y+4),c); y+=16; };
          bar("산소 O₂","62%",0.62f,ACCENT); bar("심박 HR","171 bpm",0.85f,ACCENT);
          bar("SCBA 잔압","148 bar",0.49f,STEEL); bar("배터리","62%",0.62f,OK);
          bar("측위 UWB","±0.18 m",0.9f,OK);
          y+=6; T(FONT.mono_s,x,y,FAINT,"EVENT LOG"); hline(x,rr,y+16,BORDER_S); y+=24;
          int n=0; for(auto&l:EV){ ImU32 c= l.find("E-STOP")!=std::string::npos?CRIT: l.find("CONFIRM")!=std::string::npos?ACCENT:DIM;
            DL->AddText(FONT.mono_s,FONT.mono_s->FontSize,ImVec2(x,y+n*16),c,l.c_str()); if(++n>=14)break; } }

        // ── 하단: 명령 액션 스트립(맥락별, 플랫 키캡) + E-STOP ──
        hline(0,WIN_W,WIN_H-BOT,BORDER_S);
        { std::string tt=target_type(); float x=18, by=WIN_H-BOT;
          T(FONT.mono_s,x,by+8,FAINT,("COMMAND · "+tgt).c_str()); x+=TW(FONT.mono_s,("COMMAND · "+tgt).c_str())+18; vline(x-9,by+8,by+BOT-10,BORDER);
          int idx=0;
          for(auto& c:CM["clusters"]["command"]["controls"]){
            bool ok=false; for(auto& z:c["units"]) if(z.get<std::string>()==tt) ok=true;
            if(!ok) continue;
            std::string lbl=c["label"]; std::string key=c["key"];
            ImU32 cc=color_of(c.value("color","steel")); bool danger=c.value("confirm",false)||c.value("color","")=="red";
            float lw=TW(FONT.ui_b,lbl.c_str()), kw=TW(FONT.mono_s,key.c_str());
            float cw=(lw>kw?lw:kw)+18; float cy=by+10;
            bool active=(hovered_cmd==idx);
            // 플랫: 상단 컬러 틱 + 라벨 + 키칩, 보더 없음
            DL->AddRectFilled(ImVec2(x,cy),ImVec2(x+3,cy+44),cc);
            T(FONT.ui_b,x+12,cy+2,danger?cc:TEXT,lbl.c_str());
            if(c.contains("sub")) T(FONT.small,x+12,cy+22,DIM,c["sub"].get<std::string>().c_str());
            keychip(x+12,cy+(c.contains("sub")?40:24),key.c_str(),cc);
            ImGui::SetCursorScreenPos(ImVec2(x,cy)); ImGui::InvisibleButton(c["id"].get<std::string>().c_str(),ImVec2(cw+8,46));
            if(ImGui::IsItemClicked()){ std::string a=c.value("action",lbl);
              if(danger){ pending=a; ev("ARM  "+a+" → CONFIRM 대기"); }
              else if(a=="confirm"){ if(!pending.empty()){ ev("EXEC "+pending+" → "+tgt+" (인터록 통과)"); pending.clear(); } else ev("CONFIRM (대기 명령 없음)"); }
              else ev("CMD  "+a+" → "+tgt); }
            x+=cw+18; ++idx;
          }
          // E-STOP 우측 고정(유일하게 강조)
          float ew=150, ex=WIN_W-ew-16, ey=by+10;
          DL->AddRectFilled(ImVec2(ex,ey),ImVec2(ex+ew,ey+46),alpha(CRIT,0.16f));
          DL->AddRect(ImVec2(ex,ey),ImVec2(ex+ew,ey+46),CRIT,0,0,1.5f);
          DL->AddCircleFilled(ImVec2(ex+24,ey+23),13,CRIT);
          T(FONT.ui_b,ex+42,ey+7,CRIT,"E-STOP"); T(FONT.mono_s,ex+42,ey+27,alpha(CRIT,0.85f),"전대원 후퇴 · Space×2");
          ImGui::SetCursorScreenPos(ImVec2(ex,ey)); ImGui::InvisibleButton("estop",ImVec2(ew,46));
          if(ImGui::IsItemClicked()) ev("E-STOP  전 대원 후퇴 브로드캐스트!");
          vline(ex-12,by+10,by+BOT-10,BORDER);
        }
        // 하단 상태 한 줄
        T(FONT.mono_s,18,WIN_H-15,FAINT,"화면 컨트롤 = CONSOLE-1 물리버튼 = 키보드 · 마우스 클릭 동일 · control_map 단일원천");

        ImGui::End(); ImGui::PopStyleVar();
        ImGui::Render();
        int dw,dh; glfwGetFramebufferSize(win,&dw,&dh); glViewport(0,0,dw,dh);
        ImVec4 cc=V(BG_APP); glClearColor(cc.x,cc.y,cc.z,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glFinish();
        if(frame==3){ std::vector<unsigned char> px(dw*dh*4); glReadPixels(0,0,dw,dh,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
            std::vector<unsigned char> fl(dw*dh*4); for(int y=0;y<dh;++y) memcpy(&fl[y*dw*4],&px[(dh-1-y)*dw*4],dw*4);
            stbi_write_png(out,dw,dh,4,fl.data(),dw*4); printf("[imgui] saved %s (%dx%d)\n",out,dw,dh); }
        glfwSwapBuffers(win);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(win); glfwTerminate(); return 0;
}
