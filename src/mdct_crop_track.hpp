#pragma once

#include "mdct_common.h"

#define NAME_CROP_TRACK "Crop Track"

#define SPEED_MIN 1
#define SPEED_MAX 100
#define SPEED_DEF 50
#define AVERAGE_MIN 0.001f
#define AVERAGE_MAX 0.3f

#define BRAKE_IMIN 1
#define BRAKE_IMAX 20
#define BRAKE_IDEF 10
#define BRAKE_FMIN 0.2f
#define BRAKE_FMAX 1.0f

#define MAX_SCALE_MIN 2.0f
#define MAX_SCALE_MAX 4.0f
#define MAX_SCALE_DEF 2.2f

#define CT_SLOT_ID              "ct_slot"
#define CT_SLOT_DESC            "Upstream motion detect: slot index"
#define CT_SPEED_ID             "ct_speed"
#define CT_SPEED_DESC           "Positioning speed"
#define CT_BRAKE_ID             "ct_brake"
#define CT_BRAKE_DESC           "Brake factor"
#define CT_MAX_SCALE_ID         "ct_max_scale"
#define CT_MAX_SCALE_DESC       "Maximum scale factor"

//#define S_HORIZ_ACCEL        "horizontal_acceleration"
//#define S_HORIZ_SPEED_MAX    "horizontal_max_speed"
//#define S_DOWN_ACCEL         "down_acceleration"
//#define S_DOWN_SPEED_MAX     "down_max_speed"
//#define S_UP_ACCEL           "up_acceleration"
//#define S_UP_SPEED_MAX       "up_max_speed"
//#define S_ZOOM_IN_ACCEL      "zoom_in_acceleration"
//#define S_ZOOM_IN_SPEED_MAX  "zoom_in_speed_max"
//#define S_ZOOM_OUT_ACCEL     "zoom_out_acceleration"
//#define S_ZOOM_OUT_SPEED_MAX "zoom_out_speed_max"
//#define S_CENTER_DEV         "max_center_deviation"

class CropTrack {
public:
    CropTrack()
        : m_speed(AVERAGE_MIN)
        , m_brake(0.5f)
        /*, accel_horiz(0.1f)
        , speed_horiz(0.5f)
        , accel_down(0.3f)
        , speed_down(1.0f)
        , accel_up(0.1f)
        , speed_up(0.4f)
        , accel_zoom_in(0.1f)
        , speed_zoom_in(0.2f)
        , accel_zoom_out(0.2f)
        , speed_zoom_out(0.5f)
        , center_dev(0.1f)*/
        , m_scale_maxx(MAX_SCALE_DEF)
        /*, speed_x(0.0f)
        , speed_y(0.0f)
        , speed_z(0.0f)
        , brake_x(false)
        , brake_y(false)
        , brake_z(false)*/
    {
        tp_reset(&m_texp);
        tc_reset(&m_tc_avg);
        tc_reset(&m_tc_current);
        tc_reset0(&m_tc_velocity);
        tc_reset0(&m_tc_accel);
    }
    void tick(tp_slot_t *tp);
    //void process_scale(bool slow);
    texture_position_t m_texp;
    texture_center_t m_tc_avg;
    texture_center_t m_tc_current;
    texture_center_t m_tc_velocity;
    texture_center_t m_tc_accel;
    // Setup
    float m_speed;
    float m_brake;
    //float accel_horiz, accel_down, accel_up, accel_zoom_in, accel_zoom_out;
    //float speed_horiz, speed_down, speed_up, speed_zoom_in, speed_zoom_out;
    //float center_dev;
    float m_scale_maxx;
    // Dynamic
    //float speed_x, speed_y, speed_z;
    //bool brake_x, brake_y, brake_z;
};

