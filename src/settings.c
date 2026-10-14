#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Coordonnées communes à l'affichage et aux zones interactives du menu. */
static bool coordinates(Renderer *r,int x,int y,float *px,float *py) {
    int w,h;SDL_GetWindowSize(r->window,&w,&h);if(w<=0||h<=0)return false;
    float dw=(float)w,dh=dw*9/16;if(dh>h){dh=(float)h;dw=dh*16/9;}
    *px=((float)x-((float)w-dw)/2)*640/dw;*py=((float)y-((float)h-dh)/2)*360/dh;return true;
}
static bool numeric_row(int row){return row==3||row==9;}
static float value(const App *a,int row){return row==3?a->sensitivity:a->brightness*100;}
static float minimum(int row){return row==3?.3f:50;}
static float maximum(int row){return row==3?3:200;}
static void assign(App *a,int row,float n) {
    n=clampf(n,minimum(row),maximum(row));
    if(row==3)a->sensitivity=roundf(n*100)/100;
    else a->brightness=roundf(n)/100;
}
static void finish(App *a,bool commit) {
    if(a->numeric_edit<0)return;
    if(commit) {
        char *end;float n=strtof(a->numeric_text,&end);
        if(end!=a->numeric_text&&!*end&&isfinite(n))assign(a,a->numeric_edit,n);
    }
    a->numeric_edit=-1;a->select_all=false;SDL_StopTextInput();
}
static void append(App *a,const char *s) {
    for(;*s;s++)if((*s>='0'&&*s<='9')||*s=='.'||*s==','||*s=='-') {
        if(a->select_all){a->numeric_text[0]=0;a->select_all=false;}
        size_t len=strlen(a->numeric_text);
        if(len+1<sizeof(a->numeric_text)){a->numeric_text[len]=*s==','?'.':*s;a->numeric_text[len+1]=0;}
    }
}
bool settings_event(App *a,Renderer *r,const SDL_Event *e) {
    if((a->state!=STATE_MENU&&a->state!=STATE_PAUSED&&a->state!=STATE_DEAD)||!a->menu_page)return false;
    if(e->type==SDL_TEXTINPUT&&a->numeric_edit>=0){append(a,e->text.text);return true;}
    if(e->type==SDL_KEYDOWN&&a->numeric_edit>=0) {
        SDL_Scancode key=e->key.keysym.scancode;
        if(key==SDL_SCANCODE_ESCAPE)finish(a,false);
        else if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER)finish(a,true);
        else if(key==SDL_SCANCODE_BACKSPACE||key==SDL_SCANCODE_DELETE) {
            size_t len=strlen(a->numeric_text);
            if(a->select_all||key==SDL_SCANCODE_DELETE)a->numeric_text[0]=0;
            else if(len)a->numeric_text[len-1]=0;
            a->select_all=false;
        }else if(key==SDL_SCANCODE_A&&(e->key.keysym.mod&KMOD_CTRL))a->select_all=true;
        else if(key==SDL_SCANCODE_V&&(e->key.keysym.mod&KMOD_CTRL)) {
            char *s=SDL_GetClipboardText();if(s){append(a,s);SDL_free(s);}
        }
        return true;
    }
    if(e->type==SDL_KEYDOWN&&numeric_row(a->selected)
        &&(e->key.keysym.scancode==SDL_SCANCODE_RETURN||e->key.keysym.scancode==SDL_SCANCODE_KP_ENTER)) {
        a->numeric_edit=a->selected;a->select_all=true;
        snprintf(a->numeric_text,sizeof(a->numeric_text),a->selected==3?"%.2f":"%.0f",value(a,a->selected));
        SDL_StartTextInput();return true;
    }
    if(e->type==SDL_MOUSEBUTTONUP&&e->button.button==SDL_BUTTON_LEFT&&a->slider_drag>=0){a->slider_drag=-1;return true;}
    if(e->type==SDL_MOUSEMOTION) {
        if(a->slider_drag>=0) {
            float x,y;if(coordinates(r,e->motion.x,e->motion.y,&x,&y))
                assign(a,a->slider_drag,minimum(a->slider_drag)+clampf((x-320)/150,0,1)*(maximum(a->slider_drag)-minimum(a->slider_drag)));
            return true;
        }
        if(a->numeric_edit>=0)return true;
    }
    if(e->type==SDL_MOUSEBUTTONDOWN&&e->button.button==SDL_BUTTON_LEFT) {
        float x,y;if(!coordinates(r,e->button.x,e->button.y,&x,&y))return false;
        finish(a,true);
        int row=menu_row_id(a,(int)floorf((y-128)/18));
        if(x<64||x>576||y<128||row<0||!numeric_row(row))return false;
        a->selected=row;
        if(x>=308&&x<=476) {
            a->slider_drag=row;
            assign(a,row,minimum(row)+clampf((x-320)/150,0,1)*(maximum(row)-minimum(row)));return true;
        }
        if(x>=488&&x<=556) {
            a->numeric_edit=row;a->select_all=true;
            snprintf(a->numeric_text,sizeof(a->numeric_text),row==3?"%.2f":"%.0f",value(a,row));
            SDL_StartTextInput();return true;
        }
    }
    return false;
}
void settings_controls(Mesh *m,const App *a,int row,float y) {
    if(!numeric_row(row))return;
    float t=(value(a,row)-minimum(row))/(maximum(row)-minimum(row));
    Color blue={.35f,.8f,1};
    mesh_rect(m,320,y+2,150,4,(Color){.19f,.24f,.26f});mesh_rect(m,320,y+2,150*t,4,blue);
    mesh_rect(m,318+150*t,y-1,5,10,blue);
    mesh_rect(m,488,y-3,68,13,a->numeric_edit==row?(Color){.16f,.29f,.35f}:(Color){.1f,.15f,.17f});
    char text[20];
    if(a->numeric_edit==row)snprintf(text,sizeof(text),"%.15s_",a->numeric_text);
    else snprintf(text,sizeof(text),row==3?"%.2f":"%.0f%%",value(a,row));
    mesh_text(m,493,y,1,(Color){.92f,.95f,.9f},text);
}
