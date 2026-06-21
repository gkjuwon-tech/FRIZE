// frize_nav ― AR 내비게이션 서비스 (프로덕션)
//
// 콕핏이 트윈에서 목표점+대원을 찍으면(GuideRequest), 이 서비스가:
//   - 디지털 트윈(MapPatch) 또는 정적 장애물에서 점유격자를 만들고
//   - 대원 실시간 위치(telemetry)마다 A* 경로를 재계산해
//   - 해당 고글에 ArCue(Route)를 계속 스트리밍한다(움직이면 화살표가 바뀜)
//   - 도착하면 유도를 종료한다.
//
// 토픽: 구독 frize/guide/+ (유도요청), frize/device/+/telemetry (대원 위치),
//       frize/map/patch (라이브 트윈). 발행 frize/command/<id>/ar (ArCue).
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/schemas.hpp"
#include "frize/log.hpp"
#include "frize/nav/grid.hpp"
#include "frize/nav/astar.hpp"
#include "frize/nav/guide.hpp"

using namespace frize;
using namespace frize::nav;
static auto LOG = make_logger("nav");
static std::atomic<bool> g_run{true};
static void on_sig(int){ g_run=false; }

static std::string envs(const char* k, const std::string& d){ const char* v=std::getenv(k); return v?v:d; }
static double envd(const char* k, double d){ const char* v=std::getenv(k); return v?std::atof(v):d; }

int main(int argc, char** argv){
    std::signal(SIGINT,on_sig); std::signal(SIGTERM,on_sig);
    const std::string id   = envs("FRIZE_NAV_ID","nav-1");
    const std::string host = envs("FRIZE_MQTT_HOST","localhost");
    const int         port = (int)envd("FRIZE_MQTT_PORT",1883);
    const double      res  = envd("FRIZE_NAV_RES",0.12);
    const int         infl = (int)envd("FRIZE_NAV_INFLATE",2);
    const double      zlo  = envd("FRIZE_NAV_ZLO",0.3), zhi=envd("FRIZE_NAV_ZHI",1.9);
    const double      hz   = envd("FRIZE_NAV_HZ",3.0);

    Guidance guide;
    std::mutex mtx;
    std::map<std::string,Vec3>          poses;     // 대원 → 위치
    std::map<std::string,GuideRequest>  targets;   // 대원 → 목표

    // 정적 장애물 시드(선택): JSON {"bounds":[x0,y0,w,d],"rects":[[xmin,ymin,xmax,ymax],...]}
    std::string obs = envs("FRIZE_NAV_OBSTACLES","");
    if (!obs.empty()){
        std::ifstream f(obs);
        if (f){ json j; f>>j;
            auto b=j.at("bounds"); std::vector<std::array<double,4>> rects;
            for (auto& r:j.at("rects")) rects.push_back({r[0],r[1],r[2],r[3]});
            NavGrid g = NavGrid::from_rects(b[0],b[1],b[2],b[3],res,rects); g.inflate(infl);
            guide.set_grid(std::move(g));
            LOG->info("정적 장애물 격자 로드: {} ({} rects)", obs, rects.size());
        } else LOG->warn("장애물 파일 없음: {}", obs);
    }

    MessageBus bus(id, host, port);
    bus.set_device_will(id, DeviceType::Core);

    // 라이브 트윈 → 격자 갱신
    bus.subscribe(Topic::map(), [&](const std::string&, const Envelope& e){
        try { auto mp=e.as<MapPatch>(); if(mp.occupied.empty()) return;
            NavGrid g=NavGrid::from_mappatch(mp,zlo,zhi,res); g.inflate(infl);
            guide.set_grid(std::move(g));
            LOG->info("트윈 격자 갱신: {}x{} (occ {})", guide.grid().nx, guide.grid().ny, mp.occupied.size());
        } catch(...){}
    });
    // 대원 위치
    bus.subscribe(Topic::all_telemetry(), [&](const std::string&, const Envelope& e){
        const json& p=e.payload; if(!p.contains("pose")||!p["pose"].contains("position")) return;
        try { Vec3 pos=p["pose"]["position"].get<Vec3>(); std::lock_guard<std::mutex> lk(mtx); poses[e.source]=pos; } catch(...){}
    });
    // 유도 요청(설정/해제)
    bus.subscribe(Topic::all_guides(), [&](const std::string&, const Envelope& e){
        if (e.type!=MessageType::GuideRequest) return;
        GuideRequest gr; try{ gr=e.as<GuideRequest>(); }catch(...){ return; }
        std::lock_guard<std::mutex> lk(mtx);
        if (gr.active && gr.target){ targets[gr.wearer_id]=gr;
            LOG->info("유도 시작 {} → ({:.1f},{:.1f}) [{}]", gr.wearer_id, gr.target->x, gr.target->y, gr.label);
        } else { targets.erase(gr.wearer_id); LOG->info("유도 종료 {}", gr.wearer_id); }
    });
    bus.start();
    LOG->info("nav 서비스 시작 (broker {}:{}, res {:.2f}m, inflate {})", host, port, res, infl);

    // 재유도 루프: 대원 위치마다 경로 재계산 → ArCue(Route) 스트리밍
    auto period = std::chrono::milliseconds((int)(1000.0/std::max(0.5,hz)));
    while (g_run){
        std::this_thread::sleep_for(period);
        if (!guide.have_grid()) continue;
        std::vector<std::pair<std::string,GuideRequest>> active;
        std::map<std::string,Vec3> psnap;
        { std::lock_guard<std::mutex> lk(mtx); for(auto&kv:targets) active.push_back(kv); psnap=poses; }
        for (auto& [wid, gr] : active){
            auto it=psnap.find(wid); if(it==psnap.end()) continue;   // 위치 미상
            Vec3 w=it->second, t=*gr.target;
            if (guide.arrived(w,t)){
                ArCue done; done.target_device=wid; done.kind=ArCueKind::Text;
                done.text="목표 도착 · 유도 종료"; done.color="#6cc080"; done.ttl_s=4.0; done.issued_by=id;
                bus.publish(Topic::ar_cue(wid), Envelope::wrap(MessageType::ArCue,id,done),1);
                { std::lock_guard<std::mutex> lk(mtx); targets.erase(wid); }
                LOG->info("{} 목표 도착 → 유도 종료", wid);
                continue;
            }
            auto route = guide.plan_route(w,t);
            if (route.size()<2){
                ArCue nopath; nopath.target_device=wid; nopath.kind=ArCueKind::Warning;
                nopath.text="경로 없음 · 재배치 필요"; nopath.color="#cea44a"; nopath.ttl_s=2.0; nopath.issued_by=id; nopath.severity=Severity::High;
                bus.publish(Topic::ar_cue(wid), Envelope::wrap(MessageType::ArCue,id,nopath),1);
                continue;
            }
            ArCue cue = guide.route_cue(wid, route, gr.color, id);
            cue.text = gr.label;
            bus.publish(Topic::ar_cue(wid), Envelope::wrap(MessageType::ArCue,id,cue),1);
        }
    }
    LOG->info("nav 종료");
    return 0;
}
