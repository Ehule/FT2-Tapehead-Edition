#include "tapesister/ts_editor.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const source_names[]={"SINE","TRIANGLE","SAW","PULSE","CLICK"};
static const char *const noise_names[]={"WHITE","PINKISH","METALLIC"};
static const char *const filter_names[]={"LOW PASS","BAND PASS","HIGH PASS","NOTCH"};
static const char *const shaper_names[]={"SOFT","HARD","FOLD"};
static const char *const finish_names[]={"TARGET PEAK","FIXED HEADROOM"};
#define D(id,p,o,l,u,t,mn,mx,f,c,map,en,n) {id,p,o,l,u,t,map,mn,mx,f,c,en,n}
static const ts_parameter_desc descriptors[]={
 D(TS_P_SOURCE,TS_PAGE_SOURCE,0,"SOURCE","",TS_PARAM_ENUM,0,4,1,1,TS_MAP_CATEGORY,source_names,5),
 D(TS_P_SOURCE_SHAPE,TS_PAGE_SOURCE,1,"SHAPE","%",TS_PARAM_CONTINUOUS,.05,.95,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_HARMONIC_MIX,TS_PAGE_SOURCE,2,"OSC BLEND","%",TS_PARAM_CONTINUOUS,0,1,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_NOISE_TYPE,TS_PAGE_SOURCE,3,"NOISE TYPE","",TS_PARAM_ENUM,0,2,1,1,TS_MAP_CATEGORY,noise_names,3),
 D(TS_P_NOISE_AMOUNT,TS_PAGE_SOURCE,4,"NOISE","%",TS_PARAM_CONTINUOUS,0,1,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_SEED,TS_PAGE_SOURCE,5,"SEED","",TS_PARAM_SEED,0,4294967295.0,1,256,TS_MAP_LINEAR,NULL,0),
 D(TS_P_ATTACK,TS_PAGE_CONTOUR,0,"ATTACK","s",TS_PARAM_CONTINUOUS,0,10,.001,.05,TS_MAP_LINEAR,NULL,0),
 D(TS_P_DECAY,TS_PAGE_CONTOUR,1,"DECAY","s",TS_PARAM_CONTINUOUS,.000001,10,.001,.05,TS_MAP_LOG,NULL,0),
 D(TS_P_SUSTAIN,TS_PAGE_CONTOUR,2,"SUSTAIN","%",TS_PARAM_CONTINUOUS,0,1,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_RELEASE,TS_PAGE_CONTOUR,3,"RELEASE","s",TS_PARAM_CONTINUOUS,0,10,.001,.05,TS_MAP_LINEAR,NULL,0),
 D(TS_P_PITCH_ENV_AMOUNT,TS_PAGE_CONTOUR,4,"PITCH ENV","st",TS_PARAM_CONTINUOUS,-96,96,.01,1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_PITCH_ENV_DECAY,TS_PAGE_CONTOUR,5,"PITCH DECAY","s",TS_PARAM_CONTINUOUS,.000001,10,.001,.05,TS_MAP_LOG,NULL,0),
 D(TS_P_FILTER_ENABLED,TS_PAGE_FILTER,0,"FILTER","",TS_PARAM_BOOLEAN,0,1,1,1,TS_MAP_CATEGORY,NULL,0),
 D(TS_P_FILTER_MODE,TS_PAGE_FILTER,1,"MODE","",TS_PARAM_ENUM,0,3,1,1,TS_MAP_CATEGORY,filter_names,4),
 D(TS_P_FILTER_CUTOFF,TS_PAGE_FILTER,2,"CUTOFF","Hz",TS_PARAM_CONTINUOUS,20,86400,1,100,TS_MAP_LOG,NULL,0),
 D(TS_P_FILTER_RESONANCE,TS_PAGE_FILTER,3,"RESONANCE","%",TS_PARAM_CONTINUOUS,0,.95,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_FILTER_ENV_AMOUNT,TS_PAGE_FILTER,4,"FILTER ENV","oct",TS_PARAM_CONTINUOUS,-16,16,.01,.5,TS_MAP_LINEAR,NULL,0),
 D(TS_P_SHAPER,TS_PAGE_COLOR,0,"SHAPER","",TS_PARAM_ENUM,0,2,1,1,TS_MAP_CATEGORY,shaper_names,3),
 D(TS_P_DRIVE,TS_PAGE_COLOR,1,"DRIVE","x",TS_PARAM_CONTINUOUS,.1,8,.01,.25,TS_MAP_LOG,NULL,0),
 D(TS_P_DELAY_TIME,TS_PAGE_SPACE,0,"DELAY TIME","s",TS_PARAM_CONTINUOUS,0,1,.001,.025,TS_MAP_LINEAR,NULL,0),
 D(TS_P_DELAY_FEEDBACK,TS_PAGE_SPACE,1,"DELAY FB","%",TS_PARAM_CONTINUOUS,0,.85,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_DELAY_MIX,TS_PAGE_SPACE,2,"DELAY WET","%",TS_PARAM_CONTINUOUS,0,1,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_REVERB_DECAY,TS_PAGE_SPACE,3,"AMBIENCE DECAY","%",TS_PARAM_CONTINUOUS,0,.9,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_REVERB_MIX,TS_PAGE_SPACE,4,"AMBIENCE WET","%",TS_PARAM_CONTINUOUS,0,1,.01,.1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_NAME,TS_PAGE_SAMPLE,0,"NAME","",TS_PARAM_NAME,0,127,1,1,TS_MAP_CATEGORY,NULL,0),
 D(TS_P_ROOT_NOTE,TS_PAGE_SAMPLE,1,"ROOT NOTE","MIDI",TS_PARAM_INTEGER,0,127,1,12,TS_MAP_LINEAR,NULL,0),
 D(TS_P_FINE_TUNE,TS_PAGE_SAMPLE,2,"FINE TUNE","cent",TS_PARAM_INTEGER,-100,100,.01,1,TS_MAP_LINEAR,NULL,0),
 D(TS_P_SAMPLE_RATE,TS_PAGE_SAMPLE,3,"SAMPLE RATE","Hz",TS_PARAM_SAMPLE_RATE,8000,192000,1,1,TS_MAP_CATEGORY,NULL,0),
 D(TS_P_RENDER_FRAMES,TS_PAGE_SAMPLE,4,"RENDER DURATION","s",TS_PARAM_CONTINUOUS,.02,10,.001,.1,TS_MAP_LOG,NULL,0),
 D(TS_P_FINISHING_MODE,TS_PAGE_SAMPLE,5,"FINISHING","",TS_PARAM_ENUM,0,1,1,1,TS_MAP_CATEGORY,finish_names,2),
 D(TS_P_TARGET_PEAK,TS_PAGE_SAMPLE,6,"TARGET PEAK","%",TS_PARAM_CONTINUOUS,.1,.95,.01,.05,TS_MAP_LINEAR,NULL,0),
 D(TS_P_FIXED_GAIN,TS_PAGE_SAMPLE,7,"FIXED GAIN","dB",TS_PARAM_INTEGER,-96,0,.01,1,TS_MAP_LINEAR,NULL,0)};
#undef D
const ts_parameter_desc *ts_parameter_descriptors(size_t *n){if(n)*n=TS_PARAMETER_COUNT;return descriptors;}
const ts_parameter_desc *ts_parameter_by_id(ts_parameter_id id){return (id<TS_PARAMETER_COUNT)?&descriptors[id]:NULL;}
bool ts_parameter_enabled(ts_parameter_id id,const ts_recipe*r){if(!r)return false;if(id>=TS_P_FILTER_MODE&&id<=TS_P_FILTER_ENV_AMOUNT)return r->filter_enabled;if(id==TS_P_TARGET_PEAK)return r->finishing_mode==TS_FINISH_TARGET_PEAK;if(id==TS_P_FIXED_GAIN)return r->finishing_mode==TS_FINISH_FIXED_HEADROOM;if(id==TS_P_SOURCE_SHAPE)return r->source==TS_SOURCE_PULSE;return true;}
#define G(x) *v=(double)r->x
bool ts_parameter_get_number(ts_parameter_id id,const ts_recipe*r,double*v){if(!r||!v)return false;switch(id){case TS_P_SOURCE:G(source);break;case TS_P_SOURCE_SHAPE:G(source_shape);break;case TS_P_HARMONIC_MIX:G(harmonic_mix);break;case TS_P_NOISE_TYPE:G(noise_type);break;case TS_P_NOISE_AMOUNT:G(noise_amount);break;case TS_P_SEED:G(seed);break;case TS_P_ATTACK:G(attack_seconds);break;case TS_P_DECAY:G(decay_seconds);break;case TS_P_SUSTAIN:G(sustain_level);break;case TS_P_RELEASE:G(release_seconds);break;case TS_P_PITCH_ENV_AMOUNT:G(pitch_env_semitones);break;case TS_P_PITCH_ENV_DECAY:G(pitch_env_seconds);break;case TS_P_FILTER_ENABLED:G(filter_enabled);break;case TS_P_FILTER_MODE:G(filter_mode);break;case TS_P_FILTER_CUTOFF:G(filter_cutoff_hz);break;case TS_P_FILTER_RESONANCE:G(filter_resonance);break;case TS_P_FILTER_ENV_AMOUNT:G(filter_env_octaves);break;case TS_P_SHAPER:G(shaper);break;case TS_P_DRIVE:G(drive);break;case TS_P_DELAY_TIME:G(delay_seconds);break;case TS_P_DELAY_FEEDBACK:G(delay_feedback);break;case TS_P_DELAY_MIX:G(delay_mix);break;case TS_P_REVERB_DECAY:G(reverb_decay);break;case TS_P_REVERB_MIX:G(reverb_mix);break;case TS_P_ROOT_NOTE:G(root_midi_note);break;case TS_P_FINE_TUNE:*v=(double)r->fine_tune_cent100/100.0;break;case TS_P_SAMPLE_RATE:G(sample_rate);break;case TS_P_RENDER_FRAMES:*v=(double)r->requested_frames/(double)r->sample_rate;break;case TS_P_FINISHING_MODE:G(finishing_mode);break;case TS_P_TARGET_PEAK:G(target_peak);break;case TS_P_FIXED_GAIN:*v=(double)r->fixed_gain_centidb/100.0;break;default:return false;}return true;}
#undef G
bool ts_parameter_set_number(ts_parameter_id id,ts_recipe*r,double v){const ts_parameter_desc*d=ts_parameter_by_id(id);if(!r||!d||!isfinite(v)||id==TS_P_NAME||v<d->minimum||v>d->maximum)return false;ts_recipe old=*r;switch(id){case TS_P_SOURCE:r->source=(ts_source_type)(int)v;break;case TS_P_SOURCE_SHAPE:r->source_shape=(float)v;break;case TS_P_HARMONIC_MIX:r->harmonic_mix=(float)v;break;case TS_P_NOISE_TYPE:r->noise_type=(ts_noise_type)(int)v;break;case TS_P_NOISE_AMOUNT:r->noise_amount=(float)v;break;case TS_P_SEED:r->seed=(uint64_t)v;break;case TS_P_ATTACK:r->attack_seconds=(float)v;break;case TS_P_DECAY:r->decay_seconds=(float)v;break;case TS_P_SUSTAIN:r->sustain_level=(float)v;break;case TS_P_RELEASE:r->release_seconds=(float)v;break;case TS_P_PITCH_ENV_AMOUNT:r->pitch_env_semitones=(float)v;break;case TS_P_PITCH_ENV_DECAY:r->pitch_env_seconds=(float)v;break;case TS_P_FILTER_ENABLED:r->filter_enabled=v!=0;break;case TS_P_FILTER_MODE:r->filter_mode=(ts_filter_mode)(int)v;break;case TS_P_FILTER_CUTOFF:r->filter_cutoff_hz=(float)v;break;case TS_P_FILTER_RESONANCE:r->filter_resonance=(float)v;break;case TS_P_FILTER_ENV_AMOUNT:r->filter_env_octaves=(float)v;break;case TS_P_SHAPER:r->shaper=(ts_shaper_type)(int)v;break;case TS_P_DRIVE:r->drive=(float)v;break;case TS_P_DELAY_TIME:r->delay_seconds=(float)v;break;case TS_P_DELAY_FEEDBACK:r->delay_feedback=(float)v;break;case TS_P_DELAY_MIX:r->delay_mix=(float)v;break;case TS_P_REVERB_DECAY:r->reverb_decay=(float)v;break;case TS_P_REVERB_MIX:r->reverb_mix=(float)v;break;case TS_P_ROOT_NOTE:r->root_midi_note=(uint8_t)v;break;case TS_P_FINE_TUNE:r->fine_tune_cent100=(int32_t)llround(v*100);break;case TS_P_SAMPLE_RATE:r->sample_rate=(uint32_t)v;break;case TS_P_RENDER_FRAMES:r->requested_frames=(uint32_t)llround(v*(double)r->sample_rate);break;case TS_P_FINISHING_MODE:r->finishing_mode=(ts_finishing_mode)(int)v;break;case TS_P_TARGET_PEAK:r->target_peak=(float)v;break;case TS_P_FIXED_GAIN:r->fixed_gain_centidb=(int32_t)llround(v*100);break;default:return false;}if(!ts_recipe_validate(r)){*r=old;return false;}return true;}
bool ts_parameter_format(ts_parameter_id id,const ts_recipe*r,char*t,size_t c){const ts_parameter_desc*d=ts_parameter_by_id(id);if(!d||!r||!t||!c)return false;if(id==TS_P_NAME){snprintf(t,c,"%s",r->name);return true;}double v;if(!ts_parameter_get_number(id,r,&v))return false;if(d->type==TS_PARAM_ENUM)snprintf(t,c,"%s",d->enum_labels[(size_t)v]);else if(d->type==TS_PARAM_BOOLEAN)snprintf(t,c,"%s",v?"ON":"BYPASS");else if(strcmp(d->unit,"%")==0)snprintf(t,c,"%.0f%%",v*100);else if(d->type==TS_PARAM_INTEGER||d->type==TS_PARAM_SEED||d->type==TS_PARAM_SAMPLE_RATE)snprintf(t,c,"%.0f %s",v,d->unit);else snprintf(t,c,"%.3g %s",v,d->unit);return true;}
bool ts_parameter_parse(ts_parameter_id id,ts_recipe*r,const char*t){if(!r||!t||id==TS_P_NAME)return false;errno=0;char*end;double v=strtod(t,&end);while(*end==' ')end++;if(errno||end==t||*end||!isfinite(v))return false;return ts_parameter_set_number(id,r,v);}
double ts_parameter_to_position(const ts_parameter_desc*d,double v){if(!d||d->maximum<=d->minimum)return 0;if(d->mapping==TS_MAP_LOG&&d->minimum>0)return log(v/d->minimum)/log(d->maximum/d->minimum);return (v-d->minimum)/(d->maximum-d->minimum);}
double ts_parameter_from_position(const ts_parameter_desc*d,double p){if(!d)return 0;if(p<0)p=0;if(p>1)p=1;if(d->mapping==TS_MAP_LOG&&d->minimum>0)return d->minimum*pow(d->maximum/d->minimum,p);return d->minimum+p*(d->maximum-d->minimum);}
bool ts_owned_recipe_copy(ts_owned_recipe*d,const ts_recipe*s){if(!d||!s||!s->name)return false;size_t n=strlen(s->name);char*name=malloc(n+1);if(!name)return false;memcpy(name,s->name,n+1);d->value=*s;d->name=name;d->value.name=name;return true;}
void ts_owned_recipe_destroy(ts_owned_recipe*r){if(r){free(r->name);memset(r,0,sizeof(*r));}}
bool ts_recipe_fields_equal(const ts_recipe*a,const ts_recipe*b){char *ja=NULL,*jb=NULL;size_t na=0,nb=0;ts_io_error e;if(!a||!b||ts_recipe_format(a,&ja,&na,&e)!=TS_IO_OK||ts_recipe_format(b,&jb,&nb,&e)!=TS_IO_OK){free(ja);free(jb);return false;}bool same=na==nb&&memcmp(ja,jb,na)==0;free(ja);free(jb);return same;}
uint64_t ts_recipe_identity(const ts_recipe*r){char*j=NULL;size_t n=0;ts_io_error e;if(ts_recipe_format(r,&j,&n,&e)!=TS_IO_OK)return 0;uint64_t h=UINT64_C(1469598103934665603);for(size_t i=0;i<n;i++){h^=(unsigned char)j[i];h*=UINT64_C(1099511628211);}free(j);return h;}
static void clear(ts_owned_recipe*a,size_t*n){while(*n)ts_owned_recipe_destroy(&a[--*n]);}
void ts_recipe_history_destroy(ts_recipe_history*h){if(h){clear(h->undo,&h->undo_count);clear(h->redo,&h->redo_count);}}
static bool push(ts_owned_recipe*a,size_t*n,const ts_recipe*r){if(*n==TS_EDITOR_HISTORY_CAPACITY){ts_owned_recipe_destroy(&a[0]);memmove(a,a+1,(TS_EDITOR_HISTORY_CAPACITY-1)*sizeof(*a));(*n)--;}return ts_owned_recipe_copy(&a[(*n)++],r);}
bool ts_recipe_history_commit(ts_recipe_history*h,const ts_recipe*r){if(!h||!push(h->undo,&h->undo_count,r))return false;clear(h->redo,&h->redo_count);return true;}
static bool transfer(ts_owned_recipe*from,size_t*fn,ts_owned_recipe*to,size_t*tn,ts_owned_recipe*w){if(!*fn||!push(to,tn,&w->value))return false;ts_owned_recipe_destroy(w);*w=from[--*fn];memset(&from[*fn],0,sizeof(from[*fn]));return true;}
bool ts_recipe_history_undo(ts_recipe_history*h,ts_owned_recipe*w){return h&&w&&transfer(h->undo,&h->undo_count,h->redo,&h->redo_count,w);}
bool ts_recipe_history_redo(ts_recipe_history*h,ts_owned_recipe*w){return h&&w&&transfer(h->redo,&h->redo_count,h->undo,&h->undo_count,w);}
