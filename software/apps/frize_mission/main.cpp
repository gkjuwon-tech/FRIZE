// frize_mission ― 작전 시뮬 타임라인 생성기 (콕핏 조종 시연 영상의 데이터 소스)
//
// "내가 만든 트윈에 실제로" 드론 여러 대를 굴려:
//   - 공유 TSDF 를 프레임마다 채워나가고(트윈이 자라남)
//   - 드론 레이가 벽에 막히고(blocked), 음영이 생기고
//   - 드론 카메라(=드론 VLM)가 생존자(person 박스)를 LOS 로 먼저 발견 → 탐지 이벤트
//   - 콕핏이 그 탐지를 받아 고글에 AR 명령 → 구조 비트
//
// 출력:
//   <out>/points.bin   누적 스캔 포인트 {x,y,z f32}{r,g,b u8}{agent u8}{reveal u16}
//                      reveal_frame ≤ 현재프레임 인 점만 그리면 트윈이 채워지는 애니메이션.
//   <out>/mission.json 프레임별 에이전트 포즈/탐사율 + 이벤트(탐지/AR/구조) 타임라인.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <fstream>
#include <algorithm>

#include "frize/recon/tsdf.hpp"
#include "frize/recon/mesh.hpp"
#include "frize/sim/building.hpp"
#include "frize/sim/lidar.hpp"
#include "frize/sim/agents.hpp"

using namespace frize::recon;
using namespace frize::sim;

// ── 경로 헬퍼: 웨이포인트 → 등간격 보간 ──
static std::vector<Pose> mkpath(const std::vector<Vec3f>& wp, float step, bool spin){
    std::vector<Vec3f> pts;
    for (size_t i=0;i+1<wp.size();++i){ Vec3f a=wp[i],b=wp[i+1];
        Vec3f d{b.x-a.x,b.y-a.y,b.z-a.z}; float L=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
        int n=std::max(1,(int)(L/step)); for(int s=0;s<n;++s){float t=(float)s/n; pts.push_back({a.x+d.x*t,a.y+d.y*t,a.z+d.z*t});} }
    if(!wp.empty()) pts.push_back(wp.back());
    std::vector<Pose> out; float yaw=0;
    for (size_t i=0;i<pts.size();++i){ Vec3f a=pts[i],b=pts[std::min(pts.size()-1,i+1)];
        float hy = spin ? (yaw+=0.45f) : std::atan2(b.y-a.y,b.x-a.x);
        out.push_back({a,hy,-0.10f}); }
    return out;
}

struct Agent { std::string id, type; std::vector<Pose> path; Lidar lidar; };

int main(int argc,char**argv){
    std::string out = argc>1?argv[1]:"/tmp/mission";
    int NF        = argc>2?std::atoi(argv[2]):170;   // 프레임 수
    float voxel   = argc>3?std::atof(argv[3]):0.08f;
    int fps       = argc>4?std::atoi(argv[4]):20;

    Building bld;
    Vec3f org{-1,-1,-0.6f};
    int nx=(int)std::ceil((bld.W+2)/voxel), ny=(int)std::ceil((bld.D+2)/voxel), nz=(int)std::ceil((bld.H+1.2f)/voxel);
    TsdfVolume vol(org,{nx,ny,nz},voxel);

    // person 박스 인덱스(가시성 판정용)
    std::vector<int> person_idx;
    for (int i=0;i<(int)bld.boxes().size();++i) if (std::strcmp(bld.boxes()[i].kind,"person")==0) person_idx.push_back(i);

    // ── 에이전트: 드론 3대 + 대원 3명(단층 복도+병실, 26×12) ──
    //  대원 3명은 영상의 서로 다른 클립 = 같은 건물의 여러 시야. VISOR-3은 위험대원.
    std::vector<Agent> ag;
    ag.push_back({"SCOUT-1","scout", mkpath({ // 남측 병실 정찰
        {-2,6,1.8f},{1.5f,6,1.8f},{3.25f,2.5f,1.8f},{3.25f,4.2f,1.8f},{9.75f,2.5f,1.9f},{9.75f,4.2f,1.9f},
        {16,2.5f,1.9f},{16,4.2f,1.9f},{22.5f,2.5f,2.0f},{22.5f,4.2f,2.0f},{12,6,1.9f}
    },0.5f,true), drone_lidar(1)});
    ag.push_back({"SCOUT-2","scout", mkpath({ // 북측 병실 정찰(요구조자2/화점2)
        {-1.5f,6,1.6f},{2,6,1.6f},{3.25f,9.5f,1.7f},{3.25f,8.0f,1.7f},{9.75f,9.5f,1.8f},{16,9.2f,1.8f},
        {16,8.0f,1.8f},{22.5f,9.5f,1.9f},{12,6,1.8f},{4,6,1.7f}
    },0.5f,true), drone_lidar(7)});
    ag.push_back({"SCOUT-3","scout", mkpath({ // 복도 고공 + 동측 끝방
        {-1,6,1.5f},{2,6,2.4f},{8,6,2.4f},{16,6,2.4f},{22,6,2.4f},{22.5f,3,2.2f},{22.5f,9.5f,2.2f},
        {16,6,2.3f},{8,6,2.3f},{3,6,2.2f}
    },0.5f,true), drone_lidar(13)});
    ag.push_back({"VISOR-1","visor", mkpath({ // 대원1: 남측 수색 → 요구조자1
        {-1.5f,6,1.5f},{2,5.9f,1.5f},{9.75f,5.9f,1.5f},{9.75f,3.0f,1.5f},{9.75f,5.9f,1.5f},{16,5.9f,1.5f},
        {22.5f,5.9f,1.5f},{22.8f,2.6f,1.5f},{22.5f,5.9f,1.5f}
    },0.42f,false), visor_depth(2)});
    ag.push_back({"VISOR-2","visor", mkpath({ // 대원2: 북측 수색 → 요구조자2
        {-1.2f,6,1.5f},{2,6.1f,1.5f},{3.25f,6.1f,1.5f},{3.25f,9.3f,1.5f},{3.25f,6.1f,1.5f},{9.75f,6.1f,1.5f},
        {9.75f,9.3f,1.5f},{9.75f,6.1f,1.5f}
    },0.42f,false), visor_depth(4)});
    ag.push_back({"VISOR-3","visor", mkpath({ // 대원3(위험): 남서 주방화재 방으로 과진입 → 후퇴
        {-1,6,1.5f},{2,5.9f,1.5f},{3.25f,5.9f,1.5f},{3.25f,2.8f,1.5f},{3.4f,2.0f,1.5f},
        {3.25f,5.9f,1.5f},{2,6,1.5f}
    },0.42f,false), visor_depth(6)});

    // ── 포인트 누적 버퍼 ──
    struct Rec{ float x,y,z; uint8_t r,g,b,a; uint16_t reveal; };
    std::vector<Rec> recs; recs.reserve(2'000'000);
    auto push_pt=[&](const ScanPoint& sp,int agi,int f){
        Vec3f col=Mesh::thermal_color(sp.temp_c);
        recs.push_back({sp.p.x,sp.p.y,sp.p.z,
            (uint8_t)std::min(255.f,col.x*255),(uint8_t)std::min(255.f,col.y*255),(uint8_t)std::min(255.f,col.z*255),
            (uint8_t)agi,(uint16_t)f});
    };

    // ── 타임라인 누적 ──
    std::string frames_js="["; std::string events_js="[";
    std::vector<int> detected(person_idx.size(),-1);   // 각 생존자 최초 탐지 프레임
    bool ev_first=true; auto add_ev=[&](const std::string& j){ events_js += (ev_first?"":",") + j; ev_first=false; };
    int sub=0;

    for (int f=0; f<NF; ++f){
        if (f) frames_js += ",";
        frames_js += "{\"f\":"+std::to_string(f)+",\"agents\":[";
        for (size_t a=0;a<ag.size();++a){
            auto& A=ag[a];
            size_t pi = A.path.empty()?0: (size_t)((double)f/NF * A.path.size());
            pi = std::min(pi, A.path.size()-1);
            Pose P = A.path.empty()?Pose{{0,0,0},0,0}:A.path[pi];

            // 스캔 융합(트윈 채우기) — 포인트는 서브샘플로 적재
            auto pts = A.lidar.scan(bld,P.pos,P.yaw,P.pitch);
            vol.integrate_scan(P.pos,pts);
            for (auto& sp:pts){ if((sub++ %4)==0) push_pt(sp,(int)a,f); }

            // 벽에 막힘? 전방 1.0m 내 벽 히트
            Vec3f fwd{std::cos(P.yaw),std::sin(P.yaw),0}; float t; int bi; bool blocked=false;
            if (bld.raycast(P.pos,fwd,1.0f,t,bi)) blocked=true;

            // 드론 VLM: 생존자 LOS 가시성(드론만)
            if (A.type=="scout"){
                for (size_t v=0;v<person_idx.size();++v){
                    int pb=person_idx[v];
                    const Box& B=bld.boxes()[pb];
                    Vec3f c{(B.lo.x+B.hi.x)/2,(B.lo.y+B.hi.y)/2,(B.lo.z+B.hi.z)/2};
                    Vec3f d{c.x-P.pos.x,c.y-P.pos.y,c.z-P.pos.z};
                    float L=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z); if(L<0.1f||L>14.f) continue;
                    d.x/=L;d.y/=L;d.z/=L;
                    float tt; int hb;
                    if (bld.raycast(P.pos,d,L+0.4f,tt,hb) && hb==pb && detected[v]<0){
                        detected[v]=f;
                        char buf[400];
                        std::snprintf(buf,sizeof(buf),
                          "{\"f\":%d,\"kind\":\"detect\",\"by\":\"%s\",\"target\":\"person%zu\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"conf\":%.2f,\"floor\":%d,\"text\":\"비반응 인원 1명 — %s 카메라\"}",
                          f,A.id.c_str(),v+1,c.x,c.y,c.z, 0.88+0.03*v, (int)(c.z/3.2f)+1, A.id.c_str());
                        add_ev(buf);
                    }
                }
            }
            char ab[256];
            std::snprintf(ab,sizeof(ab),"%s{\"id\":\"%s\",\"type\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"yaw\":%.3f,\"blocked\":%s,\"npts\":%zu}",
                a?",":"", A.id.c_str(),A.type.c_str(),P.pos.x,P.pos.y,P.pos.z,P.yaw, blocked?"true":"false", pts.size());
            frames_js += ab;
        }
        char fb[128];
        std::snprintf(fb,sizeof(fb),"],\"explored\":%.4f,\"observed\":%zu}", vol.explored_fraction(), vol.observed_cells());
        frames_js += fb;
    }
    frames_js += "]";   // events_js 는 AR/구조 비트까지 추가 후 아래에서 닫는다

    // ── 명령/구조 비트(탐지 이후 콕핏→고글 AR) ──
    // 가장 먼저 탐지된 생존자에 대해: 탐지+6f AR 경로지시, +스토리 비트.
    int firstdet=NF; int firstv=-1;
    for (size_t v=0;v<detected.size();++v) if(detected[v]>=0 && detected[v]<firstdet){firstdet=detected[v];firstv=(int)v;}
    if (firstv>=0){
        int fA=std::min(NF-1,firstdet+6), fB=std::min(NF-1,firstdet+18), fC=std::min(NF-1,firstdet+30);
        const Box& B=bld.boxes()[person_idx[firstv]];
        float vx=(B.lo.x+B.hi.x)/2, vy=(B.lo.y+B.hi.y)/2, vz=(B.lo.z+B.hi.z)/2;
        char buf[420];
        std::snprintf(buf,sizeof(buf),"{\"f\":%d,\"kind\":\"ar_cmd\",\"to\":\"VISOR-1\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"text\":\"AR 경로 지시 → 요구조자 위치\"}",fA,vx,vy,vz); add_ev(buf);
        std::snprintf(buf,sizeof(buf),"{\"f\":%d,\"kind\":\"ar_cue\",\"to\":\"VISOR-1\",\"text\":\"전방 우측 코너방 · 비반응 인원\"}",fB); add_ev(buf);
        std::snprintf(buf,sizeof(buf),"{\"f\":%d,\"kind\":\"rescue\",\"to\":\"VISOR-1\",\"text\":\"요구조자 확보 · 대피 경로 확보\"}",fC); add_ev(buf);
    }
    events_js += "]";

    // points.bin ― 18바이트 패킹(struct 패딩 회피: xyz12 + rgba4 + reveal2)
    std::ofstream pb(out+"/points.bin", std::ios::binary);
    for (auto& r : recs){ pb.write((char*)&r.x,12); pb.write((char*)&r.r,4); pb.write((char*)&r.reveal,2); }
    pb.close();

    // mission.json
    std::ofstream j(out+"/mission.json");
    j<<"{\"nframes\":"<<NF<<",\"fps\":"<<fps<<",\"voxel\":"<<voxel
     <<",\"npoints\":"<<recs.size()
     <<",\"building\":{\"W\":"<<bld.W<<",\"D\":"<<bld.D<<",\"H\":"<<bld.H<<",\"floor_z\":[0,3.2,6.4]}";
    // 시맨틱(가스레인지/화점/생존자/가구 등)
    j<<",\"semantics\":[";
    bool sf=true;
    for (auto& B:bld.boxes()){
        const char* k=B.kind;
        if (std::strcmp(k,"gas_range")&&std::strcmp(k,"fire")&&std::strcmp(k,"person")&&std::strcmp(k,"furniture")&&std::strcmp(k,"range_hood")) continue;
        if(!sf) j<<","; sf=false;
        j<<"{\"kind\":\""<<k<<"\",\"x\":"<<(B.lo.x+B.hi.x)/2<<",\"y\":"<<(B.lo.y+B.hi.y)/2<<",\"z\":"<<(B.lo.z+B.hi.z)/2
         <<",\"temp\":"<<(std::isnan(B.temp_c)?0.f:B.temp_c)<<"}";
    }
    j<<"]";
    j<<",\"agents\":[";
    for (size_t a=0;a<ag.size();++a) j<<(a?",":"")<<"{\"id\":\""<<ag[a].id<<"\",\"type\":\""<<ag[a].type<<"\"}";
    j<<"]";
    j<<",\"frames\":"<<frames_js;
    j<<",\"events\":"<<events_js;
    j<<"}";
    j.close();

    printf("[mission] frames=%d points=%zu explored=%.1f%% events: ", NF, recs.size(), vol.explored_fraction()*100);
    for (size_t v=0;v<detected.size();++v) printf("person%zu@f%d ", v+1, detected[v]);
    printf("\n[mission] → %s/{points.bin,mission.json}\n", out.c_str());
    return 0;
}
