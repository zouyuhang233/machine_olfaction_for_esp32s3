#include "ui_flow.h"
#include "../ui_helpers.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#if LVGL_VERSION_MAJOR == 8
#define FLOW_ALLOC lv_mem_alloc
#define FLOW_FREE lv_mem_free
#define FLOW_TIMER_DELETE lv_timer_del
#define FLOW_ANIM_DELETE lv_anim_del
#define FLOW_SCREEN lv_obj_get_screen
#else
#define FLOW_ALLOC lv_malloc
#define FLOW_FREE lv_free
#define FLOW_TIMER_DELETE lv_timer_delete
#define FLOW_ANIM_DELETE lv_anim_delete
#define FLOW_SCREEN lv_obj_get_screen
#endif
/* 值类型0表示不可用；1=int32，2=double，3=bool，4=UTF-8字符串。 */
typedef struct aido_flow_queue { int types[8]; double numbers[8]; char * texts[8]; struct aido_flow_queue * next; } flow_queue_t;
typedef struct flow_variable { const char * id; aido_flow_value_t value; struct flow_variable * next; } flow_variable_t;
struct aido_flow_animation { lv_anim_t * animation; unsigned group; int status; struct aido_flow_animation * next; struct aido_flow_animation * next_global; aido_flow_state_t * owner; struct aido_flow_animation * next_channel; bool owns_channel; };
static aido_flow_animation_t * playing_animations;
static aido_flow_state_t * flows;
static flow_variable_t * variables;
static lv_timer_t * scheduler;
static bool dispatching;
static bool enabled;
static bool destroy_requested;
static unsigned dispatch_depth;
typedef struct flow_observer { lv_obj_t * object; struct flow_observer * next; } flow_observer_t;
static flow_observer_t * observed_objects;
static void flow_tick(lv_timer_t * timer);
static void flow_lifecycle(lv_event_t * event);
static void free_animation_record(aido_flow_animation_t * item) {
    aido_flow_animation_t ** link=&playing_animations; while(*link && *link!=item) link=&(*link)->next_global;
    if(*link) *link=item->next_global;
    if(item->animation) {
        ui_anim_user_data_t * data=lv_anim_get_user_data(item->animation);
        aido_flow_animation_t ** channel=(aido_flow_animation_t **)&data->flow_watch;
        while(*channel && *channel!=item) channel=&(*channel)->next_channel;
        if(*channel) *channel=item->next_channel;
    }
    FLOW_FREE(item);
}
static void clear_animations(aido_flow_state_t * s, bool cancel) {
    aido_flow_animation_t * item = s->animations; s->animations = NULL;
    while(item) {
        aido_flow_animation_t * next=item->next;
        if(!cancel && item->status==0 && item->animation) { item->owner=NULL;item->next=NULL; }
        else { if(item->status==0 && item->animation && item->owns_channel) FLOW_ANIM_DELETE(item->animation,NULL); free_animation_record(item); }
        item=next;
    }
}
static void free_queue_item(flow_queue_t * item) { for(unsigned i=0;i<8;i++) if(item->texts[i]) FLOW_FREE(item->texts[i]); FLOW_FREE(item); }
static void clear_queue(aido_flow_state_t * s) {
    while(s->first) { flow_queue_t * item = (flow_queue_t *)s->first; s->first = item->next; free_queue_item(item); }
    s->last = NULL; s->queued = 0;
}
void aido_flow_error(aido_flow_state_t * s, const char * message) {
    if(!s->failed) { LV_LOG_WARN("Flow %s node %s: %s", s->definition->id, s->node ? s->node : "", message); }
    s->failed = true;
}
aido_flow_value_t aido_flow_number(aido_flow_state_t * s, int type, double number) {
    aido_flow_value_t result = {0};
    if(!isfinite(number) || (type == 1 && (number != trunc(number) || number < -2147483648.0 || number > 2147483647.0))) { aido_flow_error(s,"Numeric overflow"); return result; }
    result.type = type; result.number = number; return result;
}
aido_flow_value_t aido_flow_boolean(bool value) { aido_flow_value_t result = {0}; result.type = 3; result.number = value; return result; }
aido_flow_value_t aido_flow_string(aido_flow_state_t * s, const char * text) {
    aido_flow_value_t result = {0};
    if(!text || strlen(text) > 1024) { aido_flow_error(s,"String exceeds 1024 UTF-8 bytes"); return result; }
    result.type = 4; strcpy(result.text,text); return result;
}
bool aido_flow_bool(aido_flow_state_t * s, aido_flow_value_t value) { if(value.type != 3) aido_flow_error(s,"Boolean required"); return value.type == 3 && value.number != 0; }
double aido_flow_num(aido_flow_state_t * s, aido_flow_value_t value) { if(value.type != 1 && value.type != 2) aido_flow_error(s,"Number required"); return value.number; }
const char * aido_flow_text(aido_flow_state_t * s, const aido_flow_value_t * value) { if(value->type != 4) aido_flow_error(s,"String required"); return value->text; }

/* 与 ECMAScript String(number) 一致：取最短可往返十进制，再统一科学计数阈值。 */
static void flow_number_text(double value, char * out) {
    if(value==0) { strcpy(out,"0"); return; }
    char raw[64];
    for(int precision=1;precision<=17;precision++) { snprintf(raw,sizeof raw,"%.*g",precision,value); if(strtod(raw,NULL)==value) break; }
    char * exponent=strchr(raw,'e'); if(!exponent) exponent=strchr(raw,'E');
    if(!exponent) { strcpy(out,raw); return; }
    int power=atoi(exponent+1); *exponent='\0';
    bool negative=raw[0]=='-'; const char * source=raw+(negative?1:0);
    char digits[32]; unsigned count=0; for(;*source;source++) if(*source!='.') digits[count++]=*source; digits[count]='\0';
    char * cursor=out; if(negative) *cursor++='-';
    if(power>=-6 && power<21) {
        int point=power+1;
        if(point<=0) { *cursor++='0'; *cursor++='.'; for(int i=0;i<-point;i++) *cursor++='0'; memcpy(cursor,digits,count);cursor+=count; }
        else { for(int i=0;i<point;i++) *cursor++=i<(int)count?digits[i]:'0'; if(point<(int)count) { *cursor++='.';memcpy(cursor,digits+point,count-point);cursor+=count-point; } }
        *cursor='\0';
    } else { *cursor++=digits[0]; if(count>1) { *cursor++='.';memcpy(cursor,digits+1,count-1);cursor+=count-1; } sprintf(cursor,"e%s%d",power>=0?"+":"",power); }
}
static bool flow_decimal(const char * text) {
    const unsigned char * p=(const unsigned char *)text; if(*p=='+' || *p=='-') ++p;
    unsigned digits=0; while(isdigit(*p)) { ++digits; ++p; }
    if(*p=='.') { ++p; while(isdigit(*p)) { ++digits; ++p; } }
    if(!digits) return false;
    if(*p=='e' || *p=='E') { ++p;if(*p=='+' || *p=='-')++p; unsigned exponent=0;while(isdigit(*p)){++p;++exponent;}if(!exponent)return false; }
    return *p=='\0';
}
/* JS 字符串比较使用 UTF-16 单元；非BMP字符必须先拆成代理对，不能直接strcmp UTF-8。 */
static uint32_t flow_utf16_next(const unsigned char ** input, uint32_t * pending) {
    if(*pending) { uint32_t result=*pending;*pending=0;return result; }
    const unsigned char * p=*input; if(!*p)return 0;
    uint32_t value=*p++; unsigned remaining=0;
    if(value>=0xf0){value&=7;remaining=3;}else if(value>=0xe0){value&=15;remaining=2;}else if(value>=0xc0){value&=31;remaining=1;}
    while(remaining-- && *p) value=(value<<6)|(*p++&63);
    *input=p;
    if(value>0xffff){value-=0x10000;*pending=0xdc00+(value&1023);return 0xd800+(value>>10);}
    return value;
}
static int flow_text_compare(const char * left,const char * right) {
    const unsigned char * a=(const unsigned char *)left,* b=(const unsigned char *)right; uint32_t pa=0,pb=0;
    for(;;){uint32_t x=flow_utf16_next(&a,&pa),y=flow_utf16_next(&b,&pb);if(x!=y)return x<y?-1:1;if(!x)return 0;}
}

aido_flow_value_t aido_flow_convert(aido_flow_state_t * s, int type, aido_flow_value_t value) {
    if(type == value.type) return value;
    if(!value.type) { aido_flow_error(s,"Unavailable value"); return value; }
    if(type == 4) { char text[1025]; if(value.type == 3) strcpy(text,value.number ? "true" : "false"); else flow_number_text(value.number,text); return aido_flow_string(s,text); }
    if(type == 3) { if(value.type == 4) { if(strcmp(value.text,"true") && strcmp(value.text,"false")) aido_flow_error(s,"Invalid boolean string"); return aido_flow_boolean(!strcmp(value.text,"true")); } return aido_flow_boolean(value.number != 0); }
    double number = value.number;
    if(value.type == 4) { char * end; if(!flow_decimal(value.text)) { aido_flow_error(s,"Invalid numeric string"); return value; } number = strtod(value.text,&end); if(*end || !isfinite(number)) aido_flow_error(s,"Invalid numeric string"); }
    return aido_flow_number(s,type,type == 1 ? trunc(number) : number);
}
aido_flow_value_t aido_flow_unary(aido_flow_state_t * s, bool logical, aido_flow_value_t value) { return logical ? aido_flow_boolean(!aido_flow_bool(s,value)) : aido_flow_number(s,value.type,-aido_flow_num(s,value)); }
aido_flow_value_t aido_flow_concat(aido_flow_state_t * s, aido_flow_value_t a, aido_flow_value_t b) {
    if(a.type != 4 || b.type != 4 || strlen(a.text) + strlen(b.text) > 1024) { aido_flow_error(s,"Invalid string concatenation"); return a; }
    strcat(a.text,b.text); return a;
}
aido_flow_value_t aido_flow_binary(aido_flow_state_t * s, const char * op, aido_flow_value_t a, aido_flow_value_t b) {
    bool numeric = (a.type == 1 || a.type == 2) && (b.type == 1 || b.type == 2);
    if(!strcmp(op,"==") || !strcmp(op,"!=") || !strcmp(op,"<") || !strcmp(op,"<=") || !strcmp(op,">") || !strcmp(op,">=")) {
        if(!a.type || (!numeric && a.type != b.type) || (a.type == 3 && strcmp(op,"==") && strcmp(op,"!="))) { aido_flow_error(s,"Comparison type mismatch"); return aido_flow_boolean(false); }
        int c = a.type == 4 ? flow_text_compare(a.text,b.text) : a.number < b.number ? -1 : a.number > b.number ? 1 : 0;
        return aido_flow_boolean(!strcmp(op,"==") ? c == 0 : !strcmp(op,"!=") ? c != 0 : !strcmp(op,"<") ? c < 0 : !strcmp(op,"<=") ? c <= 0 : !strcmp(op,">") ? c > 0 : c >= 0);
    }
    if(!numeric || (!strcmp(op,"/") && b.number == 0)) { aido_flow_error(s,"Invalid arithmetic"); return aido_flow_boolean(false); }
    int type = a.type == 2 || b.type == 2 || !strcmp(op,"/") ? 2 : 1;
    double number;
    if(type == 1) { int64_t x=(int64_t)a.number,y=(int64_t)b.number; int64_t n=!strcmp(op,"+") ? x+y : !strcmp(op,"-") ? x-y : x*y; number=(double)n; }
    else number=!strcmp(op,"+") ? a.number+b.number : !strcmp(op,"-") ? a.number-b.number : !strcmp(op,"*") ? a.number*b.number : a.number/b.number;
    return aido_flow_number(s,type,number);
}
static flow_variable_t * variable(aido_flow_state_t * s,const char * id,aido_flow_value_t initial) {
    for(flow_variable_t * item=variables;item;item=item->next) if(!strcmp(item->id,id)) return item;
    flow_variable_t * item=FLOW_ALLOC(sizeof *item); if(!item) { aido_flow_error(s,"Variable allocation failed"); return NULL; }
    item->id=id; item->value=initial; item->next=variables; variables=item; return item;
}
aido_flow_value_t aido_flow_variable(aido_flow_state_t * s,const char * id,aido_flow_value_t initial) { flow_variable_t * item=variable(s,id,initial); return item ? item->value : initial; }
void aido_flow_assign(aido_flow_state_t * s,const char * id,aido_flow_value_t initial,aido_flow_value_t value) {
    flow_variable_t * item=variable(s,id,initial); if(!item) return;
    if(item->value.type != value.type && !(item->value.type == 2 && value.type == 1)) { aido_flow_error(s,"Variable assignment type mismatch"); return; }
    value.type=item->value.type; item->value=value;
}
static const char * fields[8] = {"value","checked","text","selected","key","eventType","gesture","code"};
aido_flow_value_t aido_flow_event(aido_flow_state_t * s,const char * field) { for(unsigned i=0;i<8;i++) if(!strcmp(fields[i],field)) { if(!s->snapshot[i].type) aido_flow_error(s,"Unavailable event field"); return s->snapshot[i]; } aido_flow_error(s,"Unknown event field"); return aido_flow_boolean(false); }
aido_flow_value_t aido_flow_widget(aido_flow_state_t * s,lv_obj_t * object,const char * type,const char * property) {
    if(!object) { aido_flow_error(s,"Missing widget"); return aido_flow_boolean(false); }
    if(!strcmp(property,"checked") || !strcmp(property,"disabled") || !strcmp(property,"pressed") || !strcmp(property,"focused")) return aido_flow_boolean(lv_obj_has_state(object,!strcmp(property,"checked") ? LV_STATE_CHECKED : !strcmp(property,"disabled") ? LV_STATE_DISABLED : !strcmp(property,"pressed") ? LV_STATE_PRESSED : LV_STATE_FOCUSED));
    if(!strcmp(property,"x")) return aido_flow_number(s,1,lv_obj_get_x(object));
    if(!strcmp(property,"y")) return aido_flow_number(s,1,lv_obj_get_y(object));
    if(!strcmp(property,"width")) return aido_flow_number(s,1,lv_obj_get_width(object));
    if(!strcmp(property,"height")) return aido_flow_number(s,1,lv_obj_get_height(object));
    if(!strcmp(property,"opacity")) return aido_flow_number(s,1,lv_obj_get_style_opa(object,0));
    if(!strcmp(property,"value")) {
        if(!strcmp(type,"lv_arc")) return aido_flow_number(s,1,lv_arc_get_value(object));
        if(!strcmp(type,"lv_bar")) return aido_flow_number(s,1,lv_bar_get_value(object));
        if(!strcmp(type,"lv_slider")) return aido_flow_number(s,1,lv_slider_get_value(object));
        if(!strcmp(type,"lv_spinbox")) return aido_flow_number(s,1,lv_spinbox_get_value(object));
    }
    if(!strcmp(property,"selected")) {
        if(!strcmp(type,"lv_dropdown")) return aido_flow_number(s,1,lv_dropdown_get_selected(object));
        if(!strcmp(type,"lv_roller")) return aido_flow_number(s,1,lv_roller_get_selected(object));
    }
    if(!strcmp(property,"text")) {
        if(!strcmp(type,"lv_label")) return aido_flow_string(s,lv_label_get_text(object));
        if(!strcmp(type,"lv_textarea")) return aido_flow_string(s,lv_textarea_get_text(object));
    }
    aido_flow_error(s,"Unreadable widget property"); return aido_flow_boolean(false);
}
/* 完整事件名由当前版本表生成，不能用少量手写枚举造成Run/C快照语义漂移。 */
static const char * flow_event_name(lv_event_code_t code) {
    switch(code) {
        case 1: return "PRESSED";
        case 2: return "PRESSING";
        case 3: return "PRESS_LOST";
        case 4: return "SHORT_CLICKED";
        case 5: return "SINGLE_CLICKED";
        case 6: return "DOUBLE_CLICKED";
        case 7: return "TRIPLE_CLICKED";
        case 8: return "LONG_PRESSED";
        case 9: return "LONG_PRESSED_REPEAT";
        case 10: return "CLICKED";
        case 11: return "RELEASED";
        case 12: return "SCROLL_BEGIN";
        case 13: return "SCROLL_THROW_BEGIN";
        case 14: return "SCROLL_END";
        case 15: return "SCROLL";
        case 16: return "GESTURE";
        case 17: return "KEY";
        case 18: return "ROTARY";
        case 19: return "FOCUSED";
        case 20: return "DEFOCUSED";
        case 21: return "LEAVE";
        case 22: return "HIT_TEST";
        case 23: return "INDEV_RESET";
        case 24: return "HOVER_OVER";
        case 25: return "HOVER_LEAVE";
        case 26: return "COVER_CHECK";
        case 27: return "REFR_EXT_DRAW_SIZE";
        case 28: return "DRAW_MAIN_BEGIN";
        case 29: return "DRAW_MAIN";
        case 30: return "DRAW_MAIN_END";
        case 31: return "DRAW_POST_BEGIN";
        case 32: return "DRAW_POST";
        case 33: return "DRAW_POST_END";
        case 34: return "DRAW_TASK_ADDED";
        case 35: return "VALUE_CHANGED";
        case 36: return "INSERT";
        case 37: return "REFRESH";
        case 38: return "READY";
        case 39: return "CANCEL";
        case 40: return "CREATE";
        case 41: return "DELETE";
        case 42: return "CHILD_CHANGED";
        case 43: return "CHILD_CREATED";
        case 44: return "CHILD_DELETED";
        case 45: return "SCREEN_UNLOAD_START";
        case 46: return "SCREEN_LOAD_START";
        case 47: return "SCREEN_LOADED";
        case 48: return "SCREEN_UNLOADED";
        case 49: return "SIZE_CHANGED";
        case 50: return "STYLE_CHANGED";
        case 51: return "LAYOUT_CHANGED";
        case 52: return "GET_SELF_SIZE";
        case 53: return "INVALIDATE_AREA";
        case 54: return "RESOLUTION_CHANGED";
        case 55: return "COLOR_FORMAT_CHANGED";
        case 56: return "REFR_REQUEST";
        case 57: return "REFR_START";
        case 58: return "REFR_READY";
        case 59: return "RENDER_START";
        case 60: return "RENDER_READY";
        case 61: return "FLUSH_START";
        case 62: return "FLUSH_FINISH";
        case 63: return "FLUSH_WAIT_START";
        case 64: return "FLUSH_WAIT_FINISH";
        case 65: return "VSYNC";
        case 126: return "CHECKED";
        case 127: return "UNCHECKED";
        default: return "";
    }
}
static void snapshot(aido_flow_state_t * s,lv_event_t * e) {
    memset(s->snapshot,0,sizeof s->snapshot);
    if(!e) return;
    lv_obj_t * object=lv_event_get_target(e);
    const char * type=lv_obj_check_type(object,&lv_arc_class) ? "lv_arc" : lv_obj_check_type(object,&lv_slider_class) ? "lv_slider" : lv_obj_check_type(object,&lv_bar_class) ? "lv_bar" : lv_obj_check_type(object,&lv_spinbox_class) ? "lv_spinbox" : lv_obj_check_type(object,&lv_label_class) ? "lv_label" : lv_obj_check_type(object,&lv_textarea_class) ? "lv_textarea" : lv_obj_check_type(object,&lv_dropdown_class) ? "lv_dropdown" : lv_obj_check_type(object,&lv_roller_class) ? "lv_roller" : "";
    if(!strcmp(type,"lv_arc") || !strcmp(type,"lv_bar") || !strcmp(type,"lv_slider") || !strcmp(type,"lv_spinbox")) s->snapshot[0]=aido_flow_widget(s,object,type,"value");
    s->snapshot[1]=aido_flow_boolean(lv_obj_has_state(object,LV_STATE_CHECKED));
    /* 未引用的超长文本不应阻止其它动作；读取该字段时再报告不可用。 */
    if(!strcmp(type,"lv_label") || !strcmp(type,"lv_textarea")) {
        const char * text=!strcmp(type,"lv_label") ? lv_label_get_text(object) : lv_textarea_get_text(object);
        if(text && strlen(text)<=1024) s->snapshot[2]=aido_flow_string(s,text);
    }
    if(!strcmp(type,"lv_dropdown") || !strcmp(type,"lv_roller")) s->snapshot[3]=aido_flow_widget(s,object,type,"selected");
    lv_event_code_t code=lv_event_get_code(e); s->snapshot[7]=aido_flow_number(s,1,code);
    if(code==LV_EVENT_KEY) s->snapshot[4]=aido_flow_number(s,1,lv_event_get_key(e));
    s->snapshot[5]=aido_flow_string(s,flow_event_name(code));
    if(code==LV_EVENT_GESTURE) {
#if LVGL_VERSION_MAJOR == 8
        lv_indev_t * input=lv_indev_get_act();
#else
        lv_indev_t * input=lv_indev_active();
#endif
        if(input) { lv_dir_t direction=lv_indev_get_gesture_dir(input); const char * gesture=direction==LV_DIR_LEFT ? "LEFT" : direction==LV_DIR_RIGHT ? "RIGHT" : direction==LV_DIR_TOP ? "UP" : "DOWN"; s->snapshot[6]=aido_flow_string(s,gesture); char trigger[32]; snprintf(trigger,sizeof trigger,"GESTURE_%s",gesture); s->snapshot[5]=aido_flow_string(s,trigger); }
    }
}
int aido_flow_delay(aido_flow_state_t * s,aido_flow_value_t value) { double time=aido_flow_num(s,value); if(time<0 || time>2147483647.0) aido_flow_error(s,"Invalid delay"); s->wake=lv_tick_get()+(uint32_t)time; s->delayed=true; s->native_event=NULL; return 1; }
void aido_flow_wait_screen(aido_flow_state_t * s,lv_obj_t * screen) {
    if(!screen || !lv_obj_is_valid(screen)) { aido_flow_error(s,"Missing navigation target"); return; }
    s->watched_screen=screen;
#if LVGL_VERSION_MAJOR == 8
    s->waiting_screen=lv_scr_act()==screen ? NULL : screen;
#else
    s->waiting_screen=lv_screen_active()==screen ? NULL : screen;
#endif
    s->native_event=NULL;
}
bool aido_flow_is_waiting(aido_flow_state_t * s) { return s->delayed || s->waiting_screen || s->waiting_group; }
void aido_flow_navigation_started(lv_obj_t * target) {
    for(aido_flow_state_t * s=flows;s;s=s->next) if(s->waiting_screen && s->waiting_screen!=target) aido_flow_error(s,"Navigation superseded");
}
void aido_flow_context_destroyed(void * context) {
    for(aido_flow_state_t * s=flows;s;s=s->next) if(s->context==context) { s->context=NULL; if(!s->definition->project_lifetime) aido_flow_error(s,"Component instance deleted"); }
}
void aido_flow_begin_animation(aido_flow_state_t * s,bool wait) { ++s->group; s->waiting_group=wait ? s->group : 0; if(wait) s->native_event=NULL; }
/* 一个真实通道允许多个FLOW等待者；复用播放时只观察，不取得取消所有权。 */
void aido_flow_animation_completed(lv_anim_t * animation) {
    ui_anim_user_data_t * data=lv_anim_get_user_data(animation);
    for(aido_flow_animation_t * item=data?data->flow_watch:NULL;item;item=item->next_channel) item->status=1;
}
void aido_flow_animation_interrupted(void * opaque,int status) {
    for(aido_flow_animation_t * item=opaque;item;item=item->next_channel) if(!item->status) item->status=status;
}
void aido_flow_animation_deleted(void * opaque) {
    for(aido_flow_animation_t * item=opaque,* next;item;item=next) {
        next=item->next_channel;if(!item->status)item->status=2;item->animation=NULL;
        if(!item->owner)free_animation_record(item);
    }
}
static void track_animation(aido_flow_state_t * s,lv_anim_t * animation,bool owns) {
    if(!animation) { if(s)aido_flow_error(s,"Animation start failed");return; }
    ui_anim_user_data_t * data=lv_anim_get_user_data(animation);
    for(aido_flow_animation_t * old=data->flow_watch;old;old=old->next_channel)
        if(old->owner==s && old->group==(s?s->group:0))return;
    aido_flow_animation_t * item=FLOW_ALLOC(sizeof *item);
    if(!item){if(s)aido_flow_error(s,"Animation allocation failed");return;}
    *item=(aido_flow_animation_t){animation,s?s->group:0,0,s?s->animations:NULL,playing_animations,s,data->flow_watch,owns};
    if(s)s->animations=item;playing_animations=item;data->flow_watch=item;
#if LVGL_VERSION_MAJOR == 8
    lv_anim_set_ready_cb(animation,aido_flow_animation_completed);
#else
    lv_anim_set_completed_cb(animation,aido_flow_animation_completed);
#endif
}
void aido_flow_track_animation(aido_flow_state_t * s,lv_anim_t * animation) { track_animation(s,animation,true); }
void aido_flow_watch_animation(aido_flow_state_t * s,lv_anim_t * animation) { track_animation(s,animation,false); }

/* 9.2在分发期间缓存事件数组地址；所有观察器在创建阶段安装，触发/结束只更新运行状态。 */
void aido_flow_observe(lv_obj_t * object) {
    if(!object) return;
    for(flow_observer_t * item=observed_objects;item;item=item->next) if(item->object==object) return;
    flow_observer_t * item=FLOW_ALLOC(sizeof *item);if(!item) { LV_LOG_WARN("Flow observer allocation failed");return; }
    item->object=object;item->next=observed_objects;observed_objects=item;
    lv_obj_add_event_cb(object,flow_lifecycle,LV_EVENT_ALL,NULL);
}
static void flow_lifecycle(lv_event_t * e) {
    lv_event_code_t code=lv_event_get_code(e); lv_obj_t * object=lv_event_get_target(e);
    for(aido_flow_state_t * s=flows;s;s=s->next) {
        if(code==LV_EVENT_SCREEN_LOADED && s->waiting_screen==object) s->waiting_screen=NULL;
        if(!s->definition->project_lifetime && ((code==LV_EVENT_SCREEN_UNLOADED && s->screen==object) || (code==LV_EVENT_DELETE && (s->source==object || s->screen==object)))) { s->failed=true; clear_queue(s); }
        if(code==LV_EVENT_DELETE) {
            if(s->waiting_screen==object) { s->waiting_screen=NULL; aido_flow_error(s,"Navigation target deleted"); }
            if(s->watched_screen==object) s->watched_screen=NULL;
            if(s->source==object) s->source=NULL;
            if(s->screen==object) s->screen=NULL;
        }
    }
    if(code==LV_EVENT_DELETE) {
        /* 删除一个通道会释放多个无主等待记录，每次从链头重新查找，避免缓存已释放的next。 */
        for(;;) {
            lv_anim_t * target=NULL;
            for(aido_flow_animation_t * item=playing_animations;item;item=item->next_global) {
                ui_anim_user_data_t * data=item->animation?lv_anim_get_user_data(item->animation):NULL;
                if(data && data->target==object) { target=item->animation;aido_flow_animation_interrupted(data->flow_watch,4);data->target=NULL;break; }
            }
            if(!target)break;
            FLOW_ANIM_DELETE(target,NULL);
        }
        flow_observer_t ** link=&observed_objects;
        while(*link && (*link)->object!=object) link=&(*link)->next;
        if(*link) { flow_observer_t * old=*link;*link=old->next;FLOW_FREE(old); }
    }
}
static void detach(aido_flow_state_t * s) { (void)s; }
static bool ready(aido_flow_state_t * s) {
    if(s->failed) return true;
    if(s->delayed) { if((int32_t)(lv_tick_get()-s->wake)<0) return false; s->delayed=false; }
    if(s->waiting_screen) return false;
    if(s->waiting_group) {
        unsigned count=0;
        for(aido_flow_animation_t * a=s->animations;a;a=a->next) if(a->group==s->waiting_group) {
            ++count;
            if(a->status>1) { aido_flow_error(s,a->status==3?"Animation stopped":a->status==4?"Animation target deleted":"Animation replaced"); return true; }
            if(a->status==0) return false;
        }
        if(!count) aido_flow_error(s,"Animation group is empty");
        s->waiting_group=0;
    }
    return true;
}
static void reset(aido_flow_state_t * s) {
    ++s->generation; clear_animations(s,true); clear_queue(s);
    s->pc=0; s->failed=false; s->finished=false; s->delayed=false; s->waiting_screen=NULL; s->waiting_group=0;
    s->watched_screen=NULL;
}
static void clear_all(void) {
    while(flows) { aido_flow_state_t * s=flows; flows=s->next; detach(s); clear_animations(s,true); clear_queue(s); FLOW_FREE(s); }
    while(playing_animations) { aido_flow_animation_t * item=playing_animations;if(item->animation) FLOW_ANIM_DELETE(item->animation,NULL);else free_animation_record(item); }
    while(variables) { flow_variable_t * item=variables;variables=item->next;FLOW_FREE(item); }
    destroy_requested=false;
}
static int execute_current(aido_flow_state_t * s) {
    if(s->failed || s->finished) return 0;
    if(dispatch_depth>=32) { aido_flow_error(s,"Synchronous event nesting exceeds 32"); return 0; }
    uint32_t generation=s->generation;
    ++dispatch_depth; int waiting=s->definition->run(s); --dispatch_depth;
    if(generation==s->generation && !waiting && !s->failed) s->finished=true;
    return waiting;
}
static void start_in_event_scope(aido_flow_state_t * s) {
    if(dispatching) { execute_current(s); s->native_event=NULL; }
    else flow_tick(NULL);
}
static void flow_tick(lv_timer_t * timer) {
    (void)timer; if(dispatching || !enabled) return; dispatching=true;
    aido_flow_state_t ** link=&flows;
    unsigned budget=1024;
    while(*link && budget-- && !destroy_requested) {
        aido_flow_state_t * s=*link;
        if(!ready(s)) { link=&s->next; continue; }
        uint32_t generation=s->generation;
        int waiting=execute_current(s);
        s->native_event=NULL;
        /* 同步动作可能再次触发并重启自身，旧调用栈不得执行或释放新实例状态。 */
        if(s->generation!=generation) continue;
        if(waiting && !s->failed) { link=&s->next; continue; }
        clear_animations(s,s->failed);
        if(s->failed) clear_queue(s);
        if(s->first) {
            flow_queue_t * item=(flow_queue_t *)s->first; s->first=item->next; if(!s->first) s->last=NULL; --s->queued;
            memset(s->snapshot,0,sizeof s->snapshot);
            for(unsigned i=0;i<8;i++) { s->snapshot[i].type=item->types[i];s->snapshot[i].number=item->numbers[i];if(item->texts[i])strcpy(s->snapshot[i].text,item->texts[i]); }
            free_queue_item(item); ++s->generation;
            s->pc=0; s->failed=false; s->finished=false; s->delayed=false; s->waiting_screen=NULL; s->waiting_group=0; continue;
        }
        *link=s->next; detach(s); FLOW_FREE(s);
    }
    dispatching=false;
    if(destroy_requested) clear_all();
}
void aido_flow_init(void) { if(enabled) return; enabled=true; destroy_requested=false; if(!scheduler) scheduler=lv_timer_create(flow_tick,1,NULL); }
void aido_flow_destroy(void) {
    enabled=false; if(scheduler) { FLOW_TIMER_DELETE(scheduler); scheduler=NULL; }
    if(dispatching) { destroy_requested=true; for(aido_flow_state_t * s=flows;s;s=s->next) s->failed=true; return; }
    clear_all();
}
void aido_flow_trigger(const aido_flow_definition_t * definition,void * context,lv_obj_t * source,lv_event_t * event) {
    if(!enabled) return;
    for(aido_flow_state_t * s=flows;s;s=s->next) if(s->definition==definition && s->context==context && s->source==source) {
        if(!definition->reentry) return;
        if(definition->reentry==1) { reset(s); snapshot(s,event); s->native_event=event; start_in_event_scope(s); return; }
        if(s->queued>=8) { LV_LOG_WARN("Flow queue full (8), trigger dropped"); return; }
        flow_queue_t * item=FLOW_ALLOC(sizeof *item); if(!item) { aido_flow_error(s,"Queue allocation failed"); return; }
        memset(item,0,sizeof *item);
        aido_flow_state_t temporary={0}; temporary.definition=definition; snapshot(&temporary,event);
        if(temporary.failed) { free_queue_item(item); aido_flow_error(s,"Event snapshot failed"); return; }
        /* 队列只复制实际文本长度，不为每个数值字段浪费1025字节字符串空间。 */
        for(unsigned i=0;i<8;i++) { item->types[i]=temporary.snapshot[i].type;item->numbers[i]=temporary.snapshot[i].number;
            if(item->types[i]==4) { size_t length=strlen(temporary.snapshot[i].text)+1;item->texts[i]=FLOW_ALLOC(length);if(!item->texts[i]) { free_queue_item(item);aido_flow_error(s,"Snapshot allocation failed");return; }memcpy(item->texts[i],temporary.snapshot[i].text,length); }
        }
        item->next=NULL;
        if(s->last) s->last->next=item; else s->first=item; s->last=item; ++s->queued; return;
    }
    aido_flow_state_t * s=FLOW_ALLOC(sizeof *s); if(!s) { LV_LOG_WARN("Flow allocation failed"); return; } memset(s,0,sizeof *s);
    s->definition=definition; s->context=context; s->source=source; s->screen=source ? FLOW_SCREEN(source) : NULL; s->native_event=event; snapshot(s,event);
    /* 尾插避免同步回调增加流程时破坏当前调度器链表游标。 */
    aido_flow_state_t ** tail=&flows; while(*tail) tail=&(*tail)->next; *tail=s;
    start_in_event_scope(s);
}
