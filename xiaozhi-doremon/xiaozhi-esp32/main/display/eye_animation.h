#ifndef EYE_ANIMATION_H
#define EYE_ANIMATION_H

#include <lvgl.h>
#include <cstdint>

// Eye emotion types matching original display_system.h exactly
enum class EyeEmotion : uint8_t {
    Idle = 0,
    Curious,
    Angry1,
    Love,
    Tired,
    Excited,
    Angry2,
    Angry3,
    Worried1,
    Curious1,
    Curious2,
    Sad1,
    Sad2,
    Happy1,
    Happy2,
    Cyclop,
    Drunk,
    Count
};

// Convert xiaozhi emotion string to EyeEmotion enum
EyeEmotion EyeEmotion_FromString(const char* emotion);

class EyeAnimation {
public:
    EyeAnimation();
    ~EyeAnimation();

    void Init(lv_obj_t* parent, int screen_w, int screen_h);
    void SetEmotion(EyeEmotion emotion);
    void SetVisible(bool visible);
    void HandleTouch(int x, int y);

private:
    // Main container (full screen, black background)
    lv_obj_t* container_ = nullptr;

    // Eye objects (solid colored rounded rectangles, NO pupils)
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;

    // Doraemon face objects
    lv_obj_t* face_ = nullptr;
    lv_obj_t* muzzle_ = nullptr;
    lv_obj_t* pupil_left_ = nullptr;
    lv_obj_t* pupil_right_ = nullptr;
    lv_obj_t* nose_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* philtrum_line_ = nullptr;
    lv_obj_t* tongue_ = nullptr;
    lv_obj_t* whiskers_[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    lv_obj_t* collar_ = nullptr;
    lv_obj_t* bell_ = nullptr;
    lv_obj_t* bell_slot_ = nullptr;

    // Shizuka face objects
    lv_obj_t* doraemon_shizuka_gif_obj_ = nullptr;
    lv_obj_t* doraemon_cute_gif_obj_ = nullptr;
    lv_obj_t* hien_nhien_img_ = nullptr;
    void* gif_controller_ = nullptr;

    enum class Character { Doraemon, DoraemonShizuka, DoraemonCute, HienNhien };
    Character current_character_ = Character::Doraemon;
    Character last_gif_owner_ = Character::Doraemon;

    // Tear overlays
    lv_obj_t* tear_left_ = nullptr;
    lv_obj_t* tear_right_ = nullptr;

    // Eyelid overlays (children of eyes, clipped by clip_corner)
    lv_obj_t* left_lid_top_ = nullptr;
    lv_obj_t* right_lid_top_ = nullptr;
    lv_obj_t* left_lid_bottom_ = nullptr;
    lv_obj_t* right_lid_bottom_ = nullptr;

    lv_timer_t* anim_timer_ = nullptr;

    int screen_w_ = 240;
    int screen_h_ = 240;

    // Geometry matching original exactly
    static constexpr int EYE_SIZE = 80;
    static constexpr int EYE_RADIUS = 24;
    static constexpr int GAP = 10;
    static constexpr int CLOSED_HEIGHT = 6;
    static constexpr int TIRED_EYE_HEIGHT = 40;

    // Blink matching original exactly
    static constexpr uint32_t BLINK_CLOSE_MS = 40;
    static constexpr uint32_t BLINK_HOLD_MS = 20;
    static constexpr uint32_t BLINK_OPEN_MS = 80;
    static constexpr int16_t BLINK_OFFSET_PX = 50;

    // Idle motion matching original
    static constexpr int BOUNCE_AMPL = 8;

    // Happy animation matching original
    static constexpr float HAPPY_SCALE = 1.15f;
    static constexpr uint32_t HAPPY_DURATION_MS = 2000;
    static constexpr float HAPPY_BOUNCE_FREQ = 2.0f;
    static constexpr float HAPPY_BOUNCE_AMPL = 8.0f;

    // Color fade matching original
    static constexpr uint32_t EYE_COLOR_FADE_MS = 500;

    // Emotion duration (2s then idle)
    static constexpr uint32_t EMOTION_DURATION_MS = 2000;

    // Lid overlay geometry
    static constexpr int LID_W = EYE_SIZE + 40;
    static constexpr int LID_H = EYE_SIZE;

    // Runtime state
    EyeEmotion current_emotion_ = EyeEmotion::Idle;
    uint32_t emotion_start_ms_ = 0;
    uint32_t emotion_end_ms_ = 0;

    // Eye color (matching original RGB float fade)
    float cr_ = 255.0f, cg_ = 255.0f, cb_ = 255.0f;
    float tr_ = 255.0f, tg_ = 255.0f, tb_ = 255.0f;
    uint32_t color_last_ms_ = 0;

    // Blink (matching original topOffset system)
    bool blink_active_ = false;
    bool blink_left_ = true;
    bool blink_right_ = true;
    uint32_t blink_start_ms_ = 0;
    uint32_t next_blink_ms_ = 0;
    int16_t top_offset_ = 0;

    // Idle look (matching original: -5 to +5)
    float off_x_ = 0.0f, off_y_ = 0.0f;
    float target_off_x_ = 0.0f, target_off_y_ = 0.0f;
    uint32_t next_look_ms_ = 0;

    // Bounce (matching original sinusoidal bob)
    float bounce_y_ = 0.0f;

    // Eye scale
    float eye_scale_ = 1.0f;

    // Doraemon animation state
    lv_obj_t* pupil_hi_left_ = nullptr;
    lv_obj_t* pupil_hi_right_ = nullptr;
    float pupil_phase_ = 0.0f;
    int16_t pupil_off_x_ = 0;
    int16_t pupil_off_y_ = 0;

    // Whisker animation
    float whisker_phase_ = 0.0f;
    static constexpr float WHISKER_ANIM_SPEED = 0.15f;
    static constexpr float WHISKER_ANIM_AMPLITUDE = 3.0f;

    // Mouth animation
    float mouth_phase_ = 0.0f;
    static constexpr float MOUTH_ANIM_SPEED = 0.2f;
    static constexpr float MOUTH_ANIM_AMPLITUDE = 10.0f;

    // Interactive state
    bool blushing_ = false;
    bool touch_held_ = false;
    uint32_t blush_start_ms_ = 0;
    uint32_t last_touch_ms_ = 0;
    float blush_intensity_ = 0.0f; // 0.0 (white) to 1.0 (dark red)
    static constexpr uint32_t BLUSH_DURATION_MS = 3000;

    // Lid overlay state (smooth interpolation)
    float left_lid_top_y_ = 0.0f;
    float right_lid_top_y_ = 0.0f;
    float left_lid_bottom_y_ = 0.0f;
    float right_lid_bottom_y_ = 0.0f;
    float left_lid_top_angle_ = 0.0f;
    float right_lid_top_angle_ = 0.0f;

    // Lid targets
    float left_lid_top_target_y_ = 0.0f;
    float right_lid_top_target_y_ = 0.0f;
    float left_lid_bottom_target_y_ = 0.0f;
    float right_lid_bottom_target_y_ = 0.0f;
    float left_lid_top_target_angle_ = 0.0f;
    float right_lid_top_target_angle_ = 0.0f;

    // Methods
    void CreateEyeObjects(lv_obj_t* parent);
    void Update(uint32_t now_ms);
    void ApplyPositions();
    void UpdateBlink(uint32_t now_ms);
    void UpdateIdleLook(uint32_t now_ms);
    void UpdateEyeColor(uint32_t now_ms);
    void UpdateEmotionTimers(uint32_t now_ms);
    void UpdateLids();
    void SetLidTargets();
    void ScheduleNextBlink();
    void GetTargetColor(EyeEmotion emo, uint8_t& r, uint8_t& g, uint8_t& b);

    static void TimerCallback(lv_timer_t* timer);
    static void OnTouchEvent(lv_event_t* e);
};

#endif // EYE_ANIMATION_H
