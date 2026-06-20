// frize_mission render ― 트윈 성장 애니메이션 렌더러(오프스크린 GL 포인트클라우드)
//
// points.bin(누적 스캔점, reveal_frame 포함)을 한 번 업로드하고, 프레임마다
// uFrame 만 바꿔 reveal≤uFrame 점만 그린다 → 드론이 훑는 대로 트윈이 채워짐.
// 화점(열) 포인트는 발광/확대. 매 프레임 MVP 를 cam.bin 으로 덤프해 콕핏
// 컴포지터가 드론/생존자 월드좌표를 화면에 정확히 투영하게 한다.
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
#include <sstream>
#include <algorithm>

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
static V3 nz(V3 a){float l=std::sqrt(dot(a,a));return l>1e-9f?V3{a.x/l,a.y/l,a.z/l}:a;}
struct M4{float m[16];};
static M4 mul(const M4&a,const M4&b){M4 r{};for(int c=0;c<4;c++)for(int rr=0;rr<4;rr++){float s=0;for(int k=0;k<4;k++)s+=a.m[k*4+rr]*b.m[c*4+k];r.m[c*4+rr]=s;}return r;}
static M4 persp(float fy,float as,float n,float f){float t=1.f/std::tan(fy*.5f);M4 r{};r.m[0]=t/as;r.m[5]=t;r.m[10]=(f+n)/(n-f);r.m[11]=-1;r.m[14]=2*f*n/(n-f);return r;}
static M4 look(V3 e,V3 c,V3 up){V3 f=nz(sub(c,e)),s=nz(cross(f,up)),u=cross(s,f);M4 r{};
    r.m[0]=s.x;r.m[4]=s.y;r.m[8]=s.z;r.m[1]=u.x;r.m[5]=u.y;r.m[9]=u.z;r.m[2]=-f.x;r.m[6]=-f.y;r.m[10]=-f.z;
    r.m[12]=-dot(s,e);r.m[13]=-dot(u,e);r.m[14]=dot(f,e);r.m[15]=1;return r;}

static GLuint comp(GLenum t,const char*s){GLuint sh=glCreateShader(t);glShaderSource(sh,1,&s,0);glCompileShader(sh);
    GLint ok;glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);if(!ok){char l[2048];glGetShaderInfoLog(sh,2048,0,l);fprintf(stderr,"sh:%s\n",l);}return sh;}

static const char* VS=R"(#version 330 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aCol; layout(location=2) in float aReveal;
uniform mat4 uMVP; uniform vec3 uCam; uniform float uFrame; uniform float uCutZ;
out vec3 vCol; out float vHeat; out float vFade;
void main(){
    if(aReveal>uFrame || aPos.z>uCutZ){ gl_Position=vec4(2.0,2.0,2.0,1.0); gl_PointSize=0.0; return; } // 미래/천장 클립
    vCol=aCol; vHeat=clamp((aCol.r-aCol.b)*1.6-0.1,0.0,1.0);
    vFade=clamp(1.0-(uFrame-aReveal)/6.0,0.0,1.0);   // 갓 찍힌 점 반짝
    vec4 cs=uMVP*vec4(aPos,1.0); gl_Position=cs;
    float d=length(uCam-aPos);
    gl_PointSize=clamp(240.0/d,2.4,8.5)+vHeat*3.0+vFade*2.5;  // 굵은 포인트(저화질 라이브 센서 느낌)
}
)";
static const char* FS=R"(#version 330 core
in vec3 vCol; in float vHeat; in float vFade; out vec4 o;
void main(){
    vec2 q=gl_PointCoord*2.0-1.0; float r=dot(q,q); if(r>1.0) discard;
    float a=smoothstep(1.0,0.15,r);
    vec3 c=vCol*(0.75+0.5*vHeat);
    c+=mix(vec3(1.0,0.45,0.12),vec3(1.0,0.85,0.45),vHeat)*pow(vHeat,1.4)*1.8; // 화점 발광
    c+=vec3(0.5,0.7,0.9)*vFade*0.9;                                          // 신규점 청백 플래시
    o=vec4(c,a);
}
)";

// 아주 작은 JSON 정수 추출(키 검색)
static int jint(const std::string& s,const std::string& key,int def){
    size_t p=s.find("\""+key+"\""); if(p==std::string::npos)return def; p=s.find(':',p); if(p==std::string::npos)return def;
    return std::atoi(s.c_str()+p+1);
}
static float jflt(const std::string& s,const std::string& key,float def){
    size_t p=s.find("\""+key+"\""); if(p==std::string::npos)return def; p=s.find(':',p); if(p==std::string::npos)return def;
    return std::atof(s.c_str()+p+1);
}

int main(int argc,char**argv){
    std::string dir = argc>1?argv[1]:"/tmp/mission";
    std::string outdir = argc>2?argv[2]:"/tmp/mission/frames";
    int W=argc>3?std::atoi(argv[3]):1280, H=argc>4?std::atoi(argv[4]):720;
    int LO=argc>5?std::atoi(argv[5]):0, HI=argc>6?std::atoi(argv[6]):-1; // 프레임 범위(캘리브레이션)
    int SAMP=argc>7?std::atoi(argv[7]):2;                                 // MSAA
    float CUTZ=argc>8?std::atof(argv[8]):2.5f;                            // 천장 컷(돌하우스)

    std::string mj; { std::ifstream f(dir+"/mission.json"); std::stringstream ss; ss<<f.rdbuf(); mj=ss.str(); }
    int NF=jint(mj,"nframes",170); long NP=jint(mj,"npoints",0);
    float BW=jflt(mj,"W",20),BD=jflt(mj,"D",15),BH=jflt(mj,"H",9.6f);

    // points.bin: Rec{float x,y,z; u8 r,g,b,a; u16 reveal} = 18 bytes
    std::ifstream pf(dir+"/points.bin",std::ios::binary);
    pf.seekg(0,std::ios::end); long bytes=pf.tellg(); pf.seekg(0); long n=bytes/18;
    std::vector<float> vb(n*7); // pos3 col3 reveal1
    std::vector<char> raw(bytes); pf.read(raw.data(),bytes);
    for(long i=0;i<n;i++){ char* p=&raw[i*18]; float x,y,z; memcpy(&x,p,4);memcpy(&y,p+4,4);memcpy(&z,p+8,4);
        uint8_t r=p[12],g=p[13],b=p[14]; uint16_t rev; memcpy(&rev,p+16,2);
        float* o=&vb[i*7]; o[0]=x;o[1]=y;o[2]=z;o[3]=r/255.f;o[4]=g/255.f;o[5]=b/255.f;o[6]=rev; }
    fprintf(stderr,"[render] %ld points, NF=%d\n",n,NF);

    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_SAMPLES,4);
    GLFWwindow* win=glfwCreateWindow(W,H,"m",0,0);if(!win)return 1;glfwMakeContextCurrent(win);load_gl();

    GLuint fbo,crb,drb;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenRenderbuffers(1,&crb);glBindRenderbuffer(GL_RENDERBUFFER,crb);glRenderbufferStorageMultisample(GL_RENDERBUFFER,SAMP,GL_RGBA8,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,crb);
    glGenRenderbuffers(1,&drb);glBindRenderbuffer(GL_RENDERBUFFER,drb);glRenderbufferStorageMultisample(GL_RENDERBUFFER,SAMP,GL_DEPTH_COMPONENT24,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,drb);

    GLuint vao,vbo;glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,vb.size()*4,vb.data(),GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,0,28,(void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,0,28,(void*)12);glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,1,GL_FLOAT,0,28,(void*)24);glEnableVertexAttribArray(2);
    GLuint prog=glCreateProgram();glAttachShader(prog,comp(GL_VERTEX_SHADER,VS));glAttachShader(prog,comp(GL_FRAGMENT_SHADER,FS));glLinkProgram(prog);glUseProgram(prog);

    glEnable(GL_DEPTH_TEST);glEnable(GL_MULTISAMPLE);glEnable(0x8642/*PROGRAM_POINT_SIZE*/);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0,0,W,H);

    GLuint rfbo,rtex;glGenFramebuffers(1,&rfbo);glGenTextures(1,&rtex);glBindTexture(GL_TEXTURE_2D,rtex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,0);
    glBindFramebuffer(GL_FRAMEBUFFER,rfbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,rtex,0);

    V3 ctr={BW*0.5f,BD*0.5f,BH*0.32f};
    float span=std::max(BW,BD);
    std::vector<float> cams; cams.reserve(NF*16);
    std::vector<unsigned char> px(W*H*4),fl(W*H*4);

    for(int f=0; f<NF; ++f){
        // 느린 궤도 + 살짝 상하 흔들림(현장감)
        float az=-1.02f + 0.72f*((float)f/NF);   // 천천히 회전
        float el=0.52f + 0.04f*std::sin(f*0.05f);
        float dist=span*0.90f;                    // 더 가까이 → 트윈 화면 꽉 채움
        V3 eye={ctr.x+dist*std::cos(az)*std::cos(el), ctr.y+dist*std::sin(az)*std::cos(el), ctr.z+dist*std::sin(el)};
        M4 P=persp(42.f*3.14159f/180.f,(float)W/H,0.4f,400.f), Vm=look(eye,ctr,{0,0,1}), MVP=mul(P,Vm);
        for(int i=0;i<16;i++) cams.push_back(MVP.m[i]);
        if(f<LO || (HI>=0 && f>=HI)) continue;   // 범위 밖은 cam 만 계산(캘리브레이션)

        glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),1,GL_FALSE,MVP.m);
        glUniform3f(glGetUniformLocation(prog,"uCam"),eye.x,eye.y,eye.z);
        glUniform1f(glGetUniformLocation(prog,"uFrame"),(float)f);
        glUniform1f(glGetUniformLocation(prog,"uCutZ"),CUTZ);
        glClearColor(0.012f,0.017f,0.027f,1.f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_POINTS,0,(GLsizei)n);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,rfbo);
        glBlitFramebuffer(0,0,W,H,0,0,W,H,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER,rfbo);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,px.data());
        for(int y=0;y<H;y++) memcpy(&fl[y*W*4],&px[(H-1-y)*W*4],W*4);
        char path[512];snprintf(path,sizeof(path),"%s/twin_%04d.png",outdir.c_str(),f);
        stbi_write_png(path,W,H,4,fl.data(),W*4);
    }
    // 카메라 MVP 덤프(컴포지터가 3D→2D 투영)
    std::ofstream cf(dir+"/cam.bin",std::ios::binary); cf.write((char*)cams.data(),cams.size()*4);
    { std::ofstream vf(dir+"/view.json"); vf<<"{\"W\":"<<W<<",\"H\":"<<H<<",\"nframes\":"<<NF<<"}"; }
    fprintf(stderr,"[render] wrote %d frames + cam.bin\n",NF);
    glfwTerminate();return 0;
}
