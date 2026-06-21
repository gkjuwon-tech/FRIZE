// frize/apparatus/main.cpp ― FRIZE VENT-1 (IoT 자동 배연/진입 포트) 엣지 SW
//
// 콕핏 OPEN/CLOSE 수신 → 액추에이터 개폐, ApparatusTelemetry(개방도/상태/온도/가스) 송출.
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <thread>
#include <random>
#include <algorithm>

#include "frize/bus.hpp"
#include "frize/protocol.hpp"
#include "frize/log.hpp"
#include "frize/apparatus/actuator.hpp"

using namespace frize;
static auto LOG = make_logger("apparatus");
static std::string envs(const char* k,const std::string& d){const char* v=std::getenv(k);return v?v:d;}
static int envi(const char* k,int d){const char* v=std::getenv(k);return v?std::atoi(v):d;}
static double envd(const char* k,double d){const char* v=std::getenv(k);return v?std::atof(v):d;}

int main(){
    const std::string id=envs("FRIZE_DEVICE_ID","vent-01");
    const std::string host=envs("FRIZE_MQTT_HOST","localhost");
    const int port=envi("FRIZE_MQTT_PORT",1883);
    const std::string backend=envs("FRIZE_ACTUATOR_BACKEND","sim");   // sim | serial
    const std::string serial=envs("FRIZE_ACTUATOR_SERIAL","/dev/ttyUSB0");
    Vec3 pos{envd("FRIZE_POS_X",6), envd("FRIZE_POS_Y",4), 0};

    LOG->info("VENT-1 {} 시작 (backend={})", id, backend);
    std::unique_ptr<apparatus::Actuator> act;
    if(backend=="serial") act=apparatus::make_actuator_serial(serial,115200);
    if(!act) act=apparatus::make_actuator_sim();
    act->open();

    MessageBus bus(id,host,port); bus.set_device_will(id, DeviceType::Apparatus);
    std::mutex q; std::deque<Command> cmds;
    bus.subscribe(Topic::command(id), [&](const std::string&, const Envelope& e){
        std::lock_guard<std::mutex> lk(q); cmds.push_back(e.as<Command>()); });
    // 콕핏 페어링: 배연 IoT 장비 능력 광고
    bus.enable_pairing(id, DeviceType::Apparatus, "0.1.0",
                       {"vent_actuator", "fan_speed", "louver"});
    bus.start();
    auto ack=[&](const Command& c, CommandStatus s, const std::string& d){
        CommandAck a; a.cmd_id=c.cmd_id; a.device_id=id; a.status=s; a.detail=d;
        bus.publish(Topic::command_ack(id), Envelope::wrap(MessageType::CommandAck,id,a),1); };

    std::mt19937 rng(7); std::normal_distribution<double> nd(0,1);
    double co=80,temp=40;
    using clk=std::chrono::steady_clock; auto t0=clk::now(),tt=t0,th=t0;
    while(true){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now=clk::now(); double dt=std::chrono::duration<double>(now-t0).count(); t0=now;
        for(;;){ Command c; {std::lock_guard<std::mutex> lk(q); if(cmds.empty())break; c=cmds.front(); cmds.pop_front();}
            ack(c,CommandStatus::Acked,"수신");
            if(c.action==CommandAction::Open){ act->command_open(); ack(c,CommandStatus::Executing,"개방"); }
            else if(c.action==CommandAction::Close){ act->command_close(); ack(c,CommandStatus::Executing,"폐쇄"); }
            else ack(c,CommandStatus::Failed,"장비 미지원 명령");
        }
        auto st=act->poll(dt);
        co=std::clamp(co+nd(rng)*20,0.0,2000.0); temp=std::clamp(temp+nd(rng)*1.5,20.0,300.0);
        if(std::chrono::duration<double>(now-tt).count()>=0.2){ tt=now;
            ApparatusTelemetry t; t.device_id=id; t.pose.position=pos;
            t.mech_state=st.state; t.position_pct=st.position_pct; t.ambient_temp_c=temp;
            t.gas.co_ppm=co; t.state=st.fault?DeviceState::Critical:DeviceState::Online;
            bus.publish(Topic::telemetry(id), Envelope::wrap(MessageType::Telemetry,id,t),0);
        }
        if(std::chrono::duration<double>(now-th).count()>=1.0){ th=now;
            Heartbeat hb; hb.device_id=id; hb.device_type=DeviceType::Apparatus;
            bus.publish(Topic::heartbeat(id), Envelope::wrap(MessageType::Heartbeat,id,hb),1,true);
        }
    }
}
