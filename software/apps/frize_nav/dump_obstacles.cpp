#include <cstdio>
#include <cstring>
#include "frize/sim/building.hpp"
using namespace frize::sim;
int main(){
  Building b; printf("{\"bounds\":[-1,-1,%.1f,%.1f],\"rects\":[", b.W+2, b.D+2);
  bool first=true;
  for(auto&B:b.boxes()){ if(!strcmp(B.kind,"slab")||!strcmp(B.kind,"roof"))continue;
    if(B.hi.z<0.3||B.lo.z>1.9)continue;
    printf("%s[%.2f,%.2f,%.2f,%.2f]",first?"":",",B.lo.x,B.lo.y,B.hi.x,B.hi.y); first=false; }
  printf("]}\n"); return 0; }
