#pragma once

#include "mdct_common.h"
#include "simd_ops.h"

#define NAME_MOTION_DETECT "Motion Detect";

#define MD_SLOT_ID                  "md_slot"
#define MD_SLOT_DESC                "Downstream crop-track: slot index"
#define MD_SHOW_DELAY_ID            "md_show_delay"
#define MD_SHOW_DELAY_DESC          "Playback: delay (frames)"
#define MD_MASK_PATH_ID             "mask_path"
#define MD_MASK_PATH_DESC           "Detect/capture: mask"
//#define MD_CAPTURE_THRESHOLD_ID     "cap_diff_thr"
//#define MD_CAPTURE_THRESHOLD_DESC   "Capture background: threshold"
//#define MD_CAPTURE_COUNT_ID         "cap_count"
//#define MD_CAPTURE_COUNT_DESC       "Capture background: count"
#define MD_DIFF_DELAY_ID            "md_diff_delay"
#define MD_DIFF_DELAY_DESC          "Detect motion: interval (frames)"
#define MD_DIFF_EXT_ID              "md_diff_ext"
#define MD_DIFF_EXT_DESC            "Detect motion: extinction speed"
#define MD_MOTION_THRESHOLD_ID      "mot_diff_thr"
#define MD_MOTION_THRESHOLD_DESC    "Detect motion: threshold"


#define TEXT_PATH_IMAGES            "BrowsePath.Images"
#define TEXT_PATH_ALL_FILES         "BrowsePath.AllFiles"
//#define IMAGE_FILTER_EXTENSIONS " (*.bmp *.jpg *.jpeg *.tga *.gif *.png)"
#define IMAGE_FILTER_EXTENSIONS " (*.bmp *.png)"

#define DIFF_DELAY_MAX 30
#define DIFF_DELAY_DEF 3

#define SHOW_DELAY_MAX 150
#define SHOW_DELAY_DEF 3

#define SIMILAR_COUNT_START 100
#define SIMILAR_COUNT_END 125

class CMotionDetect {
public:
    CMotionDetect()
        : m_slot(0)
        , m_show_delay(SHOW_DELAY_DEF)
        , m_diff_delay(DIFF_DELAY_DEF)
        , m_diff_extinction(2)
        , m_diff_index(0)
        , m_plane_root(0)
        , m_plane_size(0)
        , m_mask_source(0)
        , m_mask_chroma(0)
        , m_mask_chroma_dynamic(0)
        , m_similar_count(0)
        //, m_ssd_threshold(5.0)
        , m_motion_detected(false)
        , m_counter(0)
        , m_mask_w(0), m_mask_h(0)
        , m_width(0)
        , m_height(0)
        , m_format(VIDEO_FORMAT_NONE)
        , m_motion_threshold(250)
    {
        m_zone.x1 = 0;
        m_zone.x2 = 0;
        m_zone.y1 = 0;
        m_zone.y2 = 0;
        circlebuf_init(&m_frames);
        memset(&m_mask, 0, sizeof(m_mask));
    }
    ~CMotionDetect(){
        if (m_plane_root) {
            bfree(m_plane_root);
        }
        if (m_mask_source) {
            bfree(m_mask_source);
        }
        gs_image_file_free(&m_mask);
    }

    void set_slot(uint32_t slot) {
        m_slot = slot;
        blog(LOG_INFO, "[Motion Detect] set_slot(%d)", slot);
    }
    void set_show_delay(uint32_t delay) {
        m_show_delay = delay;
        blog(LOG_INFO, "[Motion Detect] set_show_delay(%d)", delay);
        while (m_frames.size > delay * sizeof(void *)) {
            obs_source_frame *frame;
            circlebuf_pop_front(&m_frames, &frame, sizeof(void *));
            obs_source_release_frame(NULL, frame);
        }
    }
    void set_diff_delay(uint32_t delay) {
        m_diff_delay = delay;
        blog(LOG_INFO, "[Motion Detect] set_diff_delay(%d)", delay);
    }
    void set_motion_threshold(uint8_t thr) {
        m_motion_threshold = thr;
        simd_set_m256i_threshold(thr);
    }
    void set_extinction(uint8_t ext) {
        m_diff_extinction = 1 << ext;
    }
    //void set_capture_threshold(double thr) {
    //    m_ssd_threshold = thr;
    //}
    
    void set_mask(const char *path);
    void release_frames(obs_source_t *parent) {
        while (m_frames.size) {
            obs_source_frame *frame;
            circlebuf_pop_front(&m_frames, &frame, sizeof(void *));
            obs_source_release_frame(parent, frame);
        }
    }
    void destroy_frames() {
        circlebuf_free(&m_frames);
    }

    // Setup
    uint32_t m_slot;
    uint32_t m_show_delay;      // Frame presentation delay
    gs_image_file_t m_mask;
    uint32_t m_diff_delay;      // Difference analyze delay: current frame N is being compared to N-delay
    uint8_t m_diff_extinction;

    // Dynamic
    uint32_t m_diff_index;      // New plane index in m_planes buffer

    void mask_plane();
    void mask_update();
    bool check_init(obs_source_frame* f);
    void draw_zone_y(obs_source_frame* f);
    void dynamic();
    obs_source_frame *feed_frame(obs_source_frame *f);

    struct circlebuf m_frames;
    uint8_t *m_planes[DIFF_DELAY_MAX];

    video_format m_format;
    uint8_t m_motion_threshold;
    size_t m_plane_size;
    uint8_t *m_plane_root;
    uint8_t *m_plane_blur;
    uint8_t *m_plane_diff;
    //uint8_t *m_plane_diff_lo;
    //uint8_t *m_plane_diff_hi;
    uint8_t *m_mask_source;
    uint8_t *m_mask_chroma;
    uint8_t *m_mask_chroma_dynamic;
    uint32_t m_similar_count;
    
    bool m_motion_detected;
    uint32_t m_counter;
    uint32_t m_mask_w, m_mask_h;
    uint32_t m_width, m_height;
    ZoneCrop m_zone;
};


