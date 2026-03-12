// =====================================================
// Doraemon Face Animation — LovyanGFX 240×240
// =====================================================
// Renders a cute Doraemon face with periodic blinking.
// Designed for a 240×240 round/square GC9A01 / ST7789 display.
// =====================================================

#include "doraemon_face.h"
#include <cmath>

// =====================================================
// Colour palette (RGB565)
// =====================================================
static constexpr uint16_t COL_BLUE       = 0x2C9F;  // Doraemon blue  (#0093D6 approx)
static constexpr uint16_t COL_WHITE      = 0xFFFF;
static constexpr uint16_t COL_BLACK      = 0x0000;
static constexpr uint16_t COL_RED        = 0xF800;  // nose
static constexpr uint16_t COL_RED_DARK   = 0xC000;  // nose highlight shadow
static constexpr uint16_t COL_NOSE_HI    = 0xFEDF;  // small white highlight on nose
static constexpr uint16_t COL_BG         = 0x2C9F;  // background = same blue

// =====================================================
// Geometry constants (centred on 240×240)
// =====================================================
static constexpr int16_t CX = 120;  // centre X
static constexpr int16_t CY = 120;  // centre Y

// Outer blue face circle
static constexpr int16_t FACE_R = 112;

// Inner white muzzle ellipse
static constexpr int16_t MUZZLE_CY = 138;
static constexpr int16_t MUZZLE_RX = 88;
static constexpr int16_t MUZZLE_RY = 72;

// Eyes
static constexpr int16_t EYE_L_CX = 100;
static constexpr int16_t EYE_R_CX = 140;
static constexpr int16_t EYE_CY   = 82;
static constexpr int16_t EYE_RX   = 22;   // horizontal radius
static constexpr int16_t EYE_RY   = 28;   // vertical radius (taller oval)
static constexpr int16_t EYE_OUTLINE = 2;

// Pupils
static constexpr int16_t PUPIL_R  = 7;
static constexpr int16_t PUPIL_L_CX = 108;  // slightly inward
static constexpr int16_t PUPIL_R_CX = 132;
static constexpr int16_t PUPIL_CY   = 88;   // slightly lower
// Pupil glint
static constexpr int16_t GLINT_R = 2;
static constexpr int16_t GLINT_OFF_X = -3;
static constexpr int16_t GLINT_OFF_Y = -3;

// Nose
static constexpr int16_t NOSE_CX = CX;
static constexpr int16_t NOSE_CY = 112;
static constexpr int16_t NOSE_R  = 11;

// Mouth — vertical line from nose + smile arc
static constexpr int16_t MOUTH_TOP_Y  = NOSE_CY + NOSE_R;
static constexpr int16_t MOUTH_BOT_Y  = 170;
static constexpr int16_t SMILE_CX     = CX;
static constexpr int16_t SMILE_CY_ARC = 148;
static constexpr int16_t SMILE_RX     = 52;
static constexpr int16_t SMILE_RY     = 22;
// Open mouth geometry (like the reference image)
static constexpr int16_t MOUTH_OPEN_CX = CX;
static constexpr int16_t MOUTH_OPEN_CY = 176;
static constexpr int16_t MOUTH_OPEN_RX = 22;
static constexpr int16_t MOUTH_OPEN_RY = 28;
static constexpr int16_t TONGUE_RX     = 14;
static constexpr int16_t TONGUE_RY     = 12;

// Whiskers (3 per side)
static constexpr int16_t WHISK_LEN = 38;
static constexpr int16_t WHISK_START_X_L = 60;
static constexpr int16_t WHISK_START_X_R = 180;
static constexpr int16_t WHISK_BASE_X_L  = 92;
static constexpr int16_t WHISK_BASE_X_R  = 148;
static constexpr int16_t WHISK_Y_MID     = 128;
static constexpr int16_t WHISK_Y_SPREAD  = 16;

// Collar
static constexpr int16_t COLLAR_Y   = 198;
static constexpr int16_t COLLAR_H   = 12;
static constexpr uint16_t COL_COLLAR = 0xF800;  // red

// Bell
static constexpr int16_t BELL_CX = CX;
static constexpr int16_t BELL_CY = 210;
static constexpr int16_t BELL_R  = 9;
static constexpr uint16_t COL_YELLOW = 0xFFE0;

// =====================================================
// Blink animation timing
// =====================================================
static constexpr uint32_t BLINK_INTERVAL_MIN_MS = 2500;
static constexpr uint32_t BLINK_INTERVAL_MAX_MS = 5500;
static constexpr uint32_t BLINK_CLOSE_MS        = 70;   // closing phase
static constexpr uint32_t BLINK_HOLD_MS         = 50;   // fully closed
static constexpr uint32_t BLINK_OPEN_MS         = 100;  // reopening
static constexpr uint32_t BLINK_TOTAL_MS        = BLINK_CLOSE_MS + BLINK_HOLD_MS + BLINK_OPEN_MS;

// =====================================================
// Internal state
// =====================================================
static bool     s_active       = true;   // Always active by default
static uint32_t s_nextBlinkMs  = 0;
static uint32_t s_blinkStartMs = 0;
static bool     s_blinking     = false;
// Eye movement (pupil drift)
static float    s_eyePhase     = 0.0f;
static int16_t  s_eyeOffX      = 0;
static int16_t  s_eyeOffY      = 0;
// Mouth animation (open/close)
static float    s_mouthPhase   = 0.0f;
static float    s_mouthOpenT   = 0.0f; // 0=closed, 1=open
// Whisker animation
static float    s_whiskPhase   = 0.0f;

// Expressions & Interactions
static DoraemonFace::Expression s_expr = DoraemonFace::NORMAL;
static bool     s_blushing     = false;
static uint32_t s_blushStartMs = 0;
static constexpr uint32_t BLUSH_DURATION_MS = 3000;

// Pseudo-random helper (works without Arduino random() too)
static uint32_t s_rng = 12345;
static uint32_t fastRand() {
  s_rng ^= s_rng << 13;
  s_rng ^= s_rng >> 17;
  s_rng ^= s_rng << 5;
  return s_rng;
}
static uint32_t randRange(uint32_t lo, uint32_t hi) {
  return lo + (fastRand() % (hi - lo + 1));
}

// =====================================================
// Helper: draw a thick arc (smile)
// =====================================================
static void drawSmileArc(lgfx::LGFX_Sprite* c, int16_t cx, int16_t cy,
                          int16_t rx, int16_t ry, float startDeg, float endDeg,
                          uint16_t color, uint8_t thick) {
  for (float a = startDeg; a <= endDeg; a += 1.0f) {
    float rad = a * M_PI / 180.0f;
    int16_t px = cx + (int16_t)(rx * cosf(rad));
    int16_t py = cy + (int16_t)(ry * sinf(rad));
    c->fillCircle(px, py, thick, color);
  }
}

// =====================================================
// Draw one eye (open, or partially closed for blink)
// blinkT: 0.0 = fully open, 1.0 = fully closed
// =====================================================
static void drawEye(lgfx::LGFX_Sprite* c, int16_t ecx, int16_t pupilCX,
                     int16_t ecy, int16_t pupilCY, float blinkT,
                     int16_t pupilOffX, int16_t pupilOffY) {
  // Compute visible vertical radius based on blink progress
  int16_t visRY = (int16_t)((1.0f - blinkT) * EYE_RY);
  if (visRY < 1) visRY = 1;

  // Outline (slightly larger)
  c->fillEllipse(ecx, ecy, EYE_RX + EYE_OUTLINE, visRY + EYE_OUTLINE, COL_BLACK);
  // White sclera
  c->fillEllipse(ecx, ecy, EYE_RX, visRY, COL_WHITE);

  // Pupil & glint only when eye is mostly open
  if (blinkT < 0.5f && s_expr != DoraemonFace::HAPPY && s_expr != DoraemonFace::SMILING) {
    // Adjust pupil Y so it stays inside the visible area
    int16_t adjPupilCY = pupilCY + pupilOffY;
    int16_t adjPupilCX = pupilCX + pupilOffX;
    if (adjPupilCY + PUPIL_R > ecy + visRY - 2)
      adjPupilCY = ecy + visRY - 2 - PUPIL_R;
    if (adjPupilCY - PUPIL_R < ecy - visRY + 2)
      adjPupilCY = ecy - visRY + 2 + PUPIL_R;
    if (adjPupilCX + PUPIL_R > ecx + EYE_RX - 2)
      adjPupilCX = ecx + EYE_RX - 2 - PUPIL_R;
    if (adjPupilCX - PUPIL_R < ecx - EYE_RX + 2)
      adjPupilCX = ecx - EYE_RX + 2 + PUPIL_R;

    c->fillCircle(adjPupilCX, adjPupilCY, PUPIL_R, COL_BLACK);

    // Small white glint
    c->fillCircle(adjPupilCX + GLINT_OFF_X, adjPupilCY + GLINT_OFF_Y, GLINT_R, COL_WHITE);
  }

  // When fully closed (or happy), draw a horizontal line or arc for the closed eyelid
  if (blinkT > 0.9f || s_expr == DoraemonFace::HAPPY || s_expr == DoraemonFace::SMILING) {
    if (s_expr == DoraemonFace::HAPPY || s_expr == DoraemonFace::SMILING) {
      // Happy eyes: draw an upward arc (like ^_^)
      drawSmileArc(c, ecx, ecy + 10, EYE_RX - 4, EYE_RY / 2, 210.0f, 330.0f, COL_BLACK, 2);
    } else {
      c->drawFastHLine(ecx - EYE_RX, ecy, EYE_RX * 2, COL_BLACK);
      c->drawFastHLine(ecx - EYE_RX, ecy - 1, EYE_RX * 2, COL_BLACK);
      c->drawFastHLine(ecx - EYE_RX, ecy + 1, EYE_RX * 2, COL_BLACK);
    }
  }

  // Tears for crying expression
  if (s_expr == DoraemonFace::CRYING) {
    c->fillCircle(ecx - 5, ecy + EYE_RY + 10, 4, 0x051D); // light blue tear
    c->fillTriangle(ecx - 5, ecy + EYE_RY, ecx - 9, ecy + EYE_RY + 10, ecx - 1, ecy + EYE_RY + 10, 0x051D);
    c->fillCircle(ecx + 8, ecy + EYE_RY + 15, 3, 0x051D); // another tear
    c->fillTriangle(ecx + 8, ecy + EYE_RY + 5, ecx + 5, ecy + EYE_RY + 15, ecx + 11, ecy + EYE_RY + 15, 0x051D);
  }
}

// =====================================================
// Full Doraemon face rendering
// =====================================================
static void drawDoraemonFace(lgfx::LGFX_Sprite* c, float blinkT, float mouthOpenT,
                             int16_t pupilOffX, int16_t pupilOffY) {
  // 1. Background blue circle
  c->fillScreen(COL_BLACK);
  c->fillCircle(CX, CY, FACE_R, COL_BLUE);

  // 2. White muzzle area
  c->fillEllipse(CX, MUZZLE_CY, MUZZLE_RX, MUZZLE_RY, COL_WHITE);

  // 3. Collar (red band)
  c->fillRect(CX - MUZZLE_RX + 8, COLLAR_Y, (MUZZLE_RX - 8) * 2, COLLAR_H, COL_COLLAR);

  // 4. Bell
  c->fillCircle(BELL_CX, BELL_CY, BELL_R + 1, COL_BLACK);  // outline
  c->fillCircle(BELL_CX, BELL_CY, BELL_R, COL_YELLOW);
  c->drawFastHLine(BELL_CX - BELL_R + 2, BELL_CY, (BELL_R - 2) * 2, COL_BLACK);
  c->fillCircle(BELL_CX, BELL_CY + 2, 2, COL_BLACK);  // bell slot

  // 5. Eyes (with blink)
  drawEye(c, EYE_L_CX, PUPIL_L_CX, EYE_CY, PUPIL_CY, blinkT, pupilOffX, pupilOffY);
  drawEye(c, EYE_R_CX, PUPIL_R_CX, EYE_CY, PUPIL_CY, blinkT, pupilOffX, pupilOffY);

  // 6. Nose (red circle)
  c->fillCircle(NOSE_CX, NOSE_CY, NOSE_R + 1, COL_BLACK);  // outline
  c->fillCircle(NOSE_CX, NOSE_CY, NOSE_R, COL_RED);
  // Small highlight
  c->fillCircle(NOSE_CX - 3, NOSE_CY - 3, 3, COL_NOSE_HI);

  // 7. Mouth — blend between smile and open mouth
  // Always draw the vertical line from nose
  c->drawFastVLine(CX, MOUTH_TOP_Y, MOUTH_BOT_Y - MOUTH_TOP_Y, COL_BLACK);
  c->drawFastVLine(CX - 1, MOUTH_TOP_Y, MOUTH_BOT_Y - MOUTH_TOP_Y, COL_BLACK);

  if (mouthOpenT < 0.45f) {
    // Smile arc (closed mouth)
    drawSmileArc(c, SMILE_CX, SMILE_CY_ARC, SMILE_RX, SMILE_RY,
                 10.0f, 170.0f, COL_BLACK, 1);
  } else {
    // Open mouth (ellipse) with tongue
    float t = (mouthOpenT - 0.45f) / 0.55f;
    if (t > 1.0f) t = 1.0f;
    int16_t ry = (int16_t)(MOUTH_OPEN_RY * t);
    if (ry < 6) ry = 6;
    // Mouth outline
    c->fillEllipse(MOUTH_OPEN_CX, MOUTH_OPEN_CY, MOUTH_OPEN_RX + 1, ry + 1, COL_BLACK);
    // Mouth fill (dark red)
    c->fillEllipse(MOUTH_OPEN_CX, MOUTH_OPEN_CY, MOUTH_OPEN_RX, ry, 0xC800);
    // Tongue
    c->fillEllipse(MOUTH_OPEN_CX, MOUTH_OPEN_CY + ry / 3,
                   TONGUE_RX, (int16_t)(TONGUE_RY * t), 0xF1A0);
  }

  // 8. Whiskers — 3 lines per side with gentle animation
  for (int i = -1; i <= 1; i++) {
    int16_t angleOff = i * WHISK_Y_SPREAD;
    // Calculate a small vertical offset for the whisker tip
    float whiskOffset = 3.0f * sinf(s_whiskPhase + (i * 0.5f));

    // Left whiskers
    c->drawLine(WHISK_BASE_X_L, WHISK_Y_MID + angleOff / 2,
                WHISK_START_X_L, WHISK_Y_MID + angleOff + (int16_t)whiskOffset, COL_BLACK);
    // Right whiskers
    c->drawLine(WHISK_BASE_X_R, WHISK_Y_MID + angleOff / 2,
                WHISK_START_X_R, WHISK_Y_MID + angleOff + (int16_t)whiskOffset, COL_BLACK);
  }

  // 8.5 Blush on cheeks if blushing
  if (s_blushing) {
    // Draw pink ellipses on left and right cheeks
    uint16_t colBlush = 0xFBEF; // Soft pinkish/red blush
    c->fillEllipse(CX - 50, CY - 5, 18, 10, colBlush);
    c->fillEllipse(CX + 50, CY - 5, 18, 10, colBlush);
  }

  // 9. Blue face outline on top to clean edge
  // (circle outline drawn last for crisp boundary)
  for (int t = 0; t < 2; t++) {
    c->drawCircle(CX, CY, FACE_R + t, COL_BLACK);
  }
}

// =====================================================
// Public API
// =====================================================
namespace DoraemonFace {

void begin() {
  s_active = true;  // keep Doraemon as default
  s_blinking = false;
  s_nextBlinkMs = 0;
  s_eyePhase = 0.0f;
  s_eyeOffX = 0;
  s_eyeOffY = 0;
  s_mouthPhase = 0.0f;
  s_mouthOpenT = 0.0f;
  s_whiskPhase = 0.0f;
  s_expr = NORMAL;
  s_blushing = false;
}

void activate() {
  s_active = true;
  s_blinking = false;
  s_nextBlinkMs = 0;  // blink soon after activation
}

void deactivate() {
  s_active = false;
  s_blinking = false;
}

bool isActive() {
  return s_active;
}

void update(uint32_t nowMs) {
  if (!s_active) return;

  // Eye movement: gentle drift using phase
  s_eyePhase += 0.02f; // speed
  if (s_eyePhase > 6.28318f) s_eyePhase -= 6.28318f;
  s_eyeOffX = (int16_t)(3.0f * cosf(s_eyePhase));
  s_eyeOffY = (int16_t)(2.0f * sinf(s_eyePhase * 1.3f));

  // Mouth animation: slow open/close cycle
  s_mouthPhase += 0.015f;
  if (s_mouthPhase > 6.28318f) s_mouthPhase -= 6.28318f;
  // Map sin to 0..1 with some bias (more time closed)
  float raw = (sinf(s_mouthPhase) + 1.0f) * 0.5f; // 0..1
  s_mouthOpenT = (raw > 0.2f) ? raw : 0.0f;

  // Whisker animation: gentle wiggle
  s_whiskPhase += 0.04f;
  if (s_whiskPhase > 6.28318f) s_whiskPhase -= 6.28318f;

  // Schedule next blink
  if (!s_blinking && nowMs >= s_nextBlinkMs) {
    s_blinking = true;
    s_blinkStartMs = nowMs;
  }

  // End blink
  if (s_blinking && (nowMs - s_blinkStartMs >= BLINK_TOTAL_MS)) {
    s_blinking = false;
    s_nextBlinkMs = nowMs + randRange(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
  }

  // Update blushing timer
  if (s_blushing && (nowMs - s_blushStartMs >= BLUSH_DURATION_MS)) {
    s_blushing = false;
  }
}

void setExpression(Expression expr) {
  s_expr = expr;
}

void handleTouch(int16_t x, int16_t y) {
  // Check if touch is near the cheeks (left or right side of the face)
  // Cheeks approximately at Y = 110..150, X = 40..80 (left) and 160..200 (right)
  if (y > 90 && y < 160) {
    if ((x > 30 && x < 90) || (x > 150 && x < 210)) {
      s_blushing = true;
      s_blushStartMs = millis();
    }
  }
}

bool renderFrame(lgfx::LGFX_Sprite* canvas, uint32_t nowMs) {
  if (!s_active) return false;

  // Compute blink interpolation (0 = open, 1 = closed)
  float blinkT = 0.0f;
  if (s_blinking) {
    uint32_t dt = nowMs - s_blinkStartMs;
    if (dt < BLINK_CLOSE_MS) {
      // Closing
      blinkT = (float)dt / (float)BLINK_CLOSE_MS;
    } else if (dt < BLINK_CLOSE_MS + BLINK_HOLD_MS) {
      // Fully closed
      blinkT = 1.0f;
    } else {
      // Reopening
      uint32_t openDt = dt - BLINK_CLOSE_MS - BLINK_HOLD_MS;
      blinkT = 1.0f - (float)openDt / (float)BLINK_OPEN_MS;
      if (blinkT < 0.0f) blinkT = 0.0f;
    }
  }

  // Force mouth open if happy or smiling
  float renderedMouthOpen = s_mouthOpenT;
  if (s_expr == HAPPY || s_expr == SMILING || s_blushing) {
    renderedMouthOpen = 1.0f; // wide open happy mouth
  }

  drawDoraemonFace(canvas, blinkT, renderedMouthOpen, s_eyeOffX, s_eyeOffY);
  return true;
}

} // namespace DoraemonFace
