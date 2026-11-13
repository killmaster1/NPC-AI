#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
namespace npcai {
struct Point2 { float x,y; };
inline std::optional<Point2> ProjectToUi(float x,float y,float depth,float renderW,float renderH,float uiW,float uiH) {
    for(float n:{x,y,depth,renderW,renderH,uiW,uiH}) if(!std::isfinite(n)) return {};
    if(depth<=0 || renderW<=0 || renderH<=0 || uiW<=0 || uiH<=0) return {};
    Point2 p{x*uiW/renderW,y*uiH/renderH};
    if(p.x<0 || p.x>=uiW || p.y<0 || p.y>=uiH) return {};
    return p;
}
inline std::optional<Point2> BubbleOrigin(Point2 head,float w,float h,float screenW,float screenH,float gap,float tail) {
    for(float n:{head.x,head.y,w,h,screenW,screenH,gap,tail}) if(!std::isfinite(n)) return {};
    if(w<=0 || h<=0 || w>screenW || h>screenH || gap<0 || tail<0) return {};
    const float bottom=head.y-gap-tail;
    // Do not clamp vertically away from the head: hide if it cannot fit above it.
    if(bottom<h || bottom>screenH) return {};
    return Point2{std::clamp(head.x-w*.5f,0.f,screenW-w),bottom-h};
}
}
