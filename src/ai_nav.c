#include "fastfps.h"
#include <math.h>
#include <string.h>

static const float sizes[3]={1,1.3f,1.586f};
int ai_profile(const Enemy *e){return e->elite?2:((e->type==ENEMY_BRUTE||e->type==ENEMY_BOMBER)?1:0);}
static bool moving(const World *w,int solid) {
    for(int i=0;i<w->platform_count;i++)if(w->platforms[i].solid==solid)return true;
    return false;
}
static bool clear(const World *w,Vec3 p,float r,float h) {
    if(fabsf(p.x)>ARENA_HALF_X-r||fabsf(p.z)>ARENA_HALF_Z-r)return false;
    for(int i=0;i<w->solid_count;i++) {
        if(moving(w,i))continue;
        Box b=w->solids[i];
        if(p.y<b.max.y-.002f&&p.y+h>b.min.y+.002f&&p.x+r>b.min.x&&p.x-r<b.max.x&&p.z+r>b.min.z&&p.z-r<b.max.z)return false;
    }
    return true;
}
static float floor_at(const World *w,Vec3 p,float maximum,float radius,bool stations) {
    float y=0;
    for(int i=0;i<w->solid_count;i++) {
        Box b=w->solids[i];if(moving(w,i))continue;
        if(b.max.y<=maximum+.002f&&p.x+radius>b.min.x&&p.x-radius<b.max.x&&p.z+radius>b.min.z&&p.z-radius<b.max.z)y=fmaxf(y,b.max.y);
    }
    if(stations)for(int i=0;i<w->platform_count;i++)for(int end=0;end<2;end++) {
        const Platform *lift=&w->platforms[i];Vec3 q=vadd(lift->base,vscale(lift->travel,(float)end));q.y+=lift->size.y;
        if(q.y<=maximum+.002f&&fabsf(q.x-p.x)<lift->size.x*.5f&&fabsf(q.z-p.z)<lift->size.z*.5f)y=fmaxf(y,q.y);
    }
    return y;
}
bool ai_nav_segment(const World *w,Vec3 from,Vec3 to,int profile) {
    float r=ENEMY_RADIUS*sizes[profile],h=ENEMY_HEIGHT*sizes[profile],rise=profile==0?1.15f:PLAYER_STEP;
    float length=hypotf(to.x-from.x,to.z-from.z);int steps=(int)ceilf(length/.22f);if(steps<1)steps=1;
    Vec3 previous=from;
    for(int k=1;k<=steps;k++) {
        Vec3 q=vadd(from,vscale(vsub(to,from),(float)k/(float)steps));
        q.y=floor_at(w,q,previous.y+rise,r-.01f,true);
        if(previous.y-q.y>2.5f||q.y-previous.y>rise+.002f||q.y>fmaxf(from.y,to.y)+PLAYER_STEP||!clear(w,q,r,h))return false;
        /* La montée doit aussi laisser passer le corps au-dessus du rebord. */
        if(q.y>previous.y+.01f&&!clear(w,(Vec3){previous.x,q.y,previous.z},r,h))return false;
        previous=q;
    }
    return fabsf(previous.y-to.y)<.12f;
}
static int cell(Vec3 p) {
    int x=(int)floorf((p.x+28)/2),z=(int)floorf((p.z+24)/2);
    if(x<0||x>=NAV_W||z<0||z>=NAV_H)return -1;
    return z*NAV_W+x;
}
static int add_node(World *w,Vec3 p,int lift) {
    AiState *a=&w->ai;int c=cell(p);if(c<0||a->node_count>=AI_NAV_NODES)return -1;
    for(int i=a->cells[c];i>=0;i=a->nodes[i].next)if(vlength(vsub(p,a->nodes[i].position))<.12f&&a->nodes[i].platform==lift)return i;
    uint8_t mask=0;
    for(int k=0;k<3;k++)if(clear(w,p,ENEMY_RADIUS*sizes[k],ENEMY_HEIGHT*sizes[k]))mask|=(uint8_t)(1<<k);
    if(!mask)return -1;
    int i=a->node_count++;a->nodes[i]=(AiNode){.position=p,.mask=mask,.platform=(int8_t)lift,.next=a->cells[c]};a->cells[c]=(int16_t)i;return i;
}
static void edge(AiNode *node,int to,uint8_t mask,int lift,float cost) {
    if(node->count>=AI_NAV_EDGES)return;
    node->edges[node->count++]=(AiEdge){(int16_t)to,mask,(int8_t)lift,cost};
}
void ai_nav_build(World *w) {
    AiState *a=&w->ai;a->node_count=0;memset(a->cells,0xff,sizeof(a->cells));
    for(int z=0;z<NAV_H;z++)for(int x=0;x<NAV_W;x++) {
        Vec3 p={(float)x*2-27,0,(float)z*2-23};add_node(w,p,-1);
        for(int s=0;s<w->solid_count;s++) {
            Box b=w->solids[s];if(moving(w,s)||b.max.y<=0||b.max.y>6)continue;
            if(p.x>=b.min.x&&p.x<=b.max.x&&p.z>=b.min.z&&p.z<=b.max.z){p.y=b.max.y;add_node(w,p,-1);}
            p.y=0;
        }
    }
    /* Les centres des petites marches complètent la grille sur les terrasses. */
    for(int i=0;i<w->solid_count;i++) {
        Box b=w->solids[i];if(moving(w,i)||b.max.y<=0||b.max.y>6)continue;
        add_node(w,(Vec3){(b.min.x+b.max.x)*.5f,b.max.y,(b.min.z+b.max.z)*.5f},-1);
        /* Les marches étroites ont leur zone d'appui décalée du centre :
           le volume du corps touche la marche avant que ses pieds y arrivent.
           Échantillonner les accès pour chaque gabarit conserve les escaliers
           que la grille de deux mètres et les seuls centres ne voient pas. */
        for(int profile=0;profile<3;profile++) {
            float r=ENEMY_RADIUS*sizes[profile];
            Vec3 center={(b.min.x+b.max.x)*.5f,b.max.y,(b.min.z+b.max.z)*.5f};
            for(int side=0;side<4;side++) {
                Vec3 p=center;
                if(side<2)p.x=side==0?b.min.x-r+.25f:b.max.x+r-.25f;
                else p.z=side==2?b.min.z-r+.25f:b.max.z+r-.25f;
                if(clear(w,p,r,ENEMY_HEIGHT*sizes[profile]))add_node(w,p,-1);
            }
        }
    }
    int station[MAX_PLATFORMS][2];memset(station,0xff,sizeof(station));
    for(int i=0;i<w->platform_count;i++)for(int k=0;k<2;k++) {
        Vec3 p=vadd(w->platforms[i].base,vscale(w->platforms[i].travel,(float)k));p.y+=w->platforms[i].size.y;
        station[i][k]=add_node(w,p,i);
        /* Paliers hors de l'empreinte mobile : les noeuds au sol situés sous
           la cabine ne suffisent pas à représenter un embarquement. */
        for(int side=0;side<4;side++) {
            Vec3 q=p;float offset=side<2?w->platforms[i].size.x*.5f+.8f:w->platforms[i].size.z*.5f+.8f;
            if(side<2)q.x+=(side==0?-offset:offset);else q.z+=(side==2?-offset:offset);
            q.y=floor_at(w,q,p.y+PLAYER_STEP,.01f,false);
            if(fabsf(q.y-p.y)<.4f)add_node(w,q,-1);
        }
    }
    for(int i=0;i<a->node_count;i++) {
        AiNode *n=&a->nodes[i];int c=cell(n->position),cx=c%NAV_W,cz=c/NAV_W;
        /* Connexions courtes, avec contrôle complet de hauteur et de gabarit. */
        for(int z=cz-2;z<=cz+2;z++)for(int x=cx-2;x<=cx+2;x++) {
            if(x<0||x>=NAV_W||z<0||z>=NAV_H)continue;
            for(int j=a->cells[z*NAV_W+x];j>=0;j=a->nodes[j].next) {
                AiNode *other=&a->nodes[j];Vec3 d=vsub(other->position,n->position);float length=hypotf(d.x,d.z);
                if(j==i||length>3.05f||fabsf(d.y)>2.6f)continue;
                uint8_t mask=0;
                for(int p=0;p<3;p++)if((n->mask&other->mask&(1<<p))&&ai_nav_segment(w,n->position,other->position,p))mask|=(uint8_t)(1<<p);
                if(mask)edge(n,j,mask,0,vlength(d)+fmaxf(0,d.y)*1.8f+(d.y>.35f?.8f:0));
            }
        }
    }
    for(int i=0;i<w->platform_count;i++)if(station[i][0]>=0&&station[i][1]>=0) {
        AiNode *lo=&a->nodes[station[i][0]],*hi=&a->nodes[station[i][1]];
        /* Réserver une connexion même sur une station très voisine d'un palier. */
        if(lo->count==AI_NAV_EDGES)lo->count--;
        if(hi->count==AI_NAV_EDGES)hi->count--;
        float cost=vlength(w->platforms[i].travel)+w->platforms[i].period*1.4f;
        edge(lo,station[i][1],lo->mask&hi->mask,i+1,cost);edge(hi,station[i][0],lo->mask&hi->mask,i+1,cost);
    }
    a->revision=w->revision;
    for(int i=0;i<MAX_ENEMIES;i++){a->brains[i].path_count=0;a->brains[i].plan_at=-1;a->brains[i].decide_at=0;a->brains[i].riding=false;}
}
static float heuristic(Vec3 a,Vec3 b){return vlength(vsub(a,b));}
static float priority(const AiState *a,int node,int goal){return a->costs[node]+heuristic(a->nodes[node].position,a->nodes[goal].position);}
static void heap_up(AiState *a,int at,int goal) {
    while(at>0) {
        int parent=(at-1)/2;if(priority(a,a->heap[parent],goal)<=priority(a,a->heap[at],goal))break;
        int16_t tmp=a->heap[parent];a->heap[parent]=a->heap[at];a->heap[at]=tmp;
        a->heap_pos[a->heap[parent]]=(int16_t)parent;a->heap_pos[a->heap[at]]=(int16_t)at;at=parent;
    }
}
static int nearest(const World *w,Vec3 p,int profile,bool connected) {
    const AiState *a=&w->ai;float best=1e9f;int found=-1;
    for(int i=0;i<a->node_count;i++)if(a->nodes[i].mask&(1<<profile)) {
        Vec3 d=vsub(a->nodes[i].position,p);float score=d.x*d.x+d.z*d.z+4*d.y*d.y;
        if(score>=best)continue;
        if(connected&&!ai_nav_segment(w,p,a->nodes[i].position,profile))continue;
        best=score;found=i;
    }
    return found;
}
bool ai_plan(Game *g,Enemy *e,AiBrain *b,Vec3 destination) {
    AiState *a=&g->world->ai;int profile=ai_profile(e);
    int start=nearest(g->world,e->body.position,profile,true),goal=nearest(g->world,destination,profile,false);
    b->plan_at=g->time+.65f+(float)(e->id%5)*.06f;b->destination=destination;b->goal=destination;b->revision=a->revision;b->path_count=0;b->path_cursor=0;
    if(start<0||goal<0)return false;
    for(int i=0;i<a->node_count;i++){a->costs[i]=1e9f;a->parents[i]=-1;a->heap_pos[i]=-1;a->closed[i]=0;}
    a->costs[start]=0;a->heap[0]=(int16_t)start;a->heap_pos[start]=0;int count=1,best=start;a->searches++;
    while(count>0) {
        int current=a->heap[0];count--;a->heap_pos[current]=-1;
        if(count>0) {
            a->heap[0]=a->heap[count];a->heap_pos[a->heap[0]]=0;int at=0;
            for(;;) {
                int l=at*2+1,r=l+1,small=at;if(l<count&&priority(a,a->heap[l],goal)<priority(a,a->heap[small],goal))small=l;
                if(r<count&&priority(a,a->heap[r],goal)<priority(a,a->heap[small],goal))small=r;
                if(small==at)break;
                int16_t tmp=a->heap[at];a->heap[at]=a->heap[small];a->heap[small]=tmp;
                a->heap_pos[a->heap[at]]=(int16_t)at;a->heap_pos[a->heap[small]]=(int16_t)small;at=small;
            }
        }
        a->closed[current]=1;a->expansions++;
        if(heuristic(a->nodes[current].position,a->nodes[goal].position)<heuristic(a->nodes[best].position,a->nodes[goal].position))best=current;
        if(current==goal){best=current;break;}
        AiNode *node=&a->nodes[current];
        for(int k=0;k<node->count;k++) {
            AiEdge link=node->edges[k];int next=link.to;if(!(link.mask&(1<<profile))||a->closed[next])continue;
            float crowd=0;
            for(int j=0;j<g->enemy_count;j++)if(g->enemies[j].id!=e->id&&g->enemies[j].health>0) {
                float d=vlength(vsub(a->nodes[next].position,g->enemies[j].body.position));
                if(d<2)crowd+=(2-d)*.8f;
            }
            for(int j=0;j<MAX_ENEMIES;j++) {
                const AiBrain *other=&a->brains[j];
                if(!other->used||other->id==e->id||other->path_cursor>=other->path_count)continue;
                Vec3 reserved=a->nodes[other->path[other->path_cursor]].position;
                float d=vlength(vsub(a->nodes[next].position,reserved));if(d<1.5f)crowd+=(1.5f-d)*1.2f;
            }
            float wait=0;
            if(link.lift>0) {
                const Platform *lift=&g->world->platforms[link.lift-1];
                Vec3 now=platform_position(lift,g->world->platform_time);now.y+=lift->size.y;
                wait=vlength(vsub(now,node->position))*lift->period/fmaxf(1,2*vlength(lift->travel));
            }
            float cost=a->costs[current]+link.cost+crowd+wait*2;
            if(cost>=a->costs[next])continue;
            a->costs[next]=cost;a->parents[next]=(int16_t)current;
            int at=a->heap_pos[next];if(at<0){at=count++;a->heap[at]=(int16_t)next;a->heap_pos[next]=(int16_t)at;}
            heap_up(a,at,goal);
        }
    }
    int16_t reverse[AI_NAV_NODES];int n=0;
    for(int at=best;at>=0&&n<AI_NAV_NODES;at=a->parents[at])reverse[n++]=(int16_t)at;
    for(int i=n-1;i>=0&&b->path_count<AI_PATH_LENGTH;i--)b->path[b->path_count++]=reverse[i];
    b->path_cursor=b->path_count>1?1:0;b->progress_at=g->time;b->last_position=e->body.position;
    return best==goal;
}
Vec3 ai_waypoint(Game *g,Enemy *e,AiBrain *b) {
    AiState *a=&g->world->ai;
    if(b->path_count<=0)return b->goal;
    while(b->path_cursor<b->path_count) {
        AiNode *node=&a->nodes[b->path[b->path_cursor]];Vec3 d=vsub(node->position,e->body.position);
        if(node->platform>=0) {
            const Platform *lift=&g->world->platforms[node->platform];Box box=g->world->solids[lift->solid];
            bool aboard=fabsf(e->body.position.y-box.max.y)<.16f&&e->body.position.x>box.min.x&&e->body.position.x<box.max.x
                &&e->body.position.z>box.min.z&&e->body.position.z<box.max.z;
            bool dock=hypotf((box.min.x+box.max.x)*.5f-node->position.x,(box.min.z+box.max.z)*.5f-node->position.z)<.7f&&fabsf(box.max.y-node->position.y)<.35f;
            if(aboard&&!dock){b->riding=true;return e->body.position;}
            b->riding=false;
            if(!aboard&&!dock&&vlength(d)<3) {
                Vec3 back=b->path_cursor>0?a->nodes[b->path[b->path_cursor-1]].position:e->body.position;
                Vec3 away=vsub(back,node->position);away.y=0;away=vnormalize(away);
                return vadd(node->position,vscale(away,fmaxf(lift->size.x,lift->size.z)*.5f+.7f));
            }
        }
        if(hypotf(d.x,d.z)>.6f||fabsf(d.y)>.25f)break;
        b->path_cursor++;
    }
    if(b->path_cursor>=b->path_count) {
        Vec3 last=a->nodes[b->path[b->path_count-1]].position;
        return ai_nav_segment(g->world,e->body.position,b->goal,ai_profile(e))?b->goal:last;
    }
    /* Lisser seulement un tronçon marchable : ne pas sauter une montée ou un ascenseur. */
    int next=b->path_cursor;
    while(next+1<b->path_count&&next<b->path_cursor+4) {
        AiNode *n=&a->nodes[b->path[next+1]],*previous=&a->nodes[b->path[next]];
        if(n->platform>=0||previous->platform>=0||fabsf(previous->position.y-e->body.position.y)>.3f||fabsf(n->position.y-e->body.position.y)>.3f
            ||!ai_nav_segment(g->world,e->body.position,n->position,ai_profile(e)))break;
        next++;
    }
    b->path_cursor=next;return a->nodes[b->path[next]].position;
}
