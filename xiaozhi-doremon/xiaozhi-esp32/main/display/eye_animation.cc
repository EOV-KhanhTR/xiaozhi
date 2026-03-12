#include "eye_animation.h"
#include "gif/lvgl_gif.h"
#include <esp_log.h>
#include <esp_random.h>
#include <cmath>
#include <cstring>

extern const lv_image_dsc_t doraemon_shizuka_gif;
extern const lv_image_dsc_t doraemon_cute_gif;
extern const lv_image_dsc_t feliz_shizuka_gif;

#define TAG "EyeAnimation"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
// Emotion string mapping (xiaozhi strings → original emotions)
// ============================================================
EyeEmotion EyeEmotion_FromString(const char* emotion) {
    if (!emotion) return EyeEmotion::Idle;
    if (strcmp(emotion, "neutral") == 0 || strcmp(emotion, "relaxed") == 0)
        return EyeEmotion::Idle;
    if (strcmp(emotion, "happy") == 0 || strcmp(emotion, "funny") == 0)
        return EyeEmotion::Happy1;
    if (strcmp(emotion, "laughing") == 0 || strcmp(emotion, "confident") == 0)
        return EyeEmotion::Happy2;
    if (strcmp(emotion, "sad") == 0)
        return EyeEmotion::Sad1;
    if (strcmp(emotion, "angry") == 0)
        return EyeEmotion::Angry1;
    if (strcmp(emotion, "loving") == 0 || strcmp(emotion, "kissy") == 0)
        return EyeEmotion::Love;
    if (strcmp(emotion, "surprised") == 0 || strcmp(emotion, "shocked") == 0)
        return EyeEmotion::Excited;
    if (strcmp(emotion, "sleepy") == 0)
        return EyeEmotion::Tired;
    if (strcmp(emotion, "winking") == 0 || strcmp(emotion, "silly") == 0)
        return EyeEmotion::Curious;
    if (strcmp(emotion, "cool") == 0)
        return EyeEmotion::Idle;
    if (strcmp(emotion, "thinking") == 0)
        return EyeEmotion::Curious1;
    if (strcmp(emotion, "crying") == 0)
        return EyeEmotion::Sad2;
    if (strcmp(emotion, "delicious") == 0)
        return EyeEmotion::Happy2;
    if (strcmp(emotion, "confused") == 0)
        return EyeEmotion::Curious2;
    if (strcmp(emotion, "embarrassed") == 0)
        return EyeEmotion::Worried1;
    if (strcmp(emotion, "cyclop") == 0)
        return EyeEmotion::Cyclop;
    if (strcmp(emotion, "drunk") == 0)
        return EyeEmotion::Drunk;
    return EyeEmotion::Idle;
}

// ============================================================
// Helpers
// ============================================================
static int randi(int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + (int)(esp_random() % (hi - lo + 1));
}

// ============================================================
// Constructor / Destructor
// ============================================================
EyeAnimation::EyeAnimation() {}

EyeAnimation::~EyeAnimation() {
    if (anim_timer_) {
        lv_timer_delete(anim_timer_);
        anim_timer_ = nullptr;
    }
}

// ============================================================
// Init
// ============================================================
void EyeAnimation::Init(lv_obj_t* parent, int sw, int sh) {
    screen_w_ = sw;
    screen_h_ = sh;

    CreateEyeObjects(parent);

    ScheduleNextBlink();
    next_look_ms_ = lv_tick_get() + randi(2000, 4000);

    // Initialize lid positions to hidden
    left_lid_top_y_ = -(float)LID_H;
    right_lid_top_y_ = -(float)LID_H;
    left_lid_bottom_y_ = (float)EYE_SIZE;
    right_lid_bottom_y_ = (float)EYE_SIZE;
    left_lid_top_target_y_ = -(float)LID_H;
    right_lid_top_target_y_ = -(float)LID_H;
    left_lid_bottom_target_y_ = (float)EYE_SIZE;
    right_lid_bottom_target_y_ = (float)EYE_SIZE;

    // Animation timer (~30 FPS)
    anim_timer_ = lv_timer_create(TimerCallback, 33, this);

    ESP_LOGI(TAG, "Eye animation initialized (%dx%d) matching original", sw, sh);
}

// ============================================================
// Create LVGL objects (Doraemon face)
// ============================================================
void EyeAnimation::CreateEyeObjects(lv_obj_t* parent) {
    // Container: full screen, black background
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, screen_w_, screen_h_);
    lv_obj_align(container_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_CLICKABLE);

    // Colors
    lv_color_t blue = lv_color_make(0, 147, 214);
    lv_color_t red = lv_color_make(230, 0, 0);
    lv_color_t yellow = lv_color_make(255, 220, 0);

    // Shizuka Colors (Converted from RGB565 to 24-bit for lv_color_hex)

    // Face circle
    face_ = lv_obj_create(container_);
    lv_obj_set_size(face_, 224, 224);
    lv_obj_set_style_radius(face_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(face_, blue, 0);
    lv_obj_set_style_bg_opa(face_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(face_, 2, 0);
    lv_obj_set_style_border_color(face_, lv_color_black(), 0);

    // Muzzle (white ellipse - Reverted to original bounding size)
    muzzle_ = lv_obj_create(container_);
    lv_obj_set_size(muzzle_, 176, 144);
    lv_obj_set_style_radius(muzzle_, 72, 0);
    lv_obj_set_style_bg_color(muzzle_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(muzzle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(muzzle_, 0, 0);

    // Eyes (white ovals with black border)
    left_eye_ = lv_obj_create(container_);
    lv_obj_set_size(left_eye_, 44, 56);
    lv_obj_set_style_radius(left_eye_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(left_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(left_eye_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(left_eye_, 2, 0);
    lv_obj_set_style_border_color(left_eye_, lv_color_black(), 0);

    right_eye_ = lv_obj_create(container_);
    lv_obj_set_size(right_eye_, 44, 56);
    lv_obj_set_style_radius(right_eye_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(right_eye_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(right_eye_, 2, 0);
    lv_obj_set_style_border_color(right_eye_, lv_color_black(), 0);

    // Pupils (Large, black)
    pupil_left_ = lv_obj_create(container_);
    lv_obj_set_size(pupil_left_, 28, 28);
    lv_obj_set_style_radius(pupil_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil_left_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pupil_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_left_, 0, 0);

    pupil_right_ = lv_obj_create(container_);
    lv_obj_set_size(pupil_right_, 28, 28);
    lv_obj_set_style_radius(pupil_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil_right_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pupil_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_right_, 0, 0);

    // Pupil Highlights (Tiny, white "shiny" effect)
    pupil_hi_left_ = lv_obj_create(container_);
    lv_obj_set_size(pupil_hi_left_, 8, 8);
    lv_obj_set_style_radius(pupil_hi_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil_hi_left_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(pupil_hi_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_hi_left_, 0, 0);

    pupil_hi_right_ = lv_obj_create(container_);
    lv_obj_set_size(pupil_hi_right_, 8, 8);
    lv_obj_set_style_radius(pupil_hi_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil_hi_right_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(pupil_hi_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil_hi_right_, 0, 0);

    // Nose
    nose_ = lv_obj_create(container_);
    lv_obj_set_size(nose_, 30, 30); // Enlarged
    lv_obj_set_style_radius(nose_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(nose_, red, 0);
    lv_obj_set_style_bg_opa(nose_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nose_, 2, 0);
    lv_obj_set_style_border_color(nose_, lv_color_black(), 0);

    // Philtrum (vertical line from nose to mouth)
    philtrum_line_ = lv_line_create(container_);
    lv_obj_set_style_line_color(philtrum_line_, lv_color_black(), 0);
    lv_obj_set_style_line_width(philtrum_line_, 2, 0);

    // Whiskers (3 per side) - All constructed as lines, bottom ones curved later
    for (int i = 0; i < 6; ++i) {
        whiskers_[i] = lv_line_create(container_);
        lv_obj_set_style_line_color(whiskers_[i], lv_color_black(), 0);
        lv_obj_set_style_line_width(whiskers_[i], 2, 0);
        lv_obj_set_style_line_rounded(whiskers_[i], true, 0);
    }

    // --- PERMANENT WIDE OPEN MOUTH ---
    // Mouth Container (flat top clipping mask)
    mouth_ = lv_obj_create(container_);
    lv_obj_remove_style_all(mouth_);
    lv_obj_set_size(mouth_, 100, 50); // Narrower width (130 -> 100)
    lv_obj_set_style_clip_corner(mouth_, true, 0); 
    lv_obj_remove_flag(mouth_, LV_OBJ_FLAG_SCROLLABLE);
    // Draw top line to connect the sides of the U curve
    lv_obj_set_style_border_width(mouth_, 2, 0);
    lv_obj_set_style_border_side(mouth_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(mouth_, lv_color_black(), 0);

    // Mouth Background (large circle to create the curved bottom)
    lv_obj_t* mouth_bg = lv_obj_create(mouth_);
    lv_obj_remove_style_all(mouth_bg);
    lv_obj_set_size(mouth_bg, 100, 100);
    lv_obj_set_pos(mouth_bg, 0, -50); // Shifted up to keep the bottom edge exactly flush (100-50 = 50)
    lv_obj_set_style_radius(mouth_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(mouth_bg, lv_color_make(230, 0, 0), 0);
    lv_obj_set_style_bg_opa(mouth_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_bg, 2, 0);
    lv_obj_set_style_border_color(mouth_bg, lv_color_black(), 0);

    // Tongue Container (clipping mask for perfect semi-circle)
    lv_obj_t* tongue_cont = lv_obj_create(mouth_);
    lv_obj_remove_style_all(tongue_cont);
    lv_obj_set_size(tongue_cont, 56, 28); // 56 wide, 28 high (semi-circle)
    // Center horizontally: (100-56)/2 = 22. Shift up slightly -> Y=16.
    lv_obj_set_pos(tongue_cont, 22, 16); 
    lv_obj_set_style_clip_corner(tongue_cont, true, 0); 
    lv_obj_remove_flag(tongue_cont, LV_OBJ_FLAG_SCROLLABLE);

    // Tongue Background (circle shifted up)
    lv_color_t tongue_color = lv_color_make(255, 182, 160); // Lighter pinkish-orange
    tongue_ = lv_obj_create(tongue_cont);
    lv_obj_remove_style_all(tongue_);
    lv_obj_set_size(tongue_, 56, 56); 
    lv_obj_set_pos(tongue_, 0, -28); // Shift up so exactly only the bottom half is visible
    lv_obj_set_style_radius(tongue_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tongue_, tongue_color, 0);
    lv_obj_set_style_bg_opa(tongue_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tongue_, 0, 0); 

    // --- CHARACTER COMPONENTS (Ordered by Z-Order/Layer) ---


    // Doraemon Shizuka GIF Image
    doraemon_shizuka_gif_obj_ = lv_img_create(container_);
    lv_obj_add_flag(doraemon_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(doraemon_shizuka_gif_obj_, screen_w_, screen_h_);
    lv_obj_align(doraemon_shizuka_gif_obj_, LV_ALIGN_CENTER, 0, 0);

    // Feliz Shizuka GIF container
    feliz_shizuka_gif_obj_ = lv_img_create(container_);
    lv_obj_add_flag(feliz_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(feliz_shizuka_gif_obj_, screen_w_, screen_h_);
    lv_obj_align(feliz_shizuka_gif_obj_, LV_ALIGN_CENTER, 0, 0);


    // Doraemon collar + bell
    collar_ = lv_obj_create(container_);
    lv_obj_set_size(collar_, 138, 6); 
    lv_obj_set_style_bg_color(collar_, red, 0);
    lv_obj_set_style_bg_opa(collar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(collar_, 0, 0);

    bell_ = lv_obj_create(container_);
    lv_obj_set_size(bell_, 18, 18);
    lv_obj_set_style_radius(bell_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bell_, yellow, 0);
    lv_obj_set_style_bg_opa(bell_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bell_, 2, 0);
    lv_obj_set_style_border_color(bell_, lv_color_black(), 0);

    bell_slot_ = lv_obj_create(container_);
    lv_obj_set_size(bell_slot_, 8, 3);
    lv_obj_set_style_radius(bell_slot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bell_slot_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bell_slot_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bell_slot_, 0, 0);

    // Layer 3: Hair Base + Face Ellipse



    // Tears
    lv_color_t light_blue = lv_color_make(80, 200, 255);
    tear_left_ = lv_obj_create(container_);
    lv_obj_set_size(tear_left_, 12, 16);
    lv_obj_set_style_radius(tear_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tear_left_, light_blue, 0);
    lv_obj_set_style_bg_opa(tear_left_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tear_left_, 0, 0);
    lv_obj_add_flag(tear_left_, LV_OBJ_FLAG_HIDDEN);

    tear_right_ = lv_obj_create(container_);
    lv_obj_set_size(tear_right_, 12, 16);
    lv_obj_set_style_radius(tear_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(tear_right_, light_blue, 0);
    lv_obj_set_style_bg_opa(tear_right_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tear_right_, 0, 0);
    lv_obj_add_flag(tear_right_, LV_OBJ_FLAG_HIDDEN);

    ApplyPositions();
}

// ============================================================
// Public API
// ============================================================
void EyeAnimation::SetEmotion(EyeEmotion emotion) {
    // Excited normalized to Idle (matching original)
    if (emotion == EyeEmotion::Excited) {
        emotion = EyeEmotion::Idle;
    }

    current_emotion_ = emotion;

    if (emotion != EyeEmotion::Idle) {
        uint32_t now = lv_tick_get();
        emotion_start_ms_ = now;
        emotion_end_ms_ = now + EMOTION_DURATION_MS;
    } else {
        emotion_end_ms_ = 0;
    }

    // Curious triggers a wink blink
    if (emotion == EyeEmotion::Curious) {
        blink_active_ = true;
        blink_start_ms_ = lv_tick_get();
        blink_left_ = true;
        blink_right_ = false; // only left eye blinks (wink)
    }

    // Update color target
    uint8_t r, g, b;
    GetTargetColor(emotion, r, g, b);
    tr_ = (float)r;
    tg_ = (float)g;
    tb_ = (float)b;

    // Update lid targets
    SetLidTargets();

    ESP_LOGI(TAG, "Emotion set to %d", (int)emotion);
}

void EyeAnimation::SetVisible(bool visible) {
    if (container_) {
        if (visible) lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================================
// Get target color (matching original EyeColor_getTargetForEmotion)
// ============================================================
void EyeAnimation::GetTargetColor(EyeEmotion emo, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = 255; g = 255; b = 255; // white default
    switch (emo) {
        case EyeEmotion::Angry1: r = 255; g = 120; b = 120; break;
        case EyeEmotion::Angry2: r = 255; g = 70;  b = 70;  break;
        case EyeEmotion::Angry3: r = 255; g = 30;  b = 30;  break;
        case EyeEmotion::Happy1: r = 255; g = 200; b = 80;  break;
        case EyeEmotion::Happy2: r = 255; g = 230; b = 40;  break;
        case EyeEmotion::Sad1:
        case EyeEmotion::Sad2:   r = 80;  g = 140; b = 255; break;
        case EyeEmotion::Cyclop: r = 255; g = 255; b = 255; break;
        case EyeEmotion::Drunk:  r = 255; g = 160; b = 160; break; // Pinkish red
        default: break;
    }
}

// ============================================================
// Set lid targets based on current emotion
// Angles in LVGL 0.1° units (e.g., 200 = 20°)
// Positive rotation = counter-clockwise in LVGL
// For left eye angry: positive → inner (right) side dips more
// For right eye angry: negative → inner (left) side dips more
// ============================================================
void EyeAnimation::SetLidTargets() {
    // Default: all lids hidden
    left_lid_top_target_y_ = -(float)LID_H;
    right_lid_top_target_y_ = -(float)LID_H;
    left_lid_bottom_target_y_ = (float)EYE_SIZE;
    right_lid_bottom_target_y_ = (float)EYE_SIZE;
    left_lid_top_target_angle_ = 0.0f;
    right_lid_top_target_angle_ = 0.0f;

    switch (current_emotion_) {
        // --- Angry: center V-shape, inner-top coverage ---
        // Matching original triCenterOffsetY: ANGRY1=25, ANGRY2=35, ANGRY3=45
        case EyeEmotion::Angry1:
            left_lid_top_target_y_ = -LID_H + 25.0f;
            right_lid_top_target_y_ = -LID_H + 25.0f;
            left_lid_top_target_angle_ = 150.0f;   // +15° inner dips
            right_lid_top_target_angle_ = -150.0f;
            break;
        case EyeEmotion::Angry2:
            left_lid_top_target_y_ = -LID_H + 35.0f;
            right_lid_top_target_y_ = -LID_H + 35.0f;
            left_lid_top_target_angle_ = 200.0f;   // +20°
            right_lid_top_target_angle_ = -200.0f;
            break;
        case EyeEmotion::Angry3:
            left_lid_top_target_y_ = -LID_H + 45.0f;
            right_lid_top_target_y_ = -LID_H + 45.0f;
            left_lid_top_target_angle_ = 250.0f;   // +25°
            right_lid_top_target_angle_ = -250.0f;
            break;

        // --- Curious1/2: asymmetric angry-style lid (one eye only) ---
        case EyeEmotion::Curious1:
            left_lid_top_target_y_ = -LID_H + 30.0f;
            left_lid_top_target_angle_ = 150.0f;
            break;
        case EyeEmotion::Curious2:
            right_lid_top_target_y_ = -LID_H + 30.0f;
            right_lid_top_target_angle_ = -150.0f;
            break;

        // --- Worried: outer-top coverage, droopy ---
        case EyeEmotion::Worried1:
            left_lid_top_target_y_ = -LID_H + 15.0f;
            right_lid_top_target_y_ = -LID_H + 15.0f;
            left_lid_top_target_angle_ = -100.0f;  // -10° outer dips
            right_lid_top_target_angle_ = 100.0f;
            break;

        // --- Sad: outer-top coverage, more droopy ---
        case EyeEmotion::Sad1:
            left_lid_top_target_y_ = -LID_H + 25.0f;
            right_lid_top_target_y_ = -LID_H + 25.0f;
            left_lid_top_target_angle_ = -200.0f;  // -20° outer dips
            right_lid_top_target_angle_ = 200.0f;
            break;
        case EyeEmotion::Sad2:
            left_lid_top_target_y_ = -LID_H + 35.0f;
            right_lid_top_target_y_ = -LID_H + 35.0f;
            left_lid_top_target_angle_ = -250.0f;  // -25°
            right_lid_top_target_angle_ = 250.0f;
            break;

        // --- Tired: outer-top + reduced height (height handled in ApplyPositions) ---
        case EyeEmotion::Tired:
            left_lid_top_target_y_ = -LID_H + 30.0f;
            right_lid_top_target_y_ = -LID_H + 30.0f;
            left_lid_top_target_angle_ = -200.0f;
            right_lid_top_target_angle_ = 200.0f;
            break;

        // --- Happy: bottom coverage ---
        // Matching original bottom triangle system
        case EyeEmotion::Happy1:
            left_lid_bottom_target_y_ = EYE_SIZE - 30.0f;
            right_lid_bottom_target_y_ = EYE_SIZE - 30.0f;
            break;
        case EyeEmotion::Happy2:
            left_lid_bottom_target_y_ = EYE_SIZE - 35.0f;
            right_lid_bottom_target_y_ = EYE_SIZE - 35.0f;
            break;

        case EyeEmotion::Cyclop:
            // No lid masking for single eye center
            break;

        case EyeEmotion::Drunk:
            left_lid_top_target_y_ = -LID_H + 22.0f;
            right_lid_top_target_y_ = -LID_H + 32.0f; // Asymmetric droop
            left_lid_top_target_angle_ = -120.0f;
            right_lid_top_target_angle_ = 80.0f;
            break;

        default:
            break;
    }
}
// ============================================================
// Touch Event (Blush on cheek tap via hardware timer)
// ============================================================
void EyeAnimation::HandleTouch(int x, int y) {
    if (!container_ || lv_obj_has_flag(container_, LV_OBJ_FLAG_HIDDEN)) return;

    int cx = screen_w_ / 2;
    int cy = screen_h_ / 2;
    uint32_t now = lv_tick_get();

    // -- Character Switching (Edge tap) --
    static uint32_t last_switch_ms_ = 0;
    if (now - last_switch_ms_ > 500) { // 500ms debounce
        if (x < 50 && y > 60 && y < screen_h_ - 60) {
            // Left edge tapped -> Previous Character
            if (current_character_ == Character::Doraemon) {
                current_character_ = Character::FelizShizuka;
            } else if (current_character_ == Character::FelizShizuka) {
                current_character_ = Character::DoraemonCute;
            } else if (current_character_ == Character::DoraemonCute) {
                current_character_ = Character::DoraemonShizuka;
            } else if (current_character_ == Character::DoraemonShizuka) {
                current_character_ = Character::Doraemon;
            }
            ESP_LOGI(TAG, "Hardware Touch: Left edge tap -> Switched Character PREV");
            last_switch_ms_ = now;
            return; // Exit to avoid triggering blush
        } 
        else if (x > screen_w_ - 50 && y > 60 && y < screen_h_ - 60) {
            // Right edge tapped -> Next Character
            if (current_character_ == Character::Doraemon) {
                current_character_ = Character::DoraemonShizuka;
            } else if (current_character_ == Character::DoraemonShizuka) {
                current_character_ = Character::DoraemonCute;
            } else if (current_character_ == Character::DoraemonCute) {
                current_character_ = Character::FelizShizuka;
            } else if (current_character_ == Character::FelizShizuka) {
                current_character_ = Character::Doraemon;
            }
            ESP_LOGI(TAG, "Hardware Touch: Right edge tap -> Switched Character NEXT");
            last_switch_ms_ = now;
            return; // Exit to avoid triggering blush
        }
    }

    // Check if touch is within cheek areas (bottom left/right of face)
    if (y > cy + 10 && y < cy + 80) {
        if ((x > cx - 90 && x < cx - 30) || (x > cx + 30 && x < cx + 90)) {
            blushing_ = true;
            touch_held_ = true;
            last_touch_ms_ = lv_tick_get();
            if (blush_start_ms_ == 0 || (lv_tick_get() - blush_start_ms_ > BLUSH_DURATION_MS)) {
                blush_start_ms_ = last_touch_ms_;
            }
            ESP_LOGI(TAG, "Face touch manually detected via Display! Changing color.");
        }
    }
}

// ============================================================
// Timer Callback
// ============================================================
void EyeAnimation::TimerCallback(lv_timer_t* timer) {
    auto* self = (EyeAnimation*)lv_timer_get_user_data(timer);
    uint32_t now = lv_tick_get();
    self->Update(now);
    self->ApplyPositions();
}

// ============================================================
// Main Update
// ============================================================
void EyeAnimation::Update(uint32_t now_ms) {
    UpdateBlink(now_ms);
    UpdateIdleLook(now_ms);
    UpdateEyeColor(now_ms);
    UpdateEmotionTimers(now_ms);
    UpdateLids();

    // Handle blush color transition logic
    if (blushing_) {
        // If no touch has been received for 150ms, the user has let go
        if (touch_held_ && now_ms - last_touch_ms_ > 150) {
            touch_held_ = false;
        }

        if (touch_held_) {
            // While holding, intensity grows up to 1.0f max over ~2 seconds
            blush_intensity_ += 0.01f; // Adjust speed as needed
            if (blush_intensity_ > 1.0f) blush_intensity_ = 1.0f;
            blush_start_ms_ = now_ms; // Keep the timer alive while held
        } else {
            // Once released, start fading away
            blush_intensity_ -= 0.005f;
            if (blush_intensity_ < 0.0f) {
                blush_intensity_ = 0.0f;
                if (now_ms - blush_start_ms_ > BLUSH_DURATION_MS) {
                    blushing_ = false;
                }
            }
        }
    } else {
        blush_intensity_ = 0.0f; 
    }

    // Pupil drift
    if (current_emotion_ == EyeEmotion::Drunk) {
        float drunk_t = (float)now_ms * 0.005f;
        pupil_off_x_ = (int16_t)(10.0f * cosf(drunk_t));
        pupil_off_y_ = (int16_t)(8.0f * sinf(drunk_t * 1.2f));
    } else {
        pupil_phase_ += 0.04f;
        if (pupil_phase_ > (float)M_PI * 2.0f) pupil_phase_ -= (float)M_PI * 2.0f;
        pupil_off_x_ = (int16_t)(3.0f * cosf(pupil_phase_));
        pupil_off_y_ = (int16_t)(2.0f * sinf(pupil_phase_ * 1.3f));
    }

    // Whisker animation phase
    whisker_phase_ += WHISKER_ANIM_SPEED;
    if (whisker_phase_ > (float)M_PI * 2.0f) whisker_phase_ -= (float)M_PI * 2.0f;

    // Mouth animation phase
    mouth_phase_ += MOUTH_ANIM_SPEED;
    if (mouth_phase_ > (float)M_PI * 2.0f) mouth_phase_ -= (float)M_PI * 2.0f;

    // Sinusoidal bounce (matching original BOUNCE_AMPL=8, period ~4s)
    float t = (float)(now_ms % 4000) / 4000.0f;
    bounce_y_ = sinf(t * 2.0f * (float)M_PI) * (float)BOUNCE_AMPL;
}

// ============================================================
// Blink (matching original topOffset approach exactly)
// ============================================================
void EyeAnimation::UpdateBlink(uint32_t now_ms) {
    // Auto-blink scheduling
    if (!blink_active_ && now_ms >= next_blink_ms_) {
        blink_active_ = true;
        blink_start_ms_ = now_ms;
        blink_left_ = true;
        blink_right_ = true;
        // 20% chance of wink (matching original idle wink behavior)
        if (randi(0, 4) == 0) {
            if (randi(0, 1) == 0) blink_right_ = false;
            else blink_left_ = false;
        }
    }

    if (blink_active_) {
        uint32_t elapsed = now_ms - blink_start_ms_;
        int16_t offset = 0;

        if (elapsed < BLINK_CLOSE_MS) {
            // Closing: 0 → BLINK_OFFSET_PX
            float p = (float)elapsed / (float)BLINK_CLOSE_MS;
            offset = (int16_t)((float)BLINK_OFFSET_PX * p);
        } else if (elapsed < BLINK_CLOSE_MS + BLINK_HOLD_MS) {
            // Holding closed
            offset = BLINK_OFFSET_PX;
        } else if (elapsed < BLINK_CLOSE_MS + BLINK_HOLD_MS + BLINK_OPEN_MS) {
            // Opening: BLINK_OFFSET_PX → 0
            float p = (float)(elapsed - BLINK_CLOSE_MS - BLINK_HOLD_MS) / (float)BLINK_OPEN_MS;
            offset = (int16_t)((float)BLINK_OFFSET_PX * (1.0f - p));
        } else {
            // Done
            offset = 0;
            blink_active_ = false;
            ScheduleNextBlink();
        }

        top_offset_ = offset;
    } else {
        top_offset_ = 0;
    }
}

void EyeAnimation::ScheduleNextBlink() {
    next_blink_ms_ = lv_tick_get() + randi(500, 2000);
}

// ============================================================
// Idle Look (matching original: destX/Y from -5 to +5, k=0.10)
// ============================================================
void EyeAnimation::UpdateIdleLook(uint32_t now_ms) {
    if (now_ms >= next_look_ms_) {
        target_off_x_ = (float)randi(-5, 5);
        target_off_y_ = (float)randi(-5, 5);
        next_look_ms_ = now_ms + randi(2000, 4000);
    }
    // Smooth interpolation (matching original UpdateVisualInterpolation k=0.10)
    off_x_ += (target_off_x_ - off_x_) * 0.10f;
    off_y_ += (target_off_y_ - off_y_) * 0.10f;
}

// ============================================================
// Eye Color (matching original EyeColor_update exactly)
// ============================================================
void EyeAnimation::UpdateEyeColor(uint32_t now_ms) {
    if (color_last_ms_ == 0) {
        cr_ = tr_; cg_ = tg_; cb_ = tb_;
        color_last_ms_ = now_ms;
        return;
    }
    uint32_t dt = now_ms - color_last_ms_;
    color_last_ms_ = now_ms;
    if (dt >= EYE_COLOR_FADE_MS) {
        cr_ = tr_; cg_ = tg_; cb_ = tb_;
        return;
    }
    float alpha = (float)dt / (float)EYE_COLOR_FADE_MS;
    if (alpha > 1.0f) alpha = 1.0f;
    cr_ += (tr_ - cr_) * alpha;
    cg_ += (tg_ - cg_) * alpha;
    cb_ += (tb_ - cb_) * alpha;
}

// ============================================================
// Emotion Timers (matching original: auto-return to idle after 2s)
// ============================================================
void EyeAnimation::UpdateEmotionTimers(uint32_t now_ms) {
    if (current_emotion_ != EyeEmotion::Idle &&
        emotion_end_ms_ > 0 && now_ms >= emotion_end_ms_) {
        current_emotion_ = EyeEmotion::Idle;
        emotion_end_ms_ = 0;
        // Return to white
        tr_ = 255.0f; tg_ = 255.0f; tb_ = 255.0f;
        // Reset lids
        SetLidTargets();
    }
}

// ============================================================
// Lid smooth interpolation (matching original triK = 0.15)
// ============================================================
void EyeAnimation::UpdateLids() {
    float k = 0.30f;
    left_lid_top_y_ += (left_lid_top_target_y_ - left_lid_top_y_) * k;
    right_lid_top_y_ += (right_lid_top_target_y_ - right_lid_top_y_) * k;
    left_lid_bottom_y_ += (left_lid_bottom_target_y_ - left_lid_bottom_y_) * k;
    right_lid_bottom_y_ += (right_lid_bottom_target_y_ - right_lid_bottom_y_) * k;
    left_lid_top_angle_ += (left_lid_top_target_angle_ - left_lid_top_angle_) * k;
    right_lid_top_angle_ += (right_lid_top_target_angle_ - right_lid_top_angle_) * k;
}

// ============================================================
// Apply positions (matching original EyeRenderer_drawFrame logic)
// ============================================================
void EyeAnimation::ApplyPositions() {
    if (!container_) return;

    int cx = screen_w_ / 2 + (int)off_x_;
    int cy = screen_h_ / 2 + (int)(off_y_ + bounce_y_);

    // Blink ratio from top_offset_ (0..1)
    float blinkT = (float)top_offset_ / (float)BLINK_OFFSET_PX;
    if (blinkT < 0.0f) blinkT = 0.0f;
    if (blinkT > 1.0f) blinkT = 1.0f;

    // Face positions
    if (current_character_ == Character::Doraemon) {
        // Doraemon
        lv_obj_clear_flag(face_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(muzzle_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nose_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(philtrum_line_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(collar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bell_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bell_slot_, LV_OBJ_FLAG_HIDDEN);
        for(int i=0; i<6; ++i) lv_obj_clear_flag(whiskers_[i], LV_OBJ_FLAG_HIDDEN);


        // Background: black for Doraemon
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        
        lv_obj_set_pos(face_, cx - 112, cy - 112);
        lv_obj_set_pos(muzzle_, cx - 88, cy + 18 - 72 - 8); 

        // Reset Shizuka mouth color
        // Reset Shizuka mouth color (Note: This looks like it was meant to be Doraemon's default)
        lv_obj_t* mouth_bg = lv_obj_get_child(mouth_, 0);
        if (mouth_bg) lv_obj_set_style_bg_color(mouth_bg, lv_color_make(230, 0, 0), 0);
    } else if (current_character_ == Character::DoraemonShizuka) {
        // Doraemon Shizuka GIF - Hide EVERYTHING else
        lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(muzzle_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nose_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(philtrum_line_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(collar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bell_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bell_slot_, LV_OBJ_FLAG_HIDDEN);
        for(int i=0; i<6; ++i) lv_obj_add_flag(whiskers_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_hi_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_hi_right_, LV_OBJ_FLAG_HIDDEN);

        // Show GIF
        lv_obj_clear_flag(doraemon_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
        if (gif_controller_ == nullptr || last_gif_owner_ != Character::DoraemonShizuka) {
            if (gif_controller_ != nullptr) {
                auto* controller = static_cast<LvglGif*>(gif_controller_);
                controller->Stop();
                delete controller;
                gif_controller_ = nullptr;
            }
            auto* controller = new LvglGif(&doraemon_shizuka_gif);
            gif_controller_ = controller;
            last_gif_owner_ = Character::DoraemonShizuka;
            controller->SetFrameCallback([this, controller]() {
                lv_img_set_src(doraemon_shizuka_gif_obj_, controller->image_dsc());
            });
            controller->Start();
        }
    } else if (current_character_ == Character::DoraemonCute) {
        // Doraemon Cute GIF
        lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(muzzle_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nose_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(philtrum_line_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(collar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bell_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bell_slot_, LV_OBJ_FLAG_HIDDEN);
        for(int i=0; i<6; ++i) lv_obj_add_flag(whiskers_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_hi_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_hi_right_, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(doraemon_cute_gif_obj_, LV_OBJ_FLAG_HIDDEN);
        if (gif_controller_ == nullptr || last_gif_owner_ != Character::DoraemonCute) {
            if (gif_controller_ != nullptr) {
                auto* controller = static_cast<LvglGif*>(gif_controller_);
                controller->Stop();
                delete controller;
                gif_controller_ = nullptr;
            }
            auto* controller = new LvglGif(&doraemon_cute_gif);
            gif_controller_ = controller;
            last_gif_owner_ = Character::DoraemonCute;
            controller->SetFrameCallback([this, controller]() {
                lv_img_set_src(doraemon_cute_gif_obj_, controller->image_dsc());
            });
            controller->Start();
        }
    } else if (current_character_ == Character::HienNhien) {
        // Hien Nhien Static Image - Hide EVERYTHING else
        lv_obj_add_flag(face_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(muzzle_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(nose_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(philtrum_line_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(collar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bell_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bell_slot_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bell_slot_, LV_OBJ_FLAG_HIDDEN);
        for(int i=0; i<6; ++i) lv_obj_add_flag(whiskers_[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(doraemon_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(doraemon_cute_gif_obj_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(feliz_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
        if (gif_controller_ == nullptr || last_gif_owner_ != Character::FelizShizuka) {
            if (gif_controller_ != nullptr) {
                auto* controller = static_cast<LvglGif*>(gif_controller_);
                controller->Stop();
                delete controller;
                gif_controller_ = nullptr;
            }
            auto* controller = new LvglGif(&feliz_shizuka_gif);
            gif_controller_ = controller;
            last_gif_owner_ = Character::FelizShizuka;
            controller->SetFrameCallback([this, controller]() {
                lv_img_set_src(feliz_shizuka_gif_obj_, controller->image_dsc());
            });
            controller->Start();
        }
    }


    // Cleanup for FelizShizuka
    if (current_character_ != Character::FelizShizuka && feliz_shizuka_gif_obj_ != nullptr) {
        lv_obj_add_flag(feliz_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
    }

    // Cleanup GIF if not in a GIF mode
    bool is_gif_char = (current_character_ == Character::DoraemonShizuka || 
                        current_character_ == Character::DoraemonCute ||
                        current_character_ == Character::FelizShizuka);
    if (!is_gif_char && gif_controller_ != nullptr) {
        auto* controller = static_cast<LvglGif*>(gif_controller_);
        controller->Stop();
        delete controller;
        gif_controller_ = nullptr;
        last_gif_owner_ = Character::Doraemon; // Reset
        lv_obj_add_flag(doraemon_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(doraemon_cute_gif_obj_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(feliz_shizuka_gif_obj_, LV_OBJ_FLAG_HIDDEN);
    }

    // Eyes
    int eyeW = 44;
    int eyeH = 56;
    
    // Modify eye size and structure based on character and emotion
    if (current_emotion_ == EyeEmotion::Cyclop) {
        eyeW = 90;
        eyeH = 100;
        eyeW = 40;
        eyeH = 50;
    }

    int eyeHVis = (int)(eyeH * (1.0f - blinkT));
    if (eyeHVis < 4) eyeHVis = 4;
    
    int eyeY = cy - 54 - eyeHVis / 2; 
    int left_eye_x = cx - 20 - eyeW / 2;
    int right_eye_x = cx + 20 - eyeW / 2;

    if (current_emotion_ == EyeEmotion::Cyclop) {
        left_eye_x = cx - eyeW / 2;
        lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_size(left_eye_, eyeW, eyeHVis);
    lv_obj_set_size(right_eye_, eyeW, eyeHVis);

    if (current_emotion_ == EyeEmotion::Drunk) {
        lv_obj_set_pos(left_eye_, left_eye_x, eyeY - 7);
        lv_obj_set_pos(right_eye_, right_eye_x, eyeY + 7);
    } else {
        lv_obj_set_pos(left_eye_, left_eye_x, eyeY);
        lv_obj_set_pos(right_eye_, right_eye_x, eyeY);
    }

    // Pupils (hide when blinking a lot)
    if (blinkT < 0.7f) {
        lv_obj_clear_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pupil_hi_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pupil_hi_right_, LV_OBJ_FLAG_HIDDEN);
        
        int pupil_base_lx = cx - 20;
        int pupil_base_rx = cx + 20;
        int pupil_base_y = cy - 60;
        int pupil_size = 28;

        if (current_emotion_ == EyeEmotion::Cyclop) {
            pupil_base_lx = cx;
            pupil_base_rx = cx;
            pupil_base_y = eyeY + eyeHVis / 2;
            lv_obj_add_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(pupil_hi_right_, LV_OBJ_FLAG_HIDDEN);
        }

        lv_obj_set_size(pupil_left_, pupil_size, pupil_size);
        lv_obj_set_size(pupil_right_, pupil_size, pupil_size);

        // Half pupil width is pupil_size/2.
        int half_pupil = pupil_size / 2;
        int pupil_lx = pupil_base_lx - half_pupil + pupil_off_x_;
        int pupil_ly = pupil_base_y + pupil_off_y_ - half_pupil; 
        int pupil_rx = pupil_base_rx - half_pupil + pupil_off_x_;
        int pupil_ry = pupil_base_y + pupil_off_y_ - half_pupil; 
        
        lv_obj_set_pos(pupil_left_, pupil_lx, pupil_ly);
        lv_obj_set_pos(pupil_right_, pupil_rx, pupil_ry);
        
        // Highlights follow pupils, offset to the top-right slightly
        lv_obj_set_pos(pupil_hi_left_, pupil_lx + 16, pupil_ly + 4);
        lv_obj_set_pos(pupil_hi_right_, pupil_rx + 16, pupil_ry + 4);
    } else {
        lv_obj_add_flag(pupil_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_right_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_hi_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_hi_right_, LV_OBJ_FLAG_HIDDEN);
    }

    // Nose
    lv_obj_set_pos(nose_, cx - 15, cy - 30); // Shifted up 8px further (was 22)

    // Philtrum Line
    static lv_point_precise_t philtrum_pts[2];
    philtrum_pts[0].x = (lv_coord_t)cx;
    philtrum_pts[0].y = (lv_coord_t)(cy - 2);
    philtrum_pts[1].x = (lv_coord_t)cx;
    philtrum_pts[1].y = (lv_coord_t)(cy + 33);
    lv_line_set_points(philtrum_line_, philtrum_pts, 2);


    // Whiskers (Animated positions relative to center)
    // Lines
    static lv_point_precise_t whisk_pts[6][4]; // Support up to 4 points for curves
    
    // Animation offsets
    float w_off1 = sinf(whisker_phase_) * WHISKER_ANIM_AMPLITUDE;
    float w_off2 = sinf(whisker_phase_ + 1.0f) * WHISKER_ANIM_AMPLITUDE;
    float w_off3 = sinf(whisker_phase_ + 2.0f) * WHISKER_ANIM_AMPLITUDE;

    // Top Left (2 points)
    whisk_pts[0][0].x = (lv_coord_t)(cx - 28); whisk_pts[0][0].y = (lv_coord_t)(cy + 0);
    whisk_pts[0][1].x = (lv_coord_t)(cx - 85); whisk_pts[0][1].y = (lv_coord_t)(cy - 12 + w_off1);
    lv_line_set_points(whiskers_[0], whisk_pts[0], 2);
    
    // Mid Left (2 points)
    whisk_pts[1][0].x = (lv_coord_t)(cx - 28); whisk_pts[1][0].y = (lv_coord_t)(cy + 12);
    whisk_pts[1][1].x = (lv_coord_t)(cx - 85); whisk_pts[1][1].y = (lv_coord_t)(cy + 12 + w_off2);
    lv_line_set_points(whiskers_[1], whisk_pts[1], 2);

    // Bot Left (4 points, curved down)
    whisk_pts[2][0].x = (lv_coord_t)(cx - 28); whisk_pts[2][0].y = (lv_coord_t)(cy + 24);
    whisk_pts[2][1].x = (lv_coord_t)(cx - 45); whisk_pts[2][1].y = (lv_coord_t)(cy + 28 + w_off3 * 0.3f);
    whisk_pts[2][2].x = (lv_coord_t)(cx - 65); whisk_pts[2][2].y = (lv_coord_t)(cy + 35 + w_off3 * 0.6f);
    whisk_pts[2][3].x = (lv_coord_t)(cx - 85); whisk_pts[2][3].y = (lv_coord_t)(cy + 45 + w_off3);
    lv_line_set_points(whiskers_[2], whisk_pts[2], 4);
    
    // Top Right (2 points)
    whisk_pts[3][0].x = (lv_coord_t)(cx + 28); whisk_pts[3][0].y = (lv_coord_t)(cy + 0);
    whisk_pts[3][1].x = (lv_coord_t)(cx + 85); whisk_pts[3][1].y = (lv_coord_t)(cy - 12 + w_off1);
    lv_line_set_points(whiskers_[3], whisk_pts[3], 2);
    
    // Mid Right (2 points)
    whisk_pts[4][0].x = (lv_coord_t)(cx + 28); whisk_pts[4][0].y = (lv_coord_t)(cy + 12);
    whisk_pts[4][1].x = (lv_coord_t)(cx + 85); whisk_pts[4][1].y = (lv_coord_t)(cy + 12 + w_off2);
    lv_line_set_points(whiskers_[4], whisk_pts[4], 2);

    // Bot Right (4 points, curved down)
    whisk_pts[5][0].x = (lv_coord_t)(cx + 28); whisk_pts[5][0].y = (lv_coord_t)(cy + 24);
    whisk_pts[5][1].x = (lv_coord_t)(cx + 45); whisk_pts[5][1].y = (lv_coord_t)(cy + 28 + w_off3 * 0.3f);
    whisk_pts[5][2].x = (lv_coord_t)(cx + 65); whisk_pts[5][2].y = (lv_coord_t)(cy + 35 + w_off3 * 0.6f);
    whisk_pts[5][3].x = (lv_coord_t)(cx + 85); whisk_pts[5][3].y = (lv_coord_t)(cy + 45 + w_off3);
    lv_line_set_points(whiskers_[5], whisk_pts[5], 4);

    // --- PERMANENT HAPPY MOUTH ---
    if (current_character_ == Character::Doraemon) {
        // Lowered by 15px (18 -> 33) to compensate for reduced container height
        // This keeps the bottom arc in the exact same visual position on the face
        int mouth_base_y = cy + 33; 
        lv_obj_clear_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(mouth_, cx - 50, mouth_base_y); // Center 100/2=50
        
        // Animate mouth height
        float mouth_h_offset = sinf(mouth_phase_) * MOUTH_ANIM_AMPLITUDE;
        // Base height is 50. Add offset, but keep it constrained so it doesn't invert or grow too large
        int current_mouth_h = (int)(50 + mouth_h_offset);
        if (current_mouth_h < 30) current_mouth_h = 30; // Minimum mouth height
        if (current_mouth_h > 70) current_mouth_h = 70; // Maximum mouth height
        
        lv_obj_set_size(mouth_, 100, current_mouth_h);
        
        // Adjust background position to keep bottom flush
        // Background height is 100. Shift up by (100 - current_mouth_h)
        lv_obj_t* mouth_bg = lv_obj_get_child(mouth_, 0); 
        if (mouth_bg) {
            lv_obj_set_pos(mouth_bg, 0, -(100 - current_mouth_h));
            
            // Also adjust tongue relative to the new background position
            // Tongue container is the second child
            lv_obj_t* tongue_cont = lv_obj_get_child(mouth_, 1);
            if (tongue_cont) {
                int target_tongue_y = current_mouth_h - 28 - 6;
                lv_obj_set_pos(tongue_cont, 22, target_tongue_y);
            }
        }
    }

    // Full face blush (muzzle color transition)
    lv_color_t muzzle_color = lv_color_white();
    if (blushing_ && blush_intensity_ > 0.01f) {
        lv_color_t pink = lv_color_make(255, 180, 180);
        lv_color_t deep_red = lv_color_make(240, 20, 20);
        
        if (blush_intensity_ < 0.5f) {
            // White to Pink
            muzzle_color = lv_color_mix(pink, lv_color_white(), (uint8_t)(blush_intensity_ * 2.0f * 255.0f));
        } else {
            // Pink to Deep Red
            muzzle_color = lv_color_mix(deep_red, pink, (uint8_t)((blush_intensity_ - 0.5f) * 2.0f * 255.0f));
        }
    }
    lv_obj_set_style_bg_color(muzzle_, muzzle_color, 0);

    // Tears
    if (current_emotion_ == EyeEmotion::Sad1 || current_emotion_ == EyeEmotion::Sad2) {
        lv_obj_clear_flag(tear_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(tear_right_, LV_OBJ_FLAG_HIDDEN);
        
        int tear_lx = cx - 20 - 6;
        int tear_rx = cx + 20 - 6;
        int tear_y = cy - 5;

        lv_obj_set_pos(tear_left_, tear_lx, tear_y);
        lv_obj_set_pos(tear_right_, tear_rx, tear_y);
    } else {
        lv_obj_add_flag(tear_left_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tear_right_, LV_OBJ_FLAG_HIDDEN);
    }

    // Collar + bell
    lv_obj_set_pos(collar_, cx - 69, cy + 88); // cx - 138/2 = cx - 69
    lv_obj_set_pos(bell_, cx - 9, cy + 96);
    lv_obj_set_pos(bell_slot_, cx - 4, cy + 103);
}
