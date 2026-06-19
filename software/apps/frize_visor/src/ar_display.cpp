// AR 디스플레이 ― 시각 명령 출력('출력장치 = 사람 눈앞')
//
// headless/sim: 현재 표시 상태를 로그로 출력(개발/통합).
// 실제 Jetson: 도파관에 EGL/OpenGL로 화살표·하이라이트·탈출 내비를 렌더한다
//   (렌더 백엔드는 플랫폼 종속 ― 본 파일의 상태머신을 그대로 그리면 된다).
//
// 핵심: 한 번에 '지금 필요한 한 가지'만 띄운다(연기 속 정보 과부하 금지).
#include "frize/visor/hal.hpp"
#include "frize/log.hpp"
#include <algorithm>

namespace frize::visor {

static auto LOG = frize::make_logger("ar");

static const char* arrow_for(double bearing_deg) {
    if (bearing_deg < 0) return "•";
    int s = (int)((bearing_deg + 22.5) / 45.0) % 8;
    static const char* A[8] = {"↑","↗","→","↘","↓","↙","←","↖"};
    return A[s];
}

class ArDisplayImpl : public ArDisplay {
public:
    explicit ArDisplayImpl(bool headless) : headless_(headless) {}
    bool open() override { LOG->info("AR 디스플레이 {} 모드", headless_ ? "headless" : "device"); return true; }

    void show(const ArCommand& c) override {
        current_ = c; ttl_ = 8.0; dirty_ = true;
        // 화면에 실제로 그릴 내용을 명확히(상태머신). 여기선 로그로 가시화.
        LOG->info("[AR] {} {}  '{}'  ({})",
                  arrow_for(c.bearing_deg), upper(c.kind), c.text, c.severity);
    }
    void clear() override { current_.reset(); LOG->info("[AR] clear"); }

    void tick(double dt) override {
        if (!current_) return;
        ttl_ -= dt;
        if (ttl_ <= 0) { LOG->info("[AR] (만료) {}", current_->kind); current_.reset(); }
    }

private:
    static std::string upper(std::string s){ std::transform(s.begin(),s.end(),s.begin(),::toupper); return s; }
    bool headless_; std::optional<ArCommand> current_{}; double ttl_=0; bool dirty_=false;
};

std::unique_ptr<ArDisplay> make_ar_display(bool headless) {
    return std::make_unique<ArDisplayImpl>(headless);
}

}  // namespace frize::visor
