// FRIZE Cockpit (Dear ImGui) ― 실 GCS, 모노크롬. 검정 풀블리드 트윈 + 박스 없는 플랫 HUD.
// 색은 최소(흰/회색), 레드는 치명/E-STOP 한정. 구분선·강조선 없음. 제대로 된 워드마크.
// control_map.json 단일원천 = 화면 컨트롤 = CONSOLE-1 물리버튼 = 키보드.
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
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
using json=nlohmann::json;
static int WIN_W=1860, WIN_H=1000;

// ── 모노크롬 팔레트 ──
static const ImU32 BG  =IM_COL32(8,10,13,255);
static const ImU32 TX  =IM_COL32(232,237,242,255);
static const ImU32 TX2 =IM_COL32(150,160,172,255);
static const ImU32 DIMc=IM_COL32(96,105,116,255);
static const ImU32 FNT =IM_COL32(70,78,88,255);
static const ImU32 OKc =IM_COL32(96,162,124,255);
static const ImU32 WARN=IM_COL32(196,156,78,255);
static const ImU32 CRIT=IM_COL32(206,74,62,255);
static const ImU32 LINEc=IM_COL32(38,44,52,255);
static ImU32 al(ImU32 c,float a){ return (c & 0x00FFFFFF)|((ImU32)(a*255)<<24); }

struct Tex{GLuint id=0;int w=0,h=0;};
static Tex load_tex(const char* p){ Tex t;int n;unsigned char* d=stbi_load(p,&t.w,&t.h,&n,4);
    if(!d){fprintf(stderr,"tex fail %s\n",p);return t;}
    glGenTextures(1,&t.id);glBindTexture(GL_TEXTURE_2D,t.id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t.w,t.h,0,GL_RGBA,GL_UNSIGNED_BYTE,d);stbi_image_free(d);return t;}

static ImFont *UI,*UIB,*BIG,*SM,*MO,*MOS;
static ImDrawList* DL;
static void T (ImFont* f,float x,float y,ImU32 c,const char* s){ DL->AddText(f,f->FontSize,ImVec2(x,y),c,s); }
static void TR(ImFont* f,float rx,float y,ImU32 c,const char* s){ float w=f->CalcTextSizeA(f->FontSize,1e9f,0,s).x; DL->AddText(f,f->FontSize,ImVec2(rx-w,y),c,s); }
static float WD(ImFont* f,const char* s){ return f->CalcTextSizeA(f->FontSize,1e9f,0,s).x; }
// 자간 워드마크
static float spaced(ImFont* f,float x,float y,ImU32 c,const char* s,float sp){ for(const char*p=s;*p;++p){ char ch[2]={*p,0}; T(f,x,y,c,ch); x+=WD(f,ch)+sp; } return x; }

static Tex TWN,POV; static json CM; static int sel=1; static std::deque<std::string> EV; static std::string pend;
static void ev(const std::string& s){ EV.push_front(s); if(EV.size()>16)EV.pop_back(); }
static std::string ttype(){ std::string t=CM["clusters"]["select"]["controls"][sel].value("target","");
    if(t.rfind("scout",0)==0)return"scout"; if(t.rfind("visor",0)==0)return"visor"; if(t.rfind("vent",0)==0)return"vent"; return"all"; }

int main(int argc,char** argv){
    const char* out=argc>1?argv[1]:"frize_cockpit_imgui.png";
    const char* imdir=argc>2?argv[2]:"/tmp/imgui";
    const char* twin=argc>3?argv[3]:"/tmp/twin_cockpit.png";
    const char* pov=argc>4?argv[4]:"goggle_ar_view.png";
    const char* mapf=argc>5?argv[5]:"control_map.json";
    { std::ifstream f(mapf); if(!f){fprintf(stderr,"no %s\n",mapf);return 1;} f>>CM; }
    ev("CORE linked · 4 units · UWB mesh 3 anchors");
    ev("SCOUT  recon_zone  자율 프런티어 탐사 중");
    ev("SCOUT  deploy_anchor  앵커 A3 투하 → 측위망 완성");
    ev("VISOR-1  ar_arrow  진입로 화살표 송출");
    ev("CORE  hazard  2F-E 천장 처짐 67% (구조)");

    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    GLFWwindow* win=glfwCreateWindow(WIN_W,WIN_H,"frize",nullptr,nullptr); if(!win)return 1; glfwMakeContextCurrent(win);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io=ImGui::GetIO(); io.IniFilename=nullptr;
    std::string rb=std::string(imdir)+"/misc/fonts/Roboto-Medium.ttf", co=std::string(imdir)+"/misc/fonts/Cousine-Regular.ttf";
    const char* KO="/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc";
    auto ko=[&](float s){ ImFontConfig c;c.MergeMode=true; static ImVector<ImWchar> r; if(r.empty()){ImFontGlyphRangesBuilder b;
        b.AddRanges(io.Fonts->GetGlyphRangesKorean()); for(ImWchar w:{0x2014,0x2103,0x2082,0x25CF,0x2192,0x00B7,0x00D7,0x2193,0x25C6}) b.AddChar(w); b.BuildRanges(&r);} io.Fonts->AddFontFromFileTTF(KO,s,&c,r.Data); };
    UI =io.Fonts->AddFontFromFileTTF(rb.c_str(),16); ko(17);
    UIB=io.Fonts->AddFontFromFileTTF(rb.c_str(),16); ko(17);
    BIG=io.Fonts->AddFontFromFileTTF(rb.c_str(),21);
    SM =io.Fonts->AddFontFromFileTTF(rb.c_str(),13); ko(14);
    MO =io.Fonts->AddFontFromFileTTF(co.c_str(),14);
    MOS=io.Fonts->AddFontFromFileTTF(co.c_str(),12);
    io.Fonts->Build();
    ImGui_ImplGlfw_InitForOpenGL(win,true); ImGui_ImplOpenGL3_Init("#version 330");
    TWN=load_tex(twin); POV=load_tex(pov);

    for(int frame=0;frame<4;++frame){
        glfwPollEvents(); ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ImGuiViewport* vp=ImGui::GetMainViewport(); ImGui::SetNextWindowPos(vp->Pos); ImGui::SetNextWindowSize(ImVec2(WIN_W,WIN_H));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::Begin("root",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBringToFrontOnFocus);
        DL=ImGui::GetWindowDrawList();
        DL->AddRectFilled(ImVec2(0,0),ImVec2(WIN_W,WIN_H),BG);
        const float TOP=34,BOT=78,LW=288,RW=312;

        // ── 풀블리드 트윈(검정) cover-fit ──
        if(TWN.id){ float ir=(float)TWN.w/TWN.h, vr=(float)WIN_W/WIN_H; ImVec2 u0(0,0),u1(1,1);
            if(ir>vr){float o=(1-vr/ir)/2;u0.x=o;u1.x=1-o;}else{float o=(1-ir/vr)/2;u0.y=o;u1.y=1-o;}
            DL->AddImage((ImTextureID)(intptr_t)TWN.id,ImVec2(0,0),ImVec2(WIN_W,WIN_H),u0,u1); }
        // 가장자리 스크림(그라데이션, 보더 없음)
        DL->AddRectFilledMultiColor(ImVec2(0,TOP),ImVec2(LW+30,WIN_H-BOT),al(BG,0.95f),al(BG,0),al(BG,0),al(BG,0.95f));
        DL->AddRectFilledMultiColor(ImVec2(WIN_W-RW-30,TOP),ImVec2(WIN_W,WIN_H-BOT),al(BG,0),al(BG,0.95f),al(BG,0.95f),al(BG,0));
        DL->AddRectFilledMultiColor(ImVec2(0,0),ImVec2(WIN_W,TOP+10),al(BG,0.97f),al(BG,0.97f),al(BG,0),al(BG,0));
        DL->AddRectFilledMultiColor(ImVec2(0,WIN_H-BOT-12),ImVec2(WIN_W,WIN_H),al(BG,0),al(BG,0),al(BG,0.97f),al(BG,0.97f));

        // ── 맵 마커(흰 위주, 화점만 레드) ──
        struct M{float fx,fy;ImU32 c;const char*t;int k;}; // k:0 wearer 1 anchor 2 fire
        M ms[]={{0.36f,0.55f,TX,"VISOR-1",0},{0.55f,0.43f,TX,"VISOR-2",0},{0.64f,0.60f,TX,"VISOR-3",0},
                {0.42f,0.36f,DIMc,"A1",1},{0.58f,0.35f,DIMc,"A2",1},{0.50f,0.64f,DIMc,"A3",1},
                {0.31f,0.52f,CRIT,"FIRE 640℃",2},{0.70f,0.46f,CRIT,"FIRE 520℃",2}};
        for(auto&m:ms){ float x=m.fx*WIN_W,y=m.fy*WIN_H;
            if(m.k==1){ DL->AddNgon(ImVec2(x,y),4,DIMc,4,1.4f); T(MOS,x+7,y-6,DIMc,m.t); }
            else if(m.k==2){ DL->AddCircle(ImVec2(x,y),7,al(CRIT,0.6f),16,1.4f); DL->AddCircleFilled(ImVec2(x,y),2.5f,CRIT); T(MOS,x+9,y-6,CRIT,m.t); }
            else{ DL->AddCircle(ImVec2(x,y),6,al(TX,0.6f),16,1.4f); DL->AddCircleFilled(ImVec2(x,y),2.5f,TX); T(MOS,x+9,y-6,TX,m.t);
                  T(MOS,x+9,y+6,DIMc,"UWB ±0.2m"); } }

        // ── 상단: 워드마크 로고(노란박스X) + 인라인 인디케이터(구분선 없음) ──
        float lx=18; DL->AddNgon(ImVec2(lx+9,17),9,TX,6,1.8f); DL->AddNgon(ImVec2(lx+9,17),3.5f,TX,6,1.4f);
        float ex=spaced(BIG,lx+26,5,TX,"FRIZE",2.5f);
        T(MO,ex+12,12,DIMc,"COMMAND OS");
        T(MOS,LW+34,11,TX2,"SITE SEOUL-HQ   INCIDENT 2026-0620-014   3F STRUCTURE FIRE");
        { float rx=WIN_W-18; auto ind=[&](const char* s,ImU32 c){ TR(MO,rx,11,c,s); rx-=WD(MO,s)+22; };
          ind("09:41:22Z",TX); ind("UWB 3",TX2); ind("MESH OK",TX2);
          DL->AddCircleFilled(ImVec2(rx,17),3,OKc); rx-=10; ind("CORE LINKED",TX2); }

        // ── 좌: 유닛 리스트(모노크롬, 강조선 없음) ──
        { float x=16,y=TOP+16; T(MOS,x,y,FNT,"UNITS  ·  4 ONLINE"); TR(MOS,LW,y,FNT,"[1-7]"); y+=22;
          auto& cs=CM["clusters"]["select"]["controls"];
          for(int i=0;i<(int)cs.size();++i){ auto& c=cs[i]; bool s=(i==sel); float rh=46;
            if(s) DL->AddRectFilled(ImVec2(x-6,y-3),ImVec2(LW,y+rh-9),al(TX,0.07f));
            std::string led=c.value("led","green"); ImU32 tk = led=="red"?CRIT: led=="amber"?WARN: TX2;
            DL->AddRectFilled(ImVec2(x,y+4),ImVec2(x+5,y+9),tk);
            T(UIB,x+16,y-1,s?TX:al(TX,0.82f),c["label"].get<std::string>().c_str());
            T(SM,x+16,y+19,DIMc,c.value("sub","").c_str());
            TR(MOS,LW-2,y+1,s?TX:FNT,c["key"].get<std::string>().c_str());
            float bx=x+128,bw=LW-2-bx,batt=(i==1?0.62f:i==0?0.71f:i==4?0.74f:0.9f);
            DL->AddRectFilled(ImVec2(bx,y+26),ImVec2(bx+bw,y+27),al(LINEc,1));
            DL->AddRectFilled(ImVec2(bx,y+26),ImVec2(bx+bw*batt,y+27),batt<0.2f?CRIT:TX2);
            ImGui::SetCursorScreenPos(ImVec2(x-6,y-3)); ImGui::InvisibleButton(("u"+std::to_string(i)).c_str(),ImVec2(LW-x+6,rh-6));
            if(ImGui::IsItemClicked()){ sel=i; ev("SEL  "+c["label"].get<std::string>()); }
            y+=rh; }
        }
        // 좌상단 트윈 라벨(맵 위, 박스 없음)
        T(MOS,LW+34,TOP+44,al(TX,0.9f),"DIGITAL TWIN · 3F STRUCTURE");
        T(MOS,LW+34,TOP+60,DIMc,"TSDF SURFACE · UWB-LOCALIZED WEARERS · 2 FIRE SEATS");

        // ── 우상단: 탐사율(플랫 텍스트+얇은 바, 색 없음) ──
        { float rx=WIN_W-RW-34,y=TOP+44; TR(MOS,rx+150,y,FNT,"EXPLORE 78%");
          DL->AddRectFilled(ImVec2(rx+10,y+18),ImVec2(rx+150,y+20),al(LINEc,1));
          DL->AddRectFilled(ImVec2(rx+10,y+18),ImVec2(rx+10+140*0.78f,y+20),TX2); }

        // ── PiP: 선택 유닛 고글 시야(얇은 프레임) ──
        std::string tgt=CM["clusters"]["select"]["controls"][sel]["label"];
        { float pw=312,ph=176,px=LW+34,py=WIN_H-BOT-ph-16;
          if(POV.id){ float ir=(float)POV.w/POV.h,vr=pw/ph; ImVec2 u0(0,0),u1(1,1);
            if(ir>vr){float o=(1-vr/ir)/2;u0.x=o;u1.x=1-o;}else{float o=(1-ir/vr)/2;u0.y=o;u1.y=1-o;}
            DL->AddImage((ImTextureID)(intptr_t)POV.id,ImVec2(px,py),ImVec2(px+pw,py+ph),u0,u1); }
          DL->AddRect(ImVec2(px,py),ImVec2(px+pw,py+ph),al(TX,0.22f));
          DL->AddCircleFilled(ImVec2(px+10,py+11),3,CRIT); T(MOS,px+18,py+5,TX,("POV · "+tgt+" 고글").c_str()); TR(MOS,px+pw-6,py+5,DIMc,"Tab"); }

        // ── 우: 텔레메트리 + 로그(모노, 구분선 없음) ──
        { float x=WIN_W-RW+10,y=TOP+16,rr=WIN_W-14;
          T(MOS,x,y,FNT,("TELEMETRY  ·  "+tgt).c_str()); y+=22;
          auto row=[&](const char* k,const char* v,float f,ImU32 c){ T(SM,x,y,DIMc,k); TR(MO,rr,y,TX,v); y+=15;
            DL->AddRectFilled(ImVec2(x,y),ImVec2(rr,y+3),al(LINEc,1)); DL->AddRectFilled(ImVec2(x,y),ImVec2(x+(rr-x)*f,y+3),c); y+=15; };
          row("산소 O₂","62 %",0.62f,WARN); row("심박 HR","171 bpm",0.85f,WARN);
          row("SCBA 잔압","148 bar",0.49f,TX2); row("체온","38.4 ℃",0.7f,WARN);
          row("배터리","62 %",0.62f,TX2); row("측위 UWB","±0.18 m · 앵커4",0.92f,OKc);
          y+=8; T(MOS,x,y,FNT,"대기 / 가스"); y+=20;
          const char* gk[]={"CO","LEL","O₂","HCN","AMB"}; const char* gv[]={"1180 ppm","11 %","18.4 %","6 ppm","86 ℃"};
          ImU32 gc[]={CRIT,WARN,WARN,DIMc,WARN};
          for(int i=0;i<5;++i){ float gx=x+(i%3)*100, gy=y+(i/3)*30; T(MOS,gx,gy,DIMc,gk[i]); T(MO,gx,gy+13,gc[i],gv[i]); }
          y+=64; T(MOS,x,y,FNT,"EVENT LOG"); y+=18;
          int n=0; for(auto&l:EV){ ImU32 c= l.find("hazard")!=std::string::npos||l.find("E-STOP")!=std::string::npos?CRIT:DIMc;
            DL->AddText(MOS,MOS->FontSize,ImVec2(x,y+n*15),c,l.c_str()); if(++n>=12)break; } }

        // ── 하단: 맥락 명령 스트립(플랫, 컬러틱 없음) + E-STOP ──
        { std::string tt=ttype(); float x=18,by=WIN_H-BOT;
          std::string hdr="COMMAND  ·  "+tgt; T(MOS,x,by+10,FNT,hdr.c_str()); x+=WD(MOS,hdr.c_str())+24;
          int idx=0;
          for(auto& c:CM["clusters"]["command"]["controls"]){ bool ok=false; for(auto& z:c["units"]) if(z.get<std::string>()==tt) ok=true; if(!ok)continue;
            std::string lbl=c["label"],key=c["key"]; bool danger=c.value("confirm",false)||c.value("color","")=="red";
            bool armed=(pend==c.value("action","")); ImU32 lc=danger?CRIT:TX;
            float cw=WD(UIB,lbl.c_str()); if(WD(MOS,key.c_str())>cw)cw=WD(MOS,key.c_str()); cw+=14; float cy=by+12;
            if(armed) DL->AddRectFilled(ImVec2(x-4,cy-2),ImVec2(x+cw+4,cy+44),al(WARN,0.16f));
            T(UIB,x,cy,lc,lbl.c_str());
            if(c.contains("sub")) T(SM,x,cy+20,DIMc,c["sub"].get<std::string>().c_str());
            float ky=cy+(c.contains("sub")?38:22); float kw=WD(MOS,key.c_str());
            DL->AddRectFilled(ImVec2(x-2,ky-1),ImVec2(x+kw+3,ky+13),al(TX,0.08f)); T(MOS,x+1,ky,DIMc,key.c_str());
            ImGui::SetCursorScreenPos(ImVec2(x-4,cy-2)); ImGui::InvisibleButton(c["id"].get<std::string>().c_str(),ImVec2(cw+8,46));
            if(ImGui::IsItemClicked()){ std::string a=c.value("action",lbl);
              if(danger){pend=a;ev("ARM  "+a+" → CONFIRM 대기");}
              else if(a=="confirm"){ if(!pend.empty()){ev("EXEC "+pend+" → "+tgt+" (인터록 통과)");pend.clear();} else ev("CONFIRM (대기 명령 없음)"); }
              else ev("CMD  "+a+" → "+tgt); }
            x+=cw+20; ++idx; }
          // E-STOP(유일 강조)
          float ew=156,exx=WIN_W-ew-16,ey=by+12;
          DL->AddRectFilled(ImVec2(exx,ey),ImVec2(exx+ew,ey+46),al(CRIT,0.14f)); DL->AddRect(ImVec2(exx,ey),ImVec2(exx+ew,ey+46),CRIT,0,0,1.4f);
          DL->AddCircleFilled(ImVec2(exx+24,ey+23),12,CRIT);
          T(UIB,exx+42,ey+6,CRIT,"E-STOP"); T(MOS,exx+42,ey+26,al(CRIT,0.85f),"전대원 후퇴 · Space×2");
          ImGui::SetCursorScreenPos(ImVec2(exx,ey)); ImGui::InvisibleButton("estop",ImVec2(ew,46));
          if(ImGui::IsItemClicked()) ev("E-STOP  전 대원 후퇴 브로드캐스트!");
        }
        T(MOS,18,WIN_H-15,FNT,"화면 컨트롤 = CONSOLE-1 물리버튼 = 키보드 · 마우스 동일 · control_map 단일원천");

        ImGui::End(); ImGui::PopStyleVar(); ImGui::Render();
        int dw,dh; glfwGetFramebufferSize(win,&dw,&dh); glViewport(0,0,dw,dh);
        glClearColor(8/255.f,10/255.f,13/255.f,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glFinish();
        if(frame==3){ std::vector<unsigned char> px(dw*dh*4); glReadPixels(0,0,dw,dh,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
            std::vector<unsigned char> fl(dw*dh*4); for(int y=0;y<dh;++y) memcpy(&fl[y*dw*4],&px[(dh-1-y)*dw*4],dw*4);
            stbi_write_png(out,dw,dh,4,fl.data(),dw*4); printf("[imgui] saved %s (%dx%d)\n",out,dw,dh); }
        glfwSwapBuffers(win);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); glfwDestroyWindow(win); glfwTerminate(); return 0;
}
