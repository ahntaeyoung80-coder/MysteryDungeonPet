#include "core/exploration.hpp"
#include "core/motion.hpp"
#include "core/animator.hpp"
#include <iostream>
#include <stdexcept>
using namespace pet;
void check(bool ok,const char* s) { if(!ok) throw std::runtime_error(s); }
int main(int argc,char** argv) { try {
    World w;w.terrain.fill(1);int p=10*kWidth+10;
    for(unsigned d=0;d<8;++d) {auto [dx,dy]=kDirections[d];int q=p+dx+dy*kWidth;
        check(directionBetween(p,q)==d,"direction numbering");check(canMove(w,p,q),"8-direction step");
        check(bfs(w,p).distance[q]==1,"diagonal not one action");
        Mask occupied{};occupied[q]=1;check(!canMove(w,p,q,occupied),"destination overlap");
        if(dx&&dy) {occupied[p+dx]=1;occupied[q]=0;check(canMove(w,p,q,occupied),"actor mistaken for terrain corner");
            w.terrain[p+dx]=0;check(!canMove(w,p,q)&&!canAttack(w,p,q),"wall corner pierced");w.terrain[p+dx]=1;}
    }
    check(!canMove(w,23,24),"row wrap");
    Motion motion;motion.begin(1,2,{12,12},{36,36},0,1);
    for(auto time:{0LL,17000LL,200000LL,399999LL,400000LL,900000LL}) {
        auto pos=motion.sample(time);double t=std::min(1.0,double(time)/400000);
        check(std::abs(pos.x-(12+24*t))<1e-6,"frame-dependent position");
    }
    check(!motion.active(400000),"duration changed by diagonal");
    motion.begin(1,3,{36,36},{60,36},400000,2);check(motion.sample(400000).x==36,"continuous endpoint");
    motion.pause(500000);auto held=motion.sample(900000);motion.resume(900000);check(motion.sample(900000).x==held.x,"resume jump");check(!motion.active(1200000),"resume duration");
    auto camera=cameraFor({575,575},576,576,200);check(camera.x==376&&camera.y==376,"camera edge clamp");
    check(cameraFor({1,1},576,576,200).x==0,"camera negative edge");
    if(argc>1) {auto pkg=loadPackage(argv[1]);Animator a;a.start(pkg,0,0,true,0);a.advance(0);
        auto before=a.advance(200000);a.changeDirection(pkg,0,2);auto after=a.advance(200000);
        check(before.frame==after.frame&&before.nextDeadlineUs==after.nextDeadlineUs&&after.markers.empty(),"direction restarts walking");}
    std::cout<<"improvement tests pass\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
