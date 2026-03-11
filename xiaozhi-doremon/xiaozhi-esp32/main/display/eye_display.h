#ifndef EYE_DISPLAY_H
#define EYE_DISPLAY_H

#include "lcd_display.h"
#include "eye_animation.h"
#include <memory>

/**
 * EyeDisplay: An LcdDisplay subclass that shows animated eyes
 * instead of emoji images for emotions.
 * 
 * Inherits all the standard LCD display UI (status bar, bottom bar, etc.)
 * but replaces the emoji_box_ center content with a full-screen animated
 * eye canvas that responds to SetEmotion() calls.
 */
class EyeDisplay : public SpiLcdDisplay {
public:
    EyeDisplay(esp_lcd_panel_io_handle_t io_handle,
               esp_lcd_panel_handle_t panel_handle,
               int width, int height,
               int offset_x, int offset_y,
               bool mirror_x, bool mirror_y, bool swap_xy);

    virtual ~EyeDisplay();

    // Override to create eye animation canvas instead of emoji
    virtual void SetupUI() override;

    // Override to drive eye animation from emotion strings
    virtual void SetEmotion(const char* emotion) override;

    // Override to force black background when theme is refreshed
    virtual void SetTheme(Theme* theme) override;

    // Handle raw touch coordinates directly
    virtual void HandleTouch(int x, int y) override;

private:
    std::unique_ptr<EyeAnimation> eye_animation_;
};

#endif // EYE_DISPLAY_H
