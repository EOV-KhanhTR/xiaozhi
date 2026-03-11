#pragma once
#include <LovyanGFX.hpp>

// =====================================================
// Doraemon Face Animation Module
// Draws a cute Doraemon face with blinking eyes on a
// 240×240 LovyanGFX sprite canvas.
// =====================================================
namespace DoraemonFace {

  // Call once at startup (after display init)
  void begin();

  // Call every frame (~16 ms) to advance blink timer
  void update(uint32_t nowMs);

  // Render one frame onto the given sprite.
  // Returns true while the animation is active.
  bool renderFrame(lgfx::LGFX_Sprite* canvas, uint32_t nowMs);

  // Start / stop the Doraemon face overlay
  void activate();
  void deactivate();

  // Expressions
  enum Expression {
    NORMAL,
    HAPPY,
    SMILING,
    CRYING
  };

  void setExpression(Expression expr);

  // Handle touch interactions
  void handleTouch(int16_t x, int16_t y);

  // Query state
  bool isActive();
}
