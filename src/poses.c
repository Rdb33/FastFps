#include "fastfps.h"

static float ease(float t){t=clampf(t,0,1);return t*t*(3-2*t);}
static float gesture(float t,float begin,float hold,float release,float end) {
    return ease((t-begin)/(hold-begin))*(1-ease((t-release)/(end-release)));
}
ReloadPose weapon_reload_pose(const Player *p) {
    ReloadPose r={0};if(p->reload_timer<=0||p->reload_duration<=0)return r;
    r.active=true;r.progress=clampf(1-p->reload_timer/p->reload_duration,0,1);
    r.lower=gesture(r.progress,0,.16f,.86f,1);
    r.extract=gesture(r.progress,.16f,.34f,.48f,.70f);
    r.seat=gesture(r.progress,.57f,.71f,.76f,.84f);
    r.rack=gesture(r.progress,.78f,.85f,.90f,.98f);return r;
}
