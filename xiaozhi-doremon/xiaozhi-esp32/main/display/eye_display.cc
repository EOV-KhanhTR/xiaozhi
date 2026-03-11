#include "eye_display.h"
#include "lvgl_display/lvgl_theme.h"
#include <esp_log.h>

#define TAG "EyeDisplay"

EyeDisplay::EyeDisplay(esp_lcd_panel_io_handle_t io_handle,
                       esp_lcd_panel_handle_t panel_handle,
                       int width, int height,
                       int offset_x, int offset_y,
                       bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
}

EyeDisplay::~EyeDisplay() {
    eye_animation_.reset();
}

void EyeDisplay::SetupUI() {
    // Force dark theme before base SetupUI so all elements are black
    auto* dark_theme = LvglThemeManager::GetInstance().GetTheme("dark");
    if (dark_theme) {
        current_theme_ = dark_theme;
    }

    // Call base SetupUI to create all standard UI elements
    SpiLcdDisplay::SetupUI();

    DisplayLockGuard lock(this);

    // Force the entire screen and all UI elements to black background
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    if (container_) {
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
    }
    if (top_bar_) {
        lv_obj_set_style_bg_color(top_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    }
    if (status_bar_) {
        lv_obj_set_style_bg_color(status_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
    }
    if (bottom_bar_) {
        lv_obj_set_style_bg_color(bottom_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0);
    }
    if (content_) {
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(content_, LV_OPA_COVER, 0);
    }

    // Add padding for round screen
    lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.2, 0);
    lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.2, 0);

    // Hide the standard emoji box - we'll use eye animation instead
    if (emoji_box_ != nullptr) {
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    }

    // Create eye animation on the active screen
    eye_animation_ = std::make_unique<EyeAnimation>();
    eye_animation_->Init(screen, width_, height_);

    // Move the eye container behind the top_bar_ and status_bar_
    lv_obj_t* eye_container = lv_obj_get_child(screen, -1);
    if (eye_container) {
        lv_obj_move_to_index(eye_container, 1);
    }

    ESP_LOGI(TAG, "EyeDisplay UI setup complete (black background + animated eyes)");
}

void EyeDisplay::SetTheme(Theme* theme) {
    // Let the base class apply fonts, text colors, etc.
    LcdDisplay::SetTheme(theme);

    // Then force black background on everything
    DisplayLockGuard lock(this);
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    if (container_) {
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_src(container_, nullptr, 0);
    }
    if (top_bar_) {
        lv_obj_set_style_bg_color(top_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    }
    if (status_bar_) {
        lv_obj_set_style_bg_color(status_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
    }
    if (content_) {
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(content_, LV_OPA_COVER, 0);
    }
    if (bottom_bar_) {
        lv_obj_set_style_bg_color(bottom_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0);
    }

    // Force white text for visibility on black background
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    if (network_label_) lv_obj_set_style_text_color(network_label_, lv_color_white(), 0);
    if (status_label_) lv_obj_set_style_text_color(status_label_, lv_color_white(), 0);
    if (notification_label_) lv_obj_set_style_text_color(notification_label_, lv_color_white(), 0);
    if (mute_label_) lv_obj_set_style_text_color(mute_label_, lv_color_white(), 0);
    if (battery_label_) lv_obj_set_style_text_color(battery_label_, lv_color_white(), 0);
    if (emoji_label_) lv_obj_set_style_text_color(emoji_label_, lv_color_white(), 0);
    if (chat_message_label_) lv_obj_set_style_text_color(chat_message_label_, lv_color_white(), 0);

    ESP_LOGI(TAG, "SetTheme: forced black background");
}

void EyeDisplay::SetEmotion(const char* emotion) {
    if (!eye_animation_) {
        // Fall back to default behavior if eye animation not initialized
        SpiLcdDisplay::SetEmotion(emotion);
        return;
    }

    ESP_LOGI(TAG, "SetEmotion: %s", emotion ? emotion : "null");

    // Convert emotion string to eye emotion and set it
    EyeEmotion eye_emotion = EyeEmotion_FromString(emotion);
    eye_animation_->SetEmotion(eye_emotion);

    // Also hide the default emoji display (in case it was shown)
    DisplayLockGuard lock(this);
    if (emoji_box_ != nullptr) {
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    }
}

void EyeDisplay::HandleTouch(int x, int y) {
    if (eye_animation_) {
        eye_animation_->HandleTouch(x, y);
    }
}
