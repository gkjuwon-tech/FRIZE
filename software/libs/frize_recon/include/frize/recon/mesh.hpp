// frize/recon/mesh.hpp ― 재구성 메쉬 + 상용 교환포맷 export (OBJ / PLY / glTF 2.0)
//
// 마칭큐브가 뽑은 표면 메쉬를 담고, 열화상 기반 정점 컬러를 입혀
// glTF 2.0(.gltf+.bin)으로 내보낸다. glTF는 Qt Quick 3D·Blender·three.js·
// Unreal이 바로 먹는 사실상 표준 "3D 조합" 포맷 → 콕핏 트윈이 즉시 로드 가능.
#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <string>
#include <fstream>
#include <sstream>
#include <limits>
#include "frize/recon/tsdf.hpp"

namespace frize::recon {

struct Mesh {
    std::vector<Vec3f> v;        // 정점 위치(월드 m)
    std::vector<Vec3f> n;        // 정점 법선
    std::vector<Vec3f> c;        // 정점 컬러(0..1 RGB)
    std::vector<float> temp;     // 정점 온도(°C, NaN=미측정)
    std::vector<uint32_t> idx;   // 삼각형 인덱스(3개 단위)

    size_t tris() const { return idx.size()/3; }

    // 표면적(품질 지표)
    double surface_area() const {
        double a=0;
        for (size_t t=0;t<idx.size();t+=3){
            Vec3f e1=v[idx[t+1]]-v[idx[t]], e2=v[idx[t+2]]-v[idx[t]];
            Vec3f cr{e1.y*e2.z-e1.z*e2.y, e1.z*e2.x-e1.x*e2.z, e1.x*e2.y-e1.y*e2.x};
            a += 0.5*norm(cr);
        }
        return a;
    }

    // 온도 → 컬러 램프(차가운 청회 → 주황 → 황백). 청색은 고온에서 낮게 유지해야
    // 불이 보라색이 아니라 '불색'으로 보이고, R-B(열) 기반 발광도 강하게 걸린다.
    // NaN=중립 회색.
    static Vec3f thermal_color(float t){
        if (std::isnan(t)) return {0.60f,0.64f,0.69f};
        float x = std::max(0.f, std::min(1.f, (t-20.f)/600.f));
        if (x < 0.5f){ float u=x/0.5f;                 // 청회 → 주황
            return {0.26f+0.66f*u, 0.33f+0.24f*u, 0.44f-0.34f*u}; }   // →(0.92,0.57,0.10)
        float u=(x-0.5f)/0.5f;                          // 주황 → 황백(청색은 낮게)
        return {0.92f+0.08f*u, 0.57f+0.34f*u, 0.10f+0.26f*u};         // →(1.00,0.91,0.36)
    }
    void colorize(){
        c.resize(v.size());
        for (size_t i=0;i<v.size();++i) c[i]=thermal_color(i<temp.size()?temp[i]:std::numeric_limits<float>::quiet_NaN());
    }

    // Taubin λ|μ 스무딩 ― 스캔 노이즈('찰흙' 블록감)를 매끈한 표면으로.
    // 양(λ)·음(μ) 라플라시안을 번갈아 적용해 부피 수축 없이 평활화.
    // 온도도 같이 평활해 열화상 얼룩을 줄인다. 마지막에 법선 재계산.
    void smooth_taubin(int iters=3, float lambda=0.34f, float mu=-0.35f){
        if (v.empty()||idx.empty()) return;
        const size_t N=v.size();
        std::vector<Vec3f> acc(N); std::vector<int> cnt(N);
        auto step=[&](float f){
            std::fill(acc.begin(),acc.end(),Vec3f{0,0,0});
            std::fill(cnt.begin(),cnt.end(),0);
            for (size_t t=0;t<idx.size();t+=3){
                uint32_t a=idx[t],b=idx[t+1],c2=idx[t+2];
                acc[a]=acc[a]+v[b]+v[c2]; cnt[a]+=2;
                acc[b]=acc[b]+v[a]+v[c2]; cnt[b]+=2;
                acc[c2]=acc[c2]+v[a]+v[b]; cnt[c2]+=2;
            }
            for (size_t i=0;i<N;++i) if(cnt[i]>0){
                Vec3f centroid=acc[i]*(1.0f/cnt[i]);
                v[i]=v[i]+(centroid-v[i])*f;
            }
        };
        for (int it=0; it<iters; ++it){ step(lambda); step(mu); }  // 위치만 평활(온도 보존)
        recompute_normals();
    }

    void recompute_normals(){
        n.assign(v.size(), Vec3f{0,0,0});
        for (size_t t=0;t<idx.size();t+=3){
            uint32_t a=idx[t],b=idx[t+1],cc=idx[t+2];
            Vec3f e1=v[b]-v[a], e2=v[cc]-v[a];
            Vec3f fn{e1.y*e2.z-e1.z*e2.y, e1.z*e2.x-e1.x*e2.z, e1.x*e2.y-e1.y*e2.x};
            n[a]=n[a]+fn; n[b]=n[b]+fn; n[cc]=n[cc]+fn;
        }
        for (auto& nn:n){ float L=norm(nn); if(L>1e-9f) nn=nn*(1.0f/L); else nn={0,0,1}; }
    }

    // ── OBJ (정점 컬러 확장: v x y z r g b) ───────────────────────────────────
    bool write_obj(const std::string& path) const {
        std::ofstream f(path); if(!f) return false;
        f << "# FRIZE digital-twin reconstruction\n";
        for (size_t i=0;i<v.size();++i){
            Vec3f col=i<c.size()?c[i]:Vec3f{0.7f,0.7f,0.7f};
            f<<"v "<<v[i].x<<" "<<v[i].y<<" "<<v[i].z<<" "<<col.x<<" "<<col.y<<" "<<col.z<<"\n";
        }
        for (auto&nn:n) f<<"vn "<<nn.x<<" "<<nn.y<<" "<<nn.z<<"\n";
        for (size_t t=0;t<idx.size();t+=3)
            f<<"f "<<idx[t]+1<<"//"<<idx[t]+1<<" "<<idx[t+1]+1<<"//"<<idx[t+1]+1
             <<" "<<idx[t+2]+1<<"//"<<idx[t+2]+1<<"\n";
        return true;
    }

    // ── PLY (바이너리 LE: 위치+법선+컬러) ─────────────────────────────────────
    bool write_ply(const std::string& path) const {
        std::ofstream f(path, std::ios::binary); if(!f) return false;
        f<<"ply\nformat binary_little_endian 1.0\n";
        f<<"element vertex "<<v.size()<<"\n";
        f<<"property float x\nproperty float y\nproperty float z\n";
        f<<"property float nx\nproperty float ny\nproperty float nz\n";
        f<<"property uchar red\nproperty uchar green\nproperty uchar blue\n";
        f<<"element face "<<tris()<<"\nproperty list uchar uint vertex_indices\n";
        f<<"end_header\n";
        for (size_t i=0;i<v.size();++i){
            Vec3f nn=i<n.size()?n[i]:Vec3f{0,0,1}, col=i<c.size()?c[i]:Vec3f{0.7f,0.7f,0.7f};
            float buf[6]={v[i].x,v[i].y,v[i].z,nn.x,nn.y,nn.z};
            f.write((char*)buf,sizeof(buf));
            unsigned char rgb[3]={(unsigned char)(col.x*255),(unsigned char)(col.y*255),(unsigned char)(col.z*255)};
            f.write((char*)rgb,3);
        }
        for (size_t t=0;t<idx.size();t+=3){
            unsigned char three=3; f.write((char*)&three,1);
            uint32_t tri[3]={idx[t],idx[t+1],idx[t+2]}; f.write((char*)tri,sizeof(tri));
        }
        return true;
    }

    // ── glTF 2.0 (.gltf JSON + .bin) : 상용 표준, 콕핏/Blender 즉시 로드 ──────
    // POSITION + NORMAL + COLOR_0(vec3 float) + 인덱스. PBR 머티리얼(정점컬러).
    bool write_gltf(const std::string& gltf_path, const std::string& bin_name) const {
        std::string dir = gltf_path.substr(0, gltf_path.find_last_of("/\\")+1);
        std::ofstream bf(dir+bin_name, std::ios::binary); if(!bf) return false;
        auto pad4=[&](std::ostream&os,size_t n){ while(n%4){os.put(0);++n;} };

        // 버퍼 레이아웃: [POSITION][NORMAL][COLOR_0][INDICES]
        Vec3f bmin{1e30f,1e30f,1e30f}, bmax{-1e30f,-1e30f,-1e30f};
        size_t off_pos=0;
        for (auto&p:v){ bf.write((char*)&p,12);
            bmin.x=std::min(bmin.x,p.x);bmin.y=std::min(bmin.y,p.y);bmin.z=std::min(bmin.z,p.z);
            bmax.x=std::max(bmax.x,p.x);bmax.y=std::max(bmax.y,p.y);bmax.z=std::max(bmax.z,p.z);}
        size_t len_pos=(size_t)v.size()*12; pad4(bf,len_pos);
        size_t off_nrm=(len_pos+3)/4*4;
        for (auto&nn:n) bf.write((char*)&nn,12);
        size_t len_nrm=(size_t)n.size()*12; pad4(bf,off_nrm+len_nrm);
        size_t off_col=off_nrm+(len_nrm+3)/4*4;
        for (size_t i=0;i<v.size();++i){ Vec3f col=i<c.size()?c[i]:Vec3f{0.7f,0.7f,0.7f}; bf.write((char*)&col,12);}
        size_t len_col=(size_t)v.size()*12; pad4(bf,off_col+len_col);
        size_t off_idx=off_col+(len_col+3)/4*4;
        for (auto&ix:idx) bf.write((char*)&ix,4);
        size_t len_idx=(size_t)idx.size()*4;
        size_t total=off_idx+len_idx;
        bf.close();

        std::ostringstream j; j.setf(std::ios::fixed); j.precision(6);
        j<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"FRIZE recon\"},";
        j<<"\"scenes\":[{\"nodes\":[0]}],\"scene\":0,\"nodes\":[{\"mesh\":0,\"name\":\"twin\"}],";
        j<<"\"meshes\":[{\"name\":\"twin\",\"primitives\":[{\"attributes\":{"
            "\"POSITION\":0,\"NORMAL\":1,\"COLOR_0\":2},\"indices\":3,\"material\":0,\"mode\":4}]}],";
        j<<"\"materials\":[{\"name\":\"twin\",\"pbrMetallicRoughness\":{"
            "\"metallicFactor\":0.0,\"roughnessFactor\":0.85},\"doubleSided\":true}],";
        j<<"\"buffers\":[{\"uri\":\""<<bin_name<<"\",\"byteLength\":"<<total<<"}],";
        j<<"\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":"<<off_pos<<",\"byteLength\":"<<len_pos<<",\"target\":34962},"
            "{\"buffer\":0,\"byteOffset\":"<<off_nrm<<",\"byteLength\":"<<len_nrm<<",\"target\":34962},"
            "{\"buffer\":0,\"byteOffset\":"<<off_col<<",\"byteLength\":"<<len_col<<",\"target\":34962},"
            "{\"buffer\":0,\"byteOffset\":"<<off_idx<<",\"byteLength\":"<<len_idx<<",\"target\":34963}],";
        j<<"\"accessors\":["
            "{\"bufferView\":0,\"componentType\":5126,\"count\":"<<v.size()<<",\"type\":\"VEC3\","
              "\"min\":["<<bmin.x<<","<<bmin.y<<","<<bmin.z<<"],\"max\":["<<bmax.x<<","<<bmax.y<<","<<bmax.z<<"]},"
            "{\"bufferView\":1,\"componentType\":5126,\"count\":"<<n.size()<<",\"type\":\"VEC3\"},"
            "{\"bufferView\":2,\"componentType\":5126,\"count\":"<<v.size()<<",\"type\":\"VEC3\"},"
            "{\"bufferView\":3,\"componentType\":5125,\"count\":"<<idx.size()<<",\"type\":\"SCALAR\"}]}";
        std::ofstream gf(gltf_path); if(!gf) return false; gf<<j.str();
        return true;
    }
};

}  // namespace frize::recon
