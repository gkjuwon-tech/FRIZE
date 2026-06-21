// frize_twin_render ― 디지털 트윈 실시간 컷어웨이 렌더러 (오프스크린 GL, 초 단위)
//
// 블렌더(수십 분) 대신: 재구성 PLY(위치+법선+열화상 컬러)를 GPU로 올려
// 단면 컷(루프/벽 제거)을 걸어 "내부가 보이는" 돌하우스 단면을 즉시 렌더.
//   - 컷 평면(z>CUT 제거)으로 해당 층 내부 노출 → 방·가구·화점·요구조자 보임
//   - 열화상 컬러 기반 발광(화점이 실제로 빛남) + 디퓨즈/림 라이트
//   - 다크 배경, 1초 내 렌더 → 콕핏 풀블리드 배경으로 사용
//
// 빌드: build_twin.sh (g++ + GLFW + GL, xvfb-run 헤드리스)
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

// ── GL 3.3 함수 포인터 로더(GLEW 없이 glfwGetProcAddress) ──
#define GLF(t,n) static t n=nullptr;
GLF(PFNGLGENFRAMEBUFFERSPROC,glGenFramebuffers) GLF(PFNGLBINDFRAMEBUFFERPROC,glBindFramebuffer)
GLF(PFNGLGENRENDERBUFFERSPROC,glGenRenderbuffers) GLF(PFNGLBINDRENDERBUFFERPROC,glBindRenderbuffer)
GLF(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,glRenderbufferStorageMultisample)
GLF(PFNGLFRAMEBUFFERRENDERBUFFERPROC,glFramebufferRenderbuffer)
GLF(PFNGLCHECKFRAMEBUFFERSTATUSPROC,glCheckFramebufferStatus)
GLF(PFNGLFRAMEBUFFERTEXTURE2DPROC,glFramebufferTexture2D) GLF(PFNGLBLITFRAMEBUFFERPROC,glBlitFramebuffer)
GLF(PFNGLGENVERTEXARRAYSPROC,glGenVertexArrays) GLF(PFNGLBINDVERTEXARRAYPROC,glBindVertexArray)
GLF(PFNGLGENBUFFERSPROC,glGenBuffers) GLF(PFNGLBINDBUFFERPROC,glBindBuffer) GLF(PFNGLBUFFERDATAPROC,glBufferData)
GLF(PFNGLVERTEXATTRIBPOINTERPROC,glVertexAttribPointer) GLF(PFNGLENABLEVERTEXATTRIBARRAYPROC,glEnableVertexAttribArray)
GLF(PFNGLCREATESHADERPROC,glCreateShader) GLF(PFNGLSHADERSOURCEPROC,glShaderSource) GLF(PFNGLCOMPILESHADERPROC,glCompileShader)
GLF(PFNGLGETSHADERIVPROC,glGetShaderiv) GLF(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog)
GLF(PFNGLCREATEPROGRAMPROC,glCreateProgram) GLF(PFNGLATTACHSHADERPROC,glAttachShader)
GLF(PFNGLLINKPROGRAMPROC,glLinkProgram) GLF(PFNGLUSEPROGRAMPROC,glUseProgram)
GLF(PFNGLGETUNIFORMLOCATIONPROC,glGetUniformLocation) GLF(PFNGLUNIFORMMATRIX4FVPROC,glUniformMatrix4fv)
GLF(PFNGLUNIFORM3FPROC,glUniform3f) GLF(PFNGLUNIFORM1FPROC,glUniform1f)
static void load_gl(){
    #define LD(n) n=(decltype(n))glfwGetProcAddress(#n)
    LD(glGenFramebuffers);LD(glBindFramebuffer);LD(glGenRenderbuffers);LD(glBindRenderbuffer);
    LD(glRenderbufferStorageMultisample);LD(glFramebufferRenderbuffer);LD(glCheckFramebufferStatus);
    LD(glFramebufferTexture2D);LD(glBlitFramebuffer);LD(glGenVertexArrays);LD(glBindVertexArray);
    LD(glGenBuffers);LD(glBindBuffer);LD(glBufferData);LD(glVertexAttribPointer);LD(glEnableVertexAttribArray);
    LD(glCreateShader);LD(glShaderSource);LD(glCompileShader);LD(glGetShaderiv);LD(glGetShaderInfoLog);
    LD(glCreateProgram);LD(glAttachShader);LD(glLinkProgram);LD(glUseProgram);
    LD(glGetUniformLocation);LD(glUniformMatrix4fv);LD(glUniform3f);LD(glUniform1f);
    #undef LD
}

struct V3{float x,y,z;};
static V3 sub(V3 a,V3 b){return{a.x-b.x,a.y-b.y,a.z-b.z};}
static V3 cross(V3 a,V3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V3 norml(V3 a){float l=std::sqrt(dot(a,a));return l>1e-9f?V3{a.x/l,a.y/l,a.z/l}:a;}

// 4x4 (열우선) 행렬
struct M4{float m[16];};
static M4 mul(const M4&a,const M4&b){M4 r{}; for(int c=0;c<4;c++)for(int rr=0;rr<4;rr++){float s=0;for(int k=0;k<4;k++)s+=a.m[k*4+rr]*b.m[c*4+k];r.m[c*4+rr]=s;}return r;}
static M4 perspective(float fovy,float asp,float n,float f){float t=1.f/std::tan(fovy*0.5f);M4 r{};r.m[0]=t/asp;r.m[5]=t;r.m[10]=(f+n)/(n-f);r.m[11]=-1;r.m[14]=2*f*n/(n-f);return r;}
static M4 lookat(V3 e,V3 c,V3 up){V3 f=norml(sub(c,e)),s=norml(cross(f,up)),u=cross(s,f);M4 r{};
    r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z; r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z; r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
    r.m[12]=-dot(s,e);r.m[13]=-dot(u,e);r.m[14]=dot(f,e);r.m[15]=1;return r;}

// 바이너리 LE PLY (x y z nx ny nz r g b uchar) + face list uchar uint
struct Mesh{ std::vector<float> verts; /*9 floats: pos3 nrm3 col3*/ std::vector<uint32_t> idx; V3 lo,hi; };
static bool load_ply(const char* path, Mesh& M){
    std::ifstream f(path,std::ios::binary); if(!f){fprintf(stderr,"ply open fail %s\n",path);return false;}
    std::string line; size_t nv=0,nf=0; bool inv=false;
    while(std::getline(f,line)){
        if(line.rfind("element vertex",0)==0) nv=std::stoul(line.substr(15));
        else if(line.rfind("element face",0)==0) nf=std::stoul(line.substr(13));
        else if(line.rfind("end_header",0)==0){inv=true;break;}
    }
    if(!inv){fprintf(stderr,"no header end\n");return false;}
    M.verts.resize(nv*9);
    M.lo={1e30f,1e30f,1e30f}; M.hi={-1e30f,-1e30f,-1e30f};
    for(size_t i=0;i<nv;i++){
        float pn[6]; unsigned char rgb[3];
        f.read((char*)pn,24); f.read((char*)rgb,3);
        float* o=&M.verts[i*9];
        o[0]=pn[0];o[1]=pn[1];o[2]=pn[2]; o[3]=pn[3];o[4]=pn[4];o[5]=pn[5];
        o[6]=rgb[0]/255.f;o[7]=rgb[1]/255.f;o[8]=rgb[2]/255.f;
        M.lo.x=std::min(M.lo.x,pn[0]);M.lo.y=std::min(M.lo.y,pn[1]);M.lo.z=std::min(M.lo.z,pn[2]);
        M.hi.x=std::max(M.hi.x,pn[0]);M.hi.y=std::max(M.hi.y,pn[1]);M.hi.z=std::max(M.hi.z,pn[2]);
    }
    M.idx.resize(nf*3);
    for(size_t i=0;i<nf;i++){ unsigned char c; f.read((char*)&c,1); uint32_t t[3]; f.read((char*)t,12);
        M.idx[i*3]=t[0];M.idx[i*3+1]=t[1];M.idx[i*3+2]=t[2]; }
    fprintf(stderr,"[ply] %zu verts %zu tris bbox(%.1f..%.1f, %.1f..%.1f, %.1f..%.1f)\n",
            nv,nf,M.lo.x,M.hi.x,M.lo.y,M.hi.y,M.lo.z,M.hi.z);
    return true;
}

static GLuint compile(GLenum t,const char* s){ GLuint sh=glCreateShader(t);glShaderSource(sh,1,&s,0);glCompileShader(sh);
    GLint ok;glGetShaderiv(sh,GL_COMPILE_STATUS,&ok); if(!ok){char log[2048];glGetShaderInfoLog(sh,2048,0,log);fprintf(stderr,"shader err: %s\n",log);} return sh;}

static const char* VS=R"(#version 330 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNrm; layout(location=2) in vec3 aCol;
uniform mat4 uMVP;
out vec3 wPos; out vec3 wNrm; out vec3 vCol;
void main(){ wPos=aPos; wNrm=aNrm; vCol=aCol; gl_Position=uMVP*vec4(aPos,1.0); }
)";
static const char* FS=R"(#version 330 core
in vec3 wPos; in vec3 wNrm; in vec3 vCol;
uniform vec3 uCam; uniform float uCutZ; uniform float uCutYlo; uniform float uCutYhi;
uniform vec3 uFloors;  // 슬래브 z (단면 강조용, 미사용시 무시)
out vec4 o;
void main(){
    if(wPos.z > uCutZ) discard;                       // 루프/상층 제거 → 내부 노출
    vec3 N=normalize(wNrm); if(!gl_FrontFacing) N=-N;  // 양면
    vec3 base=vCol;
    // 열: thermal_color 에서 (r-b) 가 클수록 고온. 화점 발광.
    float heat=clamp((base.r-base.b)*1.6-0.1,0.0,1.0);
    // 주광(위/측면) + 보조 + 천장 바운스
    vec3 Lk=normalize(vec3(0.35,-0.25,0.92));
    vec3 Lf=normalize(vec3(-0.5,0.4,0.25));
    float dk=max(dot(N,Lk),0.0), df=max(dot(N,Lf),0.0);
    float amb=0.28+0.12*N.z;                            // 위를 보는 면 약간 밝게
    vec3 lit=base*(amb + 0.85*dk + 0.24*df);
    // 화점 발광(주황~황백)
    vec3 emis=mix(vec3(1.0,0.42,0.10),vec3(1.0,0.85,0.45),heat)*pow(heat,1.5)*2.8;
    // 림(실루엣 분리)
    vec3 Vd=normalize(uCam-wPos);
    float rim=pow(1.0-max(dot(N,Vd),0.0),3.0)*0.35;
    vec3 col=lit+emis+rim*vec3(0.35,0.45,0.65);
    // 깊이 안개(먼 곳 어둡게 → 입체감)
    float d=length(uCam-wPos); float fog=clamp((d-18.0)/46.0,0.0,0.62);
    col=mix(col,vec3(0.02,0.03,0.05),fog);
    // 톤매핑
    col=col/(col+vec3(0.72))*1.62;
    col=pow(col,vec3(0.90));
    o=vec4(col,1.0);
}
)";

int main(int argc,char**argv){
    const char* ply = argc>1?argv[1]:"/tmp/twinout/twin.ply";
    const char* out = argc>2?argv[2]:"/tmp/twin_cockpit.png";
    float cutZ      = argc>3?std::atof(argv[3]):3.0f;   // 층 단면 높이
    int W = argc>4?std::atoi(argv[4]):1920, H=argc>5?std::atoi(argv[5]):1080;

    if(!glfwInit()){fprintf(stderr,"glfw init fail\n");return 1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES,4);
    GLFWwindow* win=glfwCreateWindow(W,H,"twin",0,0); if(!win){fprintf(stderr,"win fail\n");return 1;}
    glfwMakeContextCurrent(win);
    load_gl();

    Mesh M; if(!load_ply(ply,M)) return 1;

    // FBO(멀티샘플) 오프스크린
    GLuint fbo,crb,drb; glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenRenderbuffers(1,&crb);glBindRenderbuffer(GL_RENDERBUFFER,crb);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,4,GL_RGBA8,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,crb);
    glGenRenderbuffers(1,&drb);glBindRenderbuffer(GL_RENDERBUFFER,drb);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER,4,GL_DEPTH_COMPONENT24,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,drb);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){fprintf(stderr,"fbo incomplete\n");return 1;}

    // 메쉬 업로드
    GLuint vao,vbo,ebo; glGenVertexArrays(1,&vao);glBindVertexArray(vao);
    glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,M.verts.size()*4,M.verts.data(),GL_STATIC_DRAW);
    glGenBuffers(1,&ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,M.idx.size()*4,M.idx.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,0,36,(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,0,36,(void*)12);glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,3,GL_FLOAT,0,36,(void*)24);glEnableVertexAttribArray(2);

    GLuint prog=glCreateProgram(); glAttachShader(prog,compile(GL_VERTEX_SHADER,VS));
    glAttachShader(prog,compile(GL_FRAGMENT_SHADER,FS)); glLinkProgram(prog); glUseProgram(prog);

    // 카메라: 건물 중심을 컷 단면 위에서 비스듬히 내려봄
    V3 ctr={(M.lo.x+M.hi.x)*0.5f,(M.lo.y+M.hi.y)*0.5f, cutZ*0.42f};
    float span=std::max(M.hi.x-M.lo.x,M.hi.y-M.lo.y);
    V3 eye={ctr.x+span*0.42f, M.lo.y-span*0.50f, cutZ+span*0.60f};
    M4 P=perspective(40.f*3.1415926f/180.f,(float)W/H,0.5f,300.f);
    M4 Vm=lookat(eye,ctr,{0,0,1});
    M4 MVP=mul(P,Vm);

    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),1,GL_FALSE,MVP.m);
    glUniform3f(glGetUniformLocation(prog,"uCam"),eye.x,eye.y,eye.z);
    glUniform1f(glGetUniformLocation(prog,"uCutZ"),cutZ);
    glUniform3f(glGetUniformLocation(prog,"uFloors"),0.f,3.2f,6.4f);

    glEnable(GL_DEPTH_TEST); glEnable(GL_MULTISAMPLE); glDisable(GL_CULL_FACE);
    glViewport(0,0,W,H);
    glClearColor(0.015f,0.020f,0.030f,1.f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDrawElements(GL_TRIANGLES,(GLsizei)M.idx.size(),GL_UNSIGNED_INT,0);
    glFinish();

    // 멀티샘플 → 일반 FBO 로 resolve 후 읽기
    GLuint rfbo,rtex; glGenFramebuffers(1,&rfbo);glGenTextures(1,&rtex);
    glBindTexture(GL_TEXTURE_2D,rtex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
    glBindFramebuffer(GL_FRAMEBUFFER,rfbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,rtex,0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,rfbo);
    glBlitFramebuffer(0,0,W,H,0,0,W,H,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER,rfbo);
    std::vector<unsigned char> px(W*H*4); glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
    std::vector<unsigned char> fl(W*H*4); for(int y=0;y<H;y++) memcpy(&fl[y*W*4],&px[(H-1-y)*W*4],W*4);
    stbi_write_png(out,W,H,4,fl.data(),W*4);
    fprintf(stderr,"[twin] %s (%dx%d) cutZ=%.2f\n",out,W,H,cutZ);
    glfwDestroyWindow(win); glfwTerminate(); return 0;
}
