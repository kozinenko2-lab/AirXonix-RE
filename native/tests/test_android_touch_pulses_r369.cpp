#include <cassert>
#include <cstdint>
#include "platform/android_touch_menu.hpp"
int main(){
    AndroidTouchMenu t;
    const unsigned scene=1;
    // First neutral frame establishes the initial scene.
    assert(t.frame(0,0,true,0,true,scene)==0);
    assert(t.frame(0,16,true,16,true,scene)==16);
    assert(t.frame(0,0,true,32,true,scene)==0); // mandatory latch release
    assert(t.frame(0,32,true,48,true,scene)==32);
    assert(t.frame(0,0,true,64,true,scene)==0);
    // Direction switch while game is animating: queue UP for later.
    assert(t.frame(0,2,true,80,true,scene)==2);
    assert(t.frame(0,1,true,96,false,scene)==0);
    assert(t.frame(0,0,true,112,false,scene)==0);
    assert(t.frame(0,0,true,128,true,scene)==0);
    assert(t.frame(0,0,true,144,true,scene)==1);
    assert(t.frame(0,0,true,160,true,scene)==0);
    // New scene suppresses the old confirm A until physical release.
    assert(t.frame(16,16,true,176,false,2)==0);
    assert(t.frame(16,0,true,192,true,2)==0);
    assert(t.frame(0,0,true,208,true,2)==0);
    assert(t.frame(16,16,true,224,true,2)==16);
    // Gameplay movements stay continuous, and touch releases never leak.
    assert(t.frame(0,0,false,240,true,3)==0);
    assert(t.frame(1|16,1|16,false,256,true,3)==(1|16)); // fresh gameplay touch is continuous
    assert(t.frame(0,0,false,272,true,3)==0);
    assert(t.frame(1|16,1|16,false,288,true,3)==(1|16));
}
