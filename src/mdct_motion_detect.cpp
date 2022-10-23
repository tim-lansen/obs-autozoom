#include "mdct_motion_detect.hpp"
extern "C" {
#include <obs-module.h>
#include <obs-source.h>
#include <util/circlebuf.h>
#include <util/dstr.h>
}

/**
* Copy (resized) data block replacing out-of-range values by masking byte
*/
/*void inline copy_replace(uint8_t *psrc, uint32_t size_src, uint8_t* pdst, uint32_t size_dst, uint8_t r_min, uint8_t r_max, uint8_t mask) {
    uint8_t v;
    uint32_t index_src_i = 0, step_i;
    double index_src_f = 0.0;
    double step_f = (double)size_src / (double)size_dst;
    for (; size_dst; size_dst--, pdst++) {
        v = *psrc;
        if (v < r_min || v > r_max) {
            *pdst = mask;
        }
        index_src_f += step_f;
        step_i = (uint32_t)index_src_f - index_src_i;
        if (step_i) {
            psrc += step_i;
            index_src_i += step_i;
        }
    }
}*/
void inline copy_limit(uint8_t *psrc, uint32_t size_src, uint8_t *pdst, uint32_t size_dst, uint8_t limit) {
    uint8_t v;
    uint32_t index_src_i = 0, step_i;
    double index_src_f = 0.0;
    double step_f = (double)size_src / (double)size_dst;
    for (; size_dst; size_dst--, pdst++) {
        v = *psrc;
        if (v > limit) {
            *pdst = limit;
        } else {
            *pdst = v;
        }
        index_src_f += step_f;
        step_i = (uint32_t)index_src_f - index_src_i;
        if (step_i) {
            psrc += step_i;
            index_src_i += step_i;
        }
    }
}

/**
* Extract V plane from frame
*/
void inline extract_V(obs_source_frame *f, void *destination) {
    uint8_t *dst = (uint8_t *)destination;
    uint8_t *src, *s;
    switch (f->format) {
    case VIDEO_FORMAT_I420:
        memcpy(dst, f->data[2], f->linesize[2] * f->height >> 1);
        break;
    case VIDEO_FORMAT_YUY2:
        src = f->data[0] + 1;
        for (uint32_t y = f->height >> 1; y; --y) {
            s = src;
            for (uint32_t x = f->width >> 1; x; --x) {
                *dst = *s;
                dst++;
                s += 4;
            }
            src += f->linesize[0] << 1;
        }
        break;
    default:
        break;
    }
}

#define COLLAPSE_SPEED 33

uint32_t inline umin(uint32_t a, uint32_t b) {
    if(a < b)
        return a;
    return b;
}

void inline swap(uint32_t &a, uint32_t &b) {
    uint32_t t = a;
    a = b;
    b = t;
}

void inline crop2zone(uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2, uint32_t w, uint32_t h, ZoneCrop& zone) {
    uint32_t t;
    if (x2 >= w) {
        x2 = w - 1;
    }
    if (x1 >= w) {
        x1 = 0;
    } else if (x1 > x2) {
        swap(x1, x2);
    }
    
    if (y2 >= h) {
        y2 = h - 1;
    }
    if (y1 >= h) {
        y1 = 0;
    } else if (y1 > y2) {
        swap(y1, y2);
    }
    
    zone.x1 = x1;
    zone.x2 = x2;
    zone.y1 = y1;
    zone.y2 = y2;
    //zone.x1 = x1 < zone.x1 ? x1 : zone.x1 + umin(x1 - zone.x1, COLLAPSE_SPEED);
    //zone.x2 = x2 > zone.x2 ? x2 : zone.x2 - umin(zone.x2 - x2, COLLAPSE_SPEED);
    //zone.y1 = y1 < zone.y1 ? y1 : zone.y1 + umin(y1 - zone.y1, COLLAPSE_SPEED);
    //zone.y2 = y2 > zone.y2 ? y2 : zone.y2 - umin(zone.y2 - y2, COLLAPSE_SPEED);
}

void point(uint32_t x, uint32_t y, obs_source_frame *f) {
    if (x < f->width) {
        uint32_t stride = f->linesize[0];
        uint8_t *plane = f->data[0] + (y * stride);
        if (f->format == VIDEO_FORMAT_I420) {
            plane += x;
        } else {
            plane += x << 1;
        }
        *plane = 255;
    }
}
void line_horz(uint32_t x1, uint32_t x2, uint32_t y, obs_source_frame *f) {
    if (x2 > x1 && x2 < f->width) {
        uint32_t stride = f->linesize[0];
        uint8_t *plane = f->data[0] + (y * stride);
        uint32_t w = x2 - x1, h = f->height;
        switch (f->format) {
        case VIDEO_FORMAT_I420:
            plane += x1;
            memset(plane, 255, w);
            if (y < (h - 1)) {
                plane += stride;
            } else {
                plane -= stride;
            }
            memset(plane, 255, w);
            break;
        case VIDEO_FORMAT_YUY2:
            plane += x1 << 1;
            for (uint32_t i = 0; i < w; ++i) {
                *plane = 255;
                plane += 2;
            }
            if (y < (h - 1)) {
                plane += stride - (w << 1);
            } else {
                plane -= stride + (w << 1);
            }
            for (uint32_t i = 0; i < w; ++i) {
                *plane = 255;
                plane += 2;
            }
            break;
        default:
            break;
        }
    }
}
void line_vert(uint32_t y1, uint32_t y2, uint32_t x, obs_source_frame *f) {
    if (y2 > y1 && y2 < f->height) {
        uint32_t stride = f->linesize[0];
        uint8_t *plane;
        uint32_t w = f->width, h = y2 - y1;
        switch (f->format) {
        case VIDEO_FORMAT_I420:
            plane = f->data[0] + (y1 * stride) + x;
            for (uint32_t i = 0; i < h; ++i) {
                *plane = 255;
                plane += stride;
            }
            if (x < (w - 1)) {
                plane = f->data[0] + (y1 * stride) + x + 1;
            } else {
                plane = f->data[0] + (y1 * stride) + x - 1;
            }
            for (uint32_t i = 0; i < h; ++i) {
                *plane = 255;
                plane += stride;
            }
            break;
        case VIDEO_FORMAT_YUY2:
            plane = f->data[0] + (y1 * stride) + (x << 1);
            for (uint32_t i = 0; i < h; ++i) {
                *plane = 255;
                plane += stride;
            }
            if (x < (w - 1)) {
                plane = f->data[0] + (y1 * stride) + (x << 1) + 2;
            } else {
                plane = f->data[0] + (y1 * stride) + (x << 1) - 2;
            }
            for (uint32_t i = 0; i < h; ++i) {
                *plane = 255;
                plane += stride;
            }
            break;
        default:
            break;
        }
    }
}
void CMotionDetect::draw_zone_y(obs_source_frame *f) {
    line_horz(m_zone.x1, m_zone.x2, m_zone.y1, f);
    line_horz(m_zone.x1, m_zone.x2, m_zone.y2, f);
    line_vert(m_zone.y1, m_zone.y2, m_zone.x1, f);
    line_vert(m_zone.y1, m_zone.y2, m_zone.x2, f);
}



void CMotionDetect::set_mask(const char *path) {
    gs_image_file_free(&m_mask);
    if (path) {
        if (path[0]) {
            gs_image_file_init(&m_mask, path);
        }
        blog(LOG_INFO, "[Motion Detect] set_mask('%s')", path);
    } else {
        blog(LOG_INFO, "[Motion Detect] set_mask(NULL)");
    }
    this->mask_update();
}

/**
* 
*/
void CMotionDetect::mask_plane() {
    if (m_mask_source) {
        // Copy (scale) gray mask to half-res (w/2, h/2) mask plane used for chroma
        double step_f = 2.0 * (double)m_mask_h / (double)m_height;
        uint32_t index_src_i = 0, step_i;
        double index_src_f = 0.5;
        uint32_t hw = m_width >> 1;
        uint8_t *psrc = (uint8_t*)m_mask_source;
        uint8_t* pdst2 = m_mask_chroma;
        for (uint32_t i = (m_height >> 1); i; --i) {
            copy_limit(psrc, m_mask_w, pdst2, hw, 255);
            pdst2 += hw;
            index_src_f += step_f;
            step_i = (uint32_t)index_src_f - index_src_i;
            if (step_i) {
                psrc += m_mask_w * step_i;
                index_src_i += step_i;
            }
        }
    } else {
        memset(m_mask_chroma, 0xFF, m_plane_size);
    }
}
void CMotionDetect::mask_update() {
    if (m_mask_source) {
        bfree(m_mask_source);
        m_mask_source = 0;
    }
    if (m_mask.loaded) {
        m_mask_w = m_mask.cx;
        m_mask_h = m_mask.cy;
        m_mask_source = (uint8_t*)bzalloc(m_mask_w * m_mask_h);
        uint8_t* src = m_mask.texture_data;
        uint8_t* dst = (uint8_t*)m_mask_source;
        // Copy B from BGRA
        for (uint32_t y = 0; y < m_mask_h; ++y) {
            for (uint32_t x = 0; x < m_mask_w; ++x) {
                *dst = *src;
                dst++;
                src += 4;
            }
        }
    }
    mask_plane();
}

/**
* Check frame and current format. Reset motion zone and re-allocate buffer if new format differs.
*/
bool CMotionDetect::check_init(obs_source_frame* f) {
    if (f && f->data != 0 && f->data[0]) {
        if (f->format != m_format) {
            m_format = f->format;
            m_width = f->width;
            m_height = f->height;
            m_zone.x1 = 0;
            m_zone.x2 = m_width;
            m_zone.y1 = 0;
            m_zone.y2 = m_height;
            if (m_plane_root) {
                bfree(m_plane_root);
                m_plane_root = 0;
            }
            blog(LOG_INFO, "[Motion Detect] check_init: format=%d width=%d height=%d lsz0=%d lsz1=%d lsz2=%d",
                f->format, f->width, f->height, f->linesize[0], f->linesize[1], f->linesize[2]);
        }
        if (!m_plane_root) {
            // Allocate memory for V planes:
            //      root plane (current frame)
            //      blur plane (current frame blurred)
            //      diff plane (blur plane - plane from m_planes)
            //      diff plane low boundary values
            //      diff plane high boundary values
            //      mask plane (space for mask U/V plane data)
            //      mask diff  (space for mask U/V diff data)
            //      planes data (circle buffer for planes)
            m_plane_size = (m_width * m_height) >> 2;
            m_plane_root       = (uint8_t*)bzalloc(m_plane_size * (7 + DIFF_DELAY_MAX));
            m_plane_blur       = m_plane_root + m_plane_size;
            m_plane_diff       = m_plane_root + m_plane_size * 2;
            //m_plane_diff_lo    = m_plane_root + m_plane_size * 3;
            //m_plane_diff_hi    = m_plane_root + m_plane_size * 4;
            m_mask_chroma      = m_plane_root + m_plane_size * 5;
            m_mask_chroma_dynamic = m_plane_root + m_plane_size * 6;
            for (uint32_t i = 0; i < DIFF_DELAY_MAX; ++i) {
                m_planes[i] = m_plane_root + m_plane_size * (7 + i);
            }
            mask_plane();
        }
    }
    return m_plane_root != 0;
}

void CMotionDetect::dynamic() {
    // Saturate mask
    avx2_offset_uint8(m_mask_chroma_dynamic, m_mask_chroma_dynamic, m_plane_size, m_diff_extinction);
    // Cut out motion zone
    /*if (m_motion_detected) {
        uint32_t hw = m_width >> 1;
        uint8_t *p = m_mask_chroma_dynamic + (m_zone.x1 >> 1) + hw * (m_zone.y1 >> 1);
        uint32_t w = (m_zone.x2 - m_zone.x1) >> 1;
        uint32_t h = (m_zone.y2 - m_zone.y1) >> 1;
        for (; h; --h, p += hw) {
            memset(p, 0, w);
        }
    }*/
}

obs_source_frame *CMotionDetect::feed_frame(obs_source_frame *f) {
    // 1. Extract plane
    // 2. Blur plane
    // 3. Calculate difference to previous plane (scalar)
    // 4. Calculate difference to plane(-diff_delay) (plane)
    if (check_init(f)) {
        extract_V(f, m_plane_root);
        double difference = 0.0;
        // 
        /*SimdGaussianBlur3x3(
            m_plane_root, m_width >> 1, m_width >> 1,
            m_height >> 1,
            1,
            m_plane_blur, m_width >> 1
        );*/
        memcpy(m_plane_blur, m_plane_root, m_plane_size);
        uint32_t half_w = m_width >> 1;
        uint32_t half_h = m_height >> 1;
        /*bool motion = plane_diff_mask_detect(
            m_plane_blur,
            m_planes[m_diff_index % DIFF_DELAY_MAX],
            m_mask_chroma_diff,
            half_w, half_w, half_h, m_ssd_threshold,
            &difference
        );
        memset(m_plane_diff, 0, m_plane_size);*/

        uint8_t *line_p1 = m_plane_blur;
        uint8_t *line_p2 = m_planes[(DIFF_DELAY_MAX + m_diff_index - m_diff_delay) % DIFF_DELAY_MAX];
        uint8_t *line_mask = m_mask_chroma;
        uint8_t *line_dynamic = m_mask_chroma_dynamic;
        uint32_t x1 = half_w, x2 = 0, y1 = half_h, y2 = 0;

        for (uint32_t y = 0;
             y < half_h;
             ++y, line_p1 += half_w, line_p2 += half_w, line_mask += half_w, line_dynamic += half_w) {
            uint32_t xx1 = half_w, xx2 = 0;
            bool gotcha = avx2_diff_mask_detect_uint8(line_p1, line_p2, line_mask, half_w, xx1, xx2);
            if (gotcha) {
                point(xx1 << 1, y << 1, f);
                point(xx2 << 1, y << 1, f);
                memset(line_dynamic + xx1, 0, xx2 - xx1);
                if (x1 > xx1) x1 = xx1;
                if (x2 < xx2) x2 = xx2;
                if (y1 > y) {
                    y1 = y;
                }
                y2 = y;
            }
        }

        if (y2 > y1)
            crop2zone(x1 << 1, x2 << 1, y1 << 1, y2 << 1, m_width, m_height, m_zone);

        m_motion_detected = (y2 > y1);

        if (m_motion_detected) {
            tp_slot_t *t = &tp_slots[m_slot];
            if (m_width && m_height) {
                float w = (float)m_width;
                float h = (float)m_height;
                t->tp.multiply.x = (float)(m_zone.x2 - m_zone.x1) / w;
                t->tp.multiply.y = (float)(m_zone.y2 - m_zone.y1) / h;
                t->tp.offset.x = (float)m_zone.x1 / w;
                t->tp.offset.y = (float)m_zone.y1 / h;
            }
            draw_zone_y(f);
        }
        this->dynamic();

#if 0
        if (ssd > 0.000000001) {
            // Update background
            if (!motion) {
                m_similar_count++;
                if (m_similar_count == SIMILAR_COUNT_START) {
                    // Initialize background capture U/V plane
                    memcpy(m_buffers[10], m_buffers[0], hwxhh);
                    memcpy(m_buffers[11], m_buffers[0], hwxhh);
                } else if (m_similar_count > SIMILAR_COUNT_START) {
                    if (m_similar_count < SIMILAR_COUNT_END) {
                        // Update background ranges U/V
                        SimdBackgroundGrowRangeFast(
                            (const uint8_t*)m_buffers[0], hw,
                            hw, hh,
                            (uint8_t*)m_buffers[10], hw,
                            (uint8_t*)m_buffers[11], hw
                        );
                    } else if (m_similar_count == SIMILAR_COUNT_END) {
                        // Use background U/V
                        memcpy(m_buffers[7], m_buffers[10], hwxhh);
                        memcpy(m_buffers[8], m_buffers[11], hwxhh);
                    }
                }
            } else {
                m_similar_count = 0;
            }
        }

#endif

        // Blend with previous plane instead of copy
        uint32_t new_index = (m_diff_index + 1) % DIFF_DELAY_MAX;
        avx2_blend_uint8(
            m_plane_blur,
            m_planes[m_diff_index],
            m_planes[new_index],
            m_plane_size
        );
        //memcpy(m_planes[m_diff_index], m_plane_blur, m_plane_size);
        m_diff_index = new_index;

        // Delay:
        // 1. Push frame to circlebuffer
        circlebuf_push_back(&m_frames, &f, sizeof(void *));
        // 2. Pull delayed frame or NULL
        if (m_frames.size < m_show_delay) {
            f = NULL;
        } else {
            circlebuf_pop_front(&m_frames, &f, sizeof(void *));
            uint8_t *psrc = m_mask_chroma_dynamic;
            uint8_t *pdst = f->data[0];
            uint8_t *pd;
            uint32_t w, h = half_h;
            switch (f->format) {
            case VIDEO_FORMAT_I420:
                for (; h; --h, psrc += half_w, pdst += f->linesize[0]) {
                    memcpy(pdst, psrc, half_w);
                }
                break;
            case VIDEO_FORMAT_YUY2:
                /*for (; h; --h, pdst += f->linesize[0]) {
                    pd = pdst;
                    for (w = 0; w < half_w; ++w, pd += 2, psrc++) {
                        *pd = *psrc;
                    }
                }*/
                break;
            default:
                break;
            }
        }
    }
    return f;
}

// Shared TexturePosition slots

struct motion_detect_filter_data {
    obs_source_t *context;

    //uint32_t slot;
    //uint32_t delay;
    //char *mask_path;
    //gs_image_file_t mask;

    CMotionDetect *md;
};


extern "C" {

    static const char *NAME_MD = NAME_MOTION_DETECT;

    const char *mdf_get_name(void *unused) {
        UNUSED_PARAMETER(unused);
        return NAME_MD;
    }

    obs_properties_t *mdf_properties(void *data) {
        blog(LOG_INFO, "[MD] mdf_properties()");
        dstr filter_str = {0};
        dstr_copy(&filter_str, TEXT_PATH_IMAGES);
        dstr_cat(&filter_str, IMAGE_FILTER_EXTENSIONS ";;");
        dstr_cat(&filter_str, TEXT_PATH_ALL_FILES);
        dstr_cat(&filter_str, " (*.*)");

        obs_properties_t *props = obs_properties_create();
        obs_properties_add_int(props, MD_SLOT_ID, MD_SLOT_DESC, 0, 9, 1);
        obs_properties_add_int(props, MD_SHOW_DELAY_ID, MD_SHOW_DELAY_DESC, 1, SHOW_DELAY_MAX, 1);
        //obs_properties_add_float(props, MD_CAPTURE_THRESHOLD_ID, MD_CAPTURE_THRESHOLD_DESC, 0.1, 127.9, 0.1);
        //obs_properties_add_int(props, MD_CAPTURE_COUNT_ID, MD_CAPTURE_COUNT_DESC, 30, 300, 1);
        obs_properties_add_int(props, MD_DIFF_DELAY_ID, MD_DIFF_DELAY_DESC, 1, DIFF_DELAY_MAX, 1);
        obs_properties_add_int(props, MD_MOTION_THRESHOLD_ID, MD_MOTION_THRESHOLD_DESC, 1, 127, 1);
        obs_properties_add_int(props, MD_DIFF_EXT_ID, MD_DIFF_EXT_DESC, 0, 7, 1);

        obs_properties_add_path(props, MD_MASK_PATH_ID, MD_MASK_PATH_DESC, OBS_PATH_FILE, filter_str.array, NULL);

        UNUSED_PARAMETER(data);
        return props;
    }

    void mdf_defaults(obs_data_t *settings) {
        blog(LOG_INFO, "[MD] mdf_defaults()");
        obs_data_set_default_int(settings, MD_SLOT_ID, 0);
        obs_data_set_default_int(settings, MD_SHOW_DELAY_ID, SHOW_DELAY_DEF);

        //obs_data_set_default_double(settings, MD_CAPTURE_THRESHOLD_ID, 5.0);
        //obs_data_set_default_int(settings, MD_CAPTURE_COUNT_ID, 100);
        obs_data_set_default_int(settings, MD_DIFF_DELAY_ID, DIFF_DELAY_DEF);
        obs_data_set_default_int(settings, MD_MOTION_THRESHOLD_ID, 12);
        obs_data_set_default_int(settings, MD_DIFF_EXT_ID, 1);
    }

    void *mdf_create(obs_data_t *settings, obs_source_t *context) {
        blog(LOG_INFO, "[MD] mdf_create()");
        motion_detect_filter_data *f = (motion_detect_filter_data *)bzalloc(sizeof(motion_detect_filter_data));
        f->context = context;
        f->md = new CMotionDetect();
        obs_source_update(context, settings);
        return f;
    }

    void mdf_update(void *data, obs_data_t *s) {
        blog(LOG_INFO, "[MD] mdf_update()");
        motion_detect_filter_data *f = (motion_detect_filter_data *)data;

        f->md->set_slot((uint32_t)obs_data_get_int(s, MD_SLOT_ID));
        f->md->set_show_delay((uint32_t)obs_data_get_int(s, MD_SHOW_DELAY_ID));
        //f->md->set_capture_threshold(obs_data_get_double(s, MD_CAPTURE_THRESHOLD_ID));
        f->md->set_diff_delay(obs_data_get_int(s, MD_DIFF_DELAY_ID));
        f->md->set_motion_threshold(obs_data_get_int(s, MD_MOTION_THRESHOLD_ID));
        f->md->set_extinction(obs_data_get_int(s, MD_DIFF_EXT_ID));

        f->md->set_mask(obs_data_get_string(s, MD_MASK_PATH_ID));
    }

    void mdf_destroy(void *data) {
        blog(LOG_INFO, "[MD] mdf_destroy()");
        motion_detect_filter_data *f = (motion_detect_filter_data *)data;
        if (f->md) {
            f->md->destroy_frames();
            delete f->md;
            f->md = 0;
        }
        bfree(f);
    }

    obs_source_frame *mdf_video(void *data, obs_source_frame *frame) {
        motion_detect_filter_data *filter = (motion_detect_filter_data *)data;
        //obs_source_t *parent = obs_filter_get_parent(filter->context);

        //circlebuf_push_back(&filter->video_frames, &frame, sizeof(struct obs_source_frame *));

        frame = filter->md->feed_frame(frame);
        return frame;
    }

    void mdf_remove(void *data, obs_source_t *parent) {
        blog(LOG_INFO, "[MD] mdf_remove()");
        motion_detect_filter_data *f = (motion_detect_filter_data *)data;
        f->md->release_frames(parent);
        delete f->md;
        f->md = 0;
    }

}



