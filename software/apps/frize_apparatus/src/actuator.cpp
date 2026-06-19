#include "frize/apparatus/actuator.hpp"
#include "frize/log.hpp"
#include <algorithm>

namespace frize::apparatus {
static auto LOG = frize::make_logger("actuator");

// ===================== 시뮬레이터 =====================
class ActuatorSim : public Actuator {
public:
    bool open() override { LOG->info("[sim] 액추에이터"); return true; }
    void command_open() override  { target_=100.0; }
    void command_close() override { target_=0.0; }
    MechStatus poll(double dt) override {
        const double rate=28.0;  // %/s
        if (pos_ < target_) pos_=std::min(target_, pos_+rate*dt);
        else if (pos_ > target_) pos_=std::max(target_, pos_-rate*dt);
        MechStatus s; s.position_pct=pos_;
        if (pos_>=99.5) s.state="open"; else if (pos_<=0.5) s.state="closed";
        else s.state=(target_>pos_)?"opening":"closing";
        return s;
    }
private:
    double pos_=0.0, target_=0.0;
};
std::unique_ptr<Actuator> make_actuator_sim(){ return std::make_unique<ActuatorSim>(); }

// ===================== 실제(MCU 시리얼) =====================
#ifdef FRIZE_HAVE_SERIAL
} // ns
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
namespace frize::apparatus {
class ActuatorSerial : public Actuator {
public:
    ActuatorSerial(std::string port,int baud):port_(std::move(port)),baud_(baud){}
    bool open() override {
        fd_=::open(port_.c_str(), O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(fd_<0){ LOG->error("시리얼 실패: {}",port_); return false; }
        termios tio{}; tcgetattr(fd_,&tio); cfsetospeed(&tio,B115200); cfsetispeed(&tio,B115200);
        tio.c_cflag|=(CLOCAL|CREAD); tio.c_lflag=0; tcsetattr(fd_,TCSANOW,&tio);
        LOG->info("MCU 시리얼: {}",port_); return true;
    }
    void command_open() override  { send("OPEN\n");  target_=100; }
    void command_close() override { send("CLOSE\n"); target_=0; }
    MechStatus poll(double) override {
        // MCU가 "POS,nn,STATE\n" 회신 → 파싱
        char buf[128]; int n=::read(fd_,buf,sizeof(buf)-1);
        if(n>0){ buf[n]=0; line_+=buf; auto p=line_.find('\n');
            if(p!=std::string::npos){ parse(line_.substr(0,p)); line_.erase(0,p+1);} }
        return last_;
    }
private:
    void send(const char* s){ if(fd_>=0) (void)!::write(fd_,s,std::strlen(s)); }
    void parse(const std::string& l){ int pos=0; char st[16]={0};
        if(std::sscanf(l.c_str(),"POS,%d,%15s",&pos,st)>=1){ last_.position_pct=pos; last_.state=st; } }
    std::string port_,line_; int baud_,fd_=-1; double target_=0; MechStatus last_{};
};
std::unique_ptr<Actuator> make_actuator_serial(const std::string& port,int baud){ return std::make_unique<ActuatorSerial>(port,baud); }
#else
std::unique_ptr<Actuator> make_actuator_serial(const std::string&,int){ LOG->warn("시리얼 미빌드 ― 시뮬 폴백"); return nullptr; }
#endif

}  // namespace frize::apparatus
