// frize/recon/fire.hpp ― 화재 확산 분석 엔진 (의존성 0, 순수 C++17)
//
// 열화상은 지금까지 '화점이 있다/없다'만 봤다(정적 핫스팟). 이 모듈은 거기서
// 한 발 더 나간다: 같은 셀의 온도를 시간축으로 추적해 dT/dt(가열률)를 뽑고,
//   • 확산 전선(front)      : 지금 막 달궈지는 셀들 = 불이 '번지고 있는' 가장자리
//   • 확산 벡터(spread)     : 화점 클러스터별 진행 방향 + 속도(어디로 가는가)
//   • 재정찰 목표(revisit)  : 오래 안 본 + 열 관련 셀 = 드론이 '다시 가서' 확인할 곳
// 를 산출한다. 입력은 (x,y,z,temp_c,t) 관측 스트림뿐 → 시뮬/실기 공용.
//
// 좌표/시간 단위: 월드 m, 시간 s(시뮬은 합성 시각, 실기는 wall clock). 의존성 0.
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <queue>

namespace frize::recon {

class FireAnalyzer {
public:
    // 한 셀의 시계열 열 상태. EMA로 노이즈를 누르고 dT/dt를 안정적으로 추정.
    struct Cell {
        float temp{NaN()};       // 평활 온도(°C)
        float rate{0.f};         // dT/dt 가열률(°C/s, EMA). +면 달궈지는 중
        float peak{NaN()};       // 관측 최고온(번 적 있는지 판단)
        double last_t{-1.0};     // 마지막 관측 시각(재정찰 staleness용)
        double first_t{-1.0};    // 최초 관측 시각
        float rate_ref_temp{NaN()};  // dT/dt 기준 온도(직전 '유의미' 관측)
        double rate_ref_t{-1.0};     // dT/dt 기준 시각
        int obs{0};
    };
    // 확산 전선 셀: 지금 달궈지고 있는 표면(불이 번지는 가장자리).
    struct FrontCell { double x,y,z; float temp; float rate; };
    // 화점 클러스터의 확산 상태: 어디서(centroid) 어디로(dir) 얼마나 빨리(rate).
    struct SpreadVec {
        double cx,cy,cz;         // 화점 중심(월드 m)
        double dx,dy,dz;         // 확산 단위방향(번져나가는 쪽). 0이면 정체.
        float rate;              // 전선 평균 가열률(°C/s) ≈ 확산 강도
        float peak;              // 클러스터 최고온(°C)
        int cells;               // 화점 핵 셀 수(≈ 규모)
    };
    // 재정찰 목표: 드론이 '다시 가야' 할 곳(오래됐고 + 열과 관련).
    struct Revisit { double x,y,z; float priority; float temp; double age_s; };

    explicit FireAnalyzer(double voxel_m = 0.30) : vs_(voxel_m) {}

    // ── 튜닝 임계값(기본값은 시뮬/실기 공통으로 합리적; env/세터로 조정) ──
    void set_thresholds(float warm_c, float fire_c, float rate_min_cps) {
        warm_c_ = warm_c; fire_c_ = fire_c; rate_min_ = rate_min_cps;
    }

    // 열화상 관측 1점 융합. temp가 NaN이면(열 미측정) 무시.
    //  dT/dt는 '유의미한 시간 간격(rate_dt_ 이상)'에서만 계산한다 → 한 번 지나갈 때
    //  같은 셀을 0.0x초 간격으로 여러 번 보는 잡음이 가열률을 폭주시키지 않는다.
    //  즉 가열률 = '드론이 이 셀을 다시 보러 온 사이' 온도가 얼마나 올랐나 = 확산속도.
    void observe(double x, double y, double z, float temp, double t) {
        if (std::isnan(temp)) return;
        Cell& c = cells_[key(x,y,z)];
        if (c.obs == 0) {
            c.temp = temp; c.peak = temp; c.first_t = t; c.last_t = t; c.rate = 0.f;
            c.rate_ref_temp = temp; c.rate_ref_t = t; c.obs = 1;
            return;
        }
        float ta = 0.5f;                                       // 온도 평활(항상)
        c.temp = (1.f - ta) * c.temp + ta * temp;
        if (std::isnan(c.peak) || c.temp > c.peak) c.peak = c.temp;
        double dtr = t - c.rate_ref_t;
        if (dtr >= rate_dt_) {                                  // 유의미 간격에서만 dT/dt
            float inst = (c.temp - c.rate_ref_temp) / (float)dtr;
            inst = std::max(-50.f, std::min(80.f, inst));      // 물리적 범위로 클램프
            float a = (inst >= 0.f) ? 0.6f : 0.35f;            // 가열 빠르게, 냉각 느리게
            c.rate = (1.f - a) * c.rate + a * inst;
            c.rate_ref_temp = c.temp; c.rate_ref_t = t;
        }
        c.last_t = t;
        ++c.obs;
    }

    // 시간 경과에 따른 냉각 가정(관측 안 된 셀의 rate는 0으로 수렴). 선택적.
    // 호출 안 해도 동작하지만, 길게 안 본 전선을 '식은 걸로' 두고 싶을 때 사용.
    void decay_unobserved(double now, double half_life_s = 12.0) {
        const float k = std::exp(-std::log(2.0) / half_life_s);  // 한 틱 가정 단순화
        for (auto& [kk, c] : cells_) {
            if (now - c.last_t > half_life_s) c.rate *= k;
        }
    }

    // ── 확산 전선: 달궈지고 있고(또는 이미 뜨겁고 막 올라온) 셀들 ──
    std::vector<FrontCell> front(size_t maxn = 4000) const {
        std::vector<FrontCell> out;
        for (auto& [kk, c] : cells_) {
            if (c.obs < 2) continue;
            bool rising = c.rate > rate_min_ && c.temp > warm_c_;
            if (!rising) continue;
            auto w = world(kk);
            out.push_back({w[0], w[1], w[2], c.temp, c.rate});
            if (out.size() >= maxn) break;
        }
        // 가열률 높은 순(가장 활발히 번지는 전선 먼저)
        std::sort(out.begin(), out.end(),
                  [](const FrontCell& a, const FrontCell& b){ return a.rate > b.rate; });
        return out;
    }

    // ── 확산 벡터: 화점 핵을 클러스터링하고, 각 클러스터가 어느 방향으로
    //    번지는지(주변 전선 가열률 가중합)와 강도를 낸다. ──
    std::vector<SpreadVec> spread(size_t max_clusters = 24) const {
        // 1) 화점 핵 셀(이미 뜨겁게 타는 곳) 수집
        std::vector<int64_t> core;
        for (auto& [kk, c] : cells_)
            if (!std::isnan(c.temp) && c.temp >= fire_c_) core.push_back(kk);
        if (core.empty()) return {};

        // 2) 26-이웃 BFS로 연결요소 클러스터링(빈틈 1셀 허용 X — 핵은 촘촘)
        std::unordered_map<int64_t,int> comp;  comp.reserve(core.size()*2);
        for (auto k : core) comp[k] = -1;
        std::vector<std::vector<int64_t>> clusters;
        for (auto seed : core) {
            if (comp[seed] != -1) continue;
            int id = (int)clusters.size();
            clusters.push_back({});
            std::queue<int64_t> q; q.push(seed); comp[seed] = id;
            while (!q.empty()) {
                int64_t cur = q.front(); q.pop();
                clusters[id].push_back(cur);
                int ci,cj,ck; unkey(cur,ci,cj,ck);
                for (int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
                    if(!dx&&!dy&&!dz) continue;
                    int64_t nk = ikey(ci+dx,cj+dy,ck+dz);
                    auto it = comp.find(nk);
                    if (it != comp.end() && it->second == -1) { it->second = id; q.push(nk); }
                }
            }
        }

        // 3) 클러스터별 centroid/peak + 주변 전선으로 확산방향·강도 산출
        std::vector<SpreadVec> out;
        const double R = vs_ * 6.0;            // 전선 탐색 반경
        for (auto& cl : clusters) {
            double cx=0,cy=0,cz=0,wsum=0; float peak=NaN();
            for (auto k : cl) {
                const Cell& c = cells_.at(k);
                auto w = world(k);
                float ww = std::max(1.f, c.temp);
                cx+=w[0]*ww; cy+=w[1]*ww; cz+=w[2]*ww; wsum+=ww;
                if (std::isnan(peak)||c.temp>peak) peak=c.temp;
            }
            cx/=wsum; cy/=wsum; cz/=wsum;
            // 중심 주변에서 '달궈지는' 셀들 → 방향 = Σ unit(p-centroid)*rate
            double vx=0,vy=0,vz=0; float rsum=0; int rn=0;
            for (auto& [kk,c] : cells_) {
                if (c.rate <= rate_min_ || c.temp <= warm_c_) continue;
                auto w = world(kk);
                double ex=w[0]-cx, ey=w[1]-cy, ez=w[2]-cz;
                double d = std::sqrt(ex*ex+ey*ey+ez*ez);
                if (d < 1e-3 || d > R) continue;
                double inv = c.rate / d;
                vx+=ex*inv; vy+=ey*inv; vz+=ez*inv; rsum+=c.rate; ++rn;
            }
            double vL = std::sqrt(vx*vx+vy*vy+vz*vz);
            SpreadVec s;
            s.cx=cx; s.cy=cy; s.cz=cz; s.peak=peak; s.cells=(int)cl.size();
            if (vL > 1e-6 && rn > 0) {
                s.dx=vx/vL; s.dy=vy/vL; s.dz=vz/vL; s.rate=rsum/rn;
            } else { s.dx=s.dy=s.dz=0; s.rate=0; }
            out.push_back(s);
        }
        // 규모(셀 수) 큰 순으로 잘라서 반환
        std::sort(out.begin(), out.end(),
                  [](const SpreadVec& a, const SpreadVec& b){ return a.cells > b.cells; });
        if (out.size() > max_clusters) out.resize(max_clusters);
        return out;
    }

    // ── 재정찰 목표: '오래 안 본' + '열과 관련(뜨겁거나 번지는 중)' 셀.
    //    드론이 미탐사만 좇지 말고 여기로 '되돌아가' 불 진행을 재확인해야 한다. ──
    std::vector<Revisit> revisit_targets(double now, double stale_s = 6.0,
                                         size_t maxn = 64) const {
        std::vector<Revisit> out;
        for (auto& [kk, c] : cells_) {
            if (c.obs < 1) continue;
            double age = now - c.last_t;
            if (age < stale_s) continue;                 // 최근에 봤으면 패스
            // 열 관련성: 뜨거웠던 적 있거나(peak) 마지막에 달궈지는 중이었던 셀
            float heat = 0.f;
            if (!std::isnan(c.peak) && c.peak > warm_c_) heat += (c.peak - warm_c_);
            if (c.rate > 0.f) heat += c.rate * 8.f;       // 번지는 중이었으면 가중
            if (heat <= 1.f) continue;                    // 열과 무관한 데는 재정찰 불필요
            float prio = (float)std::min(age / stale_s, 4.0) * heat;  // 오래될수록 ↑
            auto w = world(kk);
            out.push_back({w[0], w[1], w[2], prio, c.temp, age});
        }
        std::sort(out.begin(), out.end(),
                  [](const Revisit& a, const Revisit& b){ return a.priority > b.priority; });
        if (out.size() > maxn) out.resize(maxn);
        return out;
    }

    // ── 요약 통계(리포트/HUD) ──
    struct Summary {
        int front_cells{0};        // 확산 전선 셀 수
        int burning_cells{0};      // 화점 핵 셀 수
        float max_rate{0.f};       // 최대 가열률(°C/s)
        float mean_front_rate{0.f};// 전선 평균 가열률
        float max_temp{0.f};       // 최고 온도
        double burning_area_m2{0}; // 화점 표면 근사 면적
    };
    Summary summary() const {
        Summary s; double rsum=0;
        for (auto& [kk,c] : cells_) {
            if (std::isnan(c.temp)) continue;
            if (c.temp > s.max_temp) s.max_temp = c.temp;
            if (c.rate > s.max_rate) s.max_rate = c.rate;
            if (c.temp >= fire_c_) ++s.burning_cells;
            if (c.obs >= 2 && c.rate > rate_min_ && c.temp > warm_c_) {
                ++s.front_cells; rsum += c.rate;
            }
        }
        s.mean_front_rate = s.front_cells ? (float)(rsum / s.front_cells) : 0.f;
        s.burning_area_m2 = s.burning_cells * vs_ * vs_;
        return s;
    }

    size_t tracked_cells() const { return cells_.size(); }
    double voxel() const { return vs_; }

private:
    static constexpr float NaN() { return std::numeric_limits<float>::quiet_NaN(); }
    double vs_;
    float warm_c_  = 55.f;     // '따뜻함' 기준(이 위만 전선 후보)
    float fire_c_  = 180.f;    // '타는 중' 기준(화점 핵)
    float rate_min_ = 1.5f;    // 확산으로 칠 최소 가열률(°C/s)
    double rate_dt_ = 1.0;     // dT/dt를 계산할 최소 시간 간격(초) ― 잡음 억제
    std::unordered_map<int64_t, Cell> cells_;
    size_t hot_dirty_{0};

    // 21비트씩 패킹(VoxelMap과 동일 규약) — 인덱스 ±100만 범위
    int64_t ikey(int i,int j,int k) const {
        auto enc = [](int v){ return (int64_t)(v + (1<<20)) & 0x1FFFFF; };
        return (enc(i) << 42) | (enc(j) << 21) | enc(k);
    }
    int64_t key(double x,double y,double z) const {
        return ikey((int)std::floor(x/vs_), (int)std::floor(y/vs_), (int)std::floor(z/vs_));
    }
    void unkey(int64_t k, int& i, int& j, int& kk) const {
        i = (int)((k >> 42) & 0x1FFFFF) - (1<<20);
        j = (int)((k >> 21) & 0x1FFFFF) - (1<<20);
        kk= (int)( k        & 0x1FFFFF) - (1<<20);
    }
    std::array<double,3> world(int64_t k) const {
        int i,j,kk; unkey(k,i,j,kk);
        return { (i+0.5)*vs_, (j+0.5)*vs_, (kk+0.5)*vs_ };
    }
};

}  // namespace frize::recon
