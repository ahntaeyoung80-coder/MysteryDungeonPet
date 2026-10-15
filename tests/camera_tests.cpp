#include "core/motion.hpp"
#include "core/surface.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace pet;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
using Pixel=std::array<uint8_t,4>;
void fill(Surface& s,Pixel color) {for(size_t i=0;i<s.pixels.size();i+=4) std::copy(color.begin(),color.end(),s.pixels.begin()+i);}
Pixel terrain(int x,int y) {return (x+y)%7?Pixel{uint8_t(x%200),uint8_t(y%200),80,255}:Pixel{0,0,0,0};}
}
int main() {try {
    Surface map(576,576),backdrop(200,200),actor(16,16),prop(6,6),out(200,200);
    const Pixel base{35,30,25,255},body{10,20,230,255},item{200,100,30,255};
    fill(backdrop,base);fill(actor,body);fill(prop,item);
    for(int y=0;y<576;++y) for(int x=0;x<576;++x) {
        const auto color=terrain(x,y);std::copy(color.begin(),color.end(),map.pixels.begin()+(y*576+x)*4);
    }
    unsigned frames=0;
    for(unsigned scale:{1U,2U,3U}) {
        const int viewport=(200+scale-1)/scale,anchor=viewport/2,maximum=576-viewport;
        // Include real walkable edge tiles, all corners, and both camera clamp transitions.
        const std::vector<double> coordinates={36,anchor-0.5,double(anchor),anchor+0.5,288.25,288.5,288.75,
            maximum+anchor-0.5,double(maximum+anchor),maximum+anchor+0.5,540};
        for(double y:coordinates) for(double x:coordinates) {
            const WorldPoint p{x,y};const auto point=raster(p),camera=cameraFor(p,576,576,viewport);
            check(camera.x>=0&&camera.y>=0&&camera.x<=maximum&&camera.y<=maximum,"camera outside map");
            check(camera.x==std::clamp(point.x-anchor,0,maximum) && camera.y==std::clamp(point.y-anchor,0,maximum),"camera and actor use different raster origin");
            const Point itemPosition{point.x+24,point.y};
            const std::vector<Layer> layers={{&backdrop,{0,0,200,200},{0,0},camera},
                {&map,{0,0,576,576},{0,0},{0,0}}, {&prop,{0,0,6,6},{3,3},itemPosition},
                {&actor,{0,0,16,16},{8,8},point}};
            compose(out,layers,camera,scale);
            // Independent viewport crop oracle: each destination maps to one scene pixel.
            for(int py=0;py<200;++py) for(int px=0;px<200;++px) {
                const int wx=camera.x+px/int(scale),wy=camera.y+py/int(scale);
                auto expected=terrain(wx,wy);if(!expected[3]) expected=base;
                if(wx>=itemPosition.x-3 && wx<itemPosition.x+3 && wy>=itemPosition.y-3 && wy<itemPosition.y+3) expected=item;
                if(wx>=point.x-8 && wx<point.x+8 && wy>=point.y-8 && wy<point.y+8) expected=body;
                const size_t offset=(py*200+px)*4;
                check(std::equal(expected.begin(),expected.end(),out.pixels.begin()+offset),"terrain/actor/prop clipping or alpha differs from viewport crop");
            }
            const auto previous=out.pixels;
            check(!compose(out,layers,camera,scale) && previous==out.pixels,"stationary frame changed pixels");++frames;
        }
        // Half-pixel samples across a diagonal turn must keep the same centered anchor.
        for(int step=0;step<=96;++step) {
            const auto p=WorldPoint{280+step*0.25,280+step*0.25};const auto point=raster(p),camera=cameraFor(p,576,576,viewport);
            check(point.x-camera.x==anchor && point.y-camera.y==anchor,"camera follow introduces a one-pixel wobble");
        }
    }
    check(cameraFor({10,10},24,24,200).x==0,"small scene negative clamp");
    std::cout<<"camera pass: "<<frames<<" frames, 1/2/3x, all edges, clamp transitions, opaque viewport, aligned props and stable raster\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
