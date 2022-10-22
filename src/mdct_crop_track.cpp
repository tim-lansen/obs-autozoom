extern "C" {
#include <graphics/vec2.h>
#include <graphics/image-file.h>
#include <obs-module.h>
#include <obs-source.h>
#include <util/circlebuf.h>
#include <util/dstr.h>
}

#include "mdct_crop_track.hpp"

tp_slot_t tp_slots[10];

static const char *ct_effect_text = "uniform float4x4 ViewProj;\
uniform texture2d image;\
\
uniform float2 mul_val;\
uniform float2 add_val;\
\
sampler_state textureSampler {\
    Filter    = Linear;\
AddressU  = Border;\
AddressV  = Border;\
BorderColor = 00000000;\
};\
\
struct VertData {\
    float4 pos : POSITION;\
    float2 uv  : TEXCOORD0;\
};\
\
VertData VSCrop(VertData v_in)\
{\
    VertData vert_out;\
    vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);\
    vert_out.uv  = v_in.uv * mul_val + add_val;\
    return vert_out;\
}\
\
float4 PSCrop(VertData v_in) : TARGET\
{\
    return image.Sample(textureSampler, v_in.uv);\
}\
\
technique Draw\
{\
    pass\
    {\
        vertex_shader = VSCrop(v_in);\
    pixel_shader  = PSCrop(v_in);\
    }\
}\
";

//#define SCALE_OPTIONS 0.05

struct crop_track_data {
    obs_source_t *context;
    gs_effect_t *effect;
    // User defined
    uint32_t ct_slot;
    CropTrack *ct;

    // Dynamic pass to shader
    gs_eparam_t *param_mul;
    gs_eparam_t *param_add;
    // Output dimensions
    uint32_t width, height;
};

void CropTrack::tick(tp_slot_t *tp_slot) {
    // Current boundaries:
    texture_position_t *tp = &tp_slot->tp;
    texture_center_t tc;

    //tp2tc_limit(tp, &tc, m_scale_max);
    tp2tc(tp, &tc);
    //tc.scale = tc.scale * 1.1;

    // Filter crop-track
    //tc_average(&m_tc_avg, &tc, m_average_factor, &m_tc_avg);
    tc_average(&tc, &m_tc_current, m_speed, &m_tc_avg);
    tc_limit(&m_tc_avg, m_scale_maxx);
    //tc2tp(&m_tc_avg, &m_texp);
    // Calc acceleration
    tc_sub(&m_tc_avg, &m_tc_current, &m_tc_accel);
    tc_scale(&m_tc_accel, 0.05f, &m_tc_accel);
    // Brake
    tc_brake(&m_tc_velocity, &m_tc_accel, m_brake);

    tc_add(&m_tc_velocity, &m_tc_accel, &m_tc_velocity);
    tc_add(&m_tc_current, &m_tc_velocity, &m_tc_current);

    tc_limit(&m_tc_current, m_scale_maxx);
    tc2tp(&m_tc_current, &m_texp);
    
    return;
}


extern "C" {

    static const char *NAME_CT = NAME_CROP_TRACK;

    const char *cte_get_name(void *unused) {
        UNUSED_PARAMETER(unused);
        return NAME_CT;
    }

    obs_properties_t *cte_properties(void *data) {
        obs_properties_t *props = obs_properties_create();
        obs_properties_add_int(props, CT_SLOT_ID, CT_SLOT_DESC, 0, 9, 1);
        obs_properties_add_int(props, CT_SPEED_ID, CT_SPEED_DESC, SPEED_MIN, SPEED_MAX, 1);
        obs_properties_add_int(props, CT_BRAKE_ID, CT_BRAKE_DESC, BRAKE_IMIN, BRAKE_IMAX, 1);
        obs_properties_add_float(props, CT_MAX_SCALE_ID, CT_MAX_SCALE_DESC, MAX_SCALE_MIN, MAX_SCALE_MAX, 0.1);
        UNUSED_PARAMETER(data);
        return props;
    }

    void cte_defaults(obs_data_t *settings) {
        obs_data_set_default_int(settings, CT_SLOT_ID, 0);
        obs_data_set_default_int(settings, CT_SPEED_ID, SPEED_DEF);
        obs_data_set_default_int(settings, CT_BRAKE_ID, BRAKE_IDEF);
        obs_data_set_default_double(settings, CT_MAX_SCALE_ID, MAX_SCALE_DEF);
    }

    void cte_update(void *data, obs_data_t *s) {
        struct crop_track_data *f = (crop_track_data *)data;
        f->ct_slot = (uint32_t)obs_data_get_int(s, CT_SLOT_ID);
        f->ct->m_scale_maxx = 1.0f / (float)obs_data_get_double(s, CT_MAX_SCALE_ID);
        f->ct->m_speed = translate_int_float(obs_data_get_int(s, CT_SPEED_ID), SPEED_MIN, SPEED_MAX, AVERAGE_MIN, AVERAGE_MAX);
        f->ct->m_brake = translate_int_float(BRAKE_IMAX + BRAKE_IMIN - obs_data_get_int(s, CT_BRAKE_ID), BRAKE_IMIN, BRAKE_IMAX, BRAKE_FMIN, BRAKE_FMAX);
        tp_reset(&tp_slots[f->ct_slot].tp);
        f->width = 0;
        f->height = 0;
    }

    void *cte_create(obs_data_t *settings, obs_source_t *context) {
        struct crop_track_data *filter = (crop_track_data *)bzalloc(sizeof(*filter));
        char *errors = NULL;
        filter->ct = NULL;
        filter->context = context;
        obs_enter_graphics();
        filter->effect = gs_effect_create(ct_effect_text, NULL, &errors);
        obs_leave_graphics();

        if (!filter->effect) {
            bfree(filter);
            return NULL;
        }

        filter->ct = new CropTrack();

        filter->param_mul = gs_effect_get_param_by_name(filter->effect, "mul_val");
        filter->param_add = gs_effect_get_param_by_name(filter->effect, "add_val");

        obs_source_update(context, settings);
        return filter;
    }

    void cte_destroy(void *data) {
        struct crop_track_data *filter = (crop_track_data *)data;

        obs_enter_graphics();
        gs_effect_destroy(filter->effect);
        obs_leave_graphics();
        if (filter->ct)
            delete filter->ct;
        bfree(filter);
    }

    void cte_tick(void *data, float seconds) {
        struct crop_track_data *filter = (crop_track_data *)data;
        obs_source_t *target = obs_filter_get_target(filter->context);
        if (target) {
            filter->width = obs_source_get_base_width(target);
            filter->height = obs_source_get_base_height(target);
        }
	filter->ct->tick(&tp_slots[filter->ct_slot]);//, seconds);

        //UNUSED_PARAMETER(seconds);
    }

    void cte_render(void *data, gs_effect_t *effect) {
        struct crop_track_data *filter = (crop_track_data *)data;

        if (!obs_source_process_filter_begin(filter->context, GS_RGBA, OBS_NO_DIRECT_RENDERING))
            return;

        gs_effect_set_vec2(filter->param_mul, &filter->ct->m_texp.multiply);
        gs_effect_set_vec2(filter->param_add, &filter->ct->m_texp.offset);

        obs_source_process_filter_end(filter->context, filter->effect, filter->width, filter->height);

        UNUSED_PARAMETER(effect);
    }
}