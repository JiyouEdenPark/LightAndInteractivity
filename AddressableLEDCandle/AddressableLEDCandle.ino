/*
 * NeoPixel Jewel 7 (RGBW) - Candle / 캔들
 * Pixel 0 = wick, 1~6 = flame ring swaying in wind (gust / calm / normal at random)
 * 0번 = 심지, 1~6번 = 바람에 흔들리는 불꽃 (질풍/잔바람/보통 랜덤)
 */

#include <Adafruit_NeoPixel.h>

#define PIN 9
#define NUMPIXELS 7

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRBW + NEO_KHZ800);

int wickBrightness = 200;   // Wick brightness (0–255) / 0번 심지 밝기
int flameBrightness = 30;  // Ring flame brightness / 1~6번 불꽃 밝기
// Wick color (candle glow: warm amber/orange, 0–255) / 0번 심지 색
int wickR = 220;
int wickG = 85;
int wickB = 0;
int wickW = 40;   // A bit of W for softer glow / W 채널 살짝 섞어 부드럽게
int holdMin = 70;   // Hold time min (ms), normal band / 켜져 있는 시간 최소 (ms)
int holdMax = 220;  // Hold time max (ms) / 켜져 있는 시간 최대 (ms)
int gapMin = 50;   // Gap min (ms) before next transition / 텀 최소 (ms)
int gapMax = 180;  // Gap max (ms) / 텀 최대 (ms)
float fadeOutSpeed;
float fadeInSpeed;

int currentFlame = 1;
int targetFlame = 2;
float fadeOutProgress = 0.0;
float fadeInProgress = 0.0;
unsigned long holdUntil = 0;
unsigned long gapUntil = 0;
int nextGapLen = 100;

// Wind-based brightness (dimmer in wind, gap between dim/bright) / 바람에 따른 밝기
float targetBrightnessMul = 1.0;
float currentBrightnessMul = 1.0;
const float brightnessSmooth = 0.04;
const float brightnessMin = 0.22;
// Flicker: brightness wobble over time, independent of wind / 일렁임 (바람과 별개)
const float flickerMin = 0.65;  // Flicker min / 일렁임 최소
const float flickerMax = 1.0;   // Flicker max / 일렁임 최대

// Pick random wind band: gust / normal / calm — natural candle feel
// 바람 세기 랜덤: 질풍 / 보통 / 잔바람
void pickNextTiming() {
  int wind = random(0, 100);
  int holdLen, gapLen;

  if (wind < 22) {
    // Gust: quick change, dimmer (strong wind) / 질풍: 확확, 어두워짐
    holdLen = random(25, 75);
    gapLen = random(25, 85);
    fadeOutSpeed = 0.10 + (float)random(0, 55) / 1000.0;
    fadeInSpeed = 0.06 + (float)random(0, 45) / 1000.0;
    targetBrightnessMul =
        0.32 + (float)random(0, 22) / 100.0;  // 0.32~0.54 dimmer
  } else if (wind < 78) {
    // Normal: wide dim–bright range / 보통: 어두운~밝은 넓게
    holdLen = random(holdMin, holdMax + 1);
    gapLen = random(gapMin, gapMax + 1);
    fadeOutSpeed = 0.032 + (float)random(0, 42) / 1000.0;
    fadeInSpeed = 0.018 + (float)random(0, 35) / 1000.0;
    targetBrightnessMul =
        0.52 + (float)random(0, 48) / 100.0;  // 0.52~1.0
  } else {
    // Calm: stay bright / 잔바람: 밝게 유지
    holdLen = random(140, 380);
    gapLen = random(120, 350);
    fadeOutSpeed = 0.014 + (float)random(0, 22) / 1000.0;
    fadeInSpeed = 0.008 + (float)random(0, 18) / 1000.0;
    targetBrightnessMul = 0.82 + (float)random(0, 20) / 100.0;  // 0.82~1.02
  }
  if (targetBrightnessMul > 1.0)
    targetBrightnessMul = 1.0;
  if (targetBrightnessMul < brightnessMin)
    targetBrightnessMul = brightnessMin;

  holdUntil = millis() + (unsigned long)holdLen;
  nextGapLen = gapLen;
  gapUntil = 0;
}

void setup() {
  pixels.begin();
  pixels.clear();
  pixels.show();
  randomSeed(analogRead(0));
  pickNextTiming();
}

void loop() {
  unsigned long now = millis();

  // When fade done: pick next pixel, start new hold / 페이드 끝 → 다음 픽셀, 유지 시작
  if (fadeOutProgress >= 1.0 && fadeInProgress >= 1.0) {
    currentFlame = targetFlame;
    do {
      targetFlame = random(1, 7);
    } while (targetFlame == currentFlame);
    fadeOutProgress = 0.0;
    fadeInProgress = 0.0;
    gapUntil = 0;
    pickNextTiming();
  }

  // After hold: start gap (keep current pixel on for nextGapLen ms)
  // 유지 끝 → 텀 시작 (전 픽셀 유지, nextGapLen ms)
  if (now >= holdUntil && gapUntil == 0 &&
      (fadeOutProgress < 0.01 && fadeInProgress < 0.01)) {
    gapUntil = now + (unsigned long)nextGapLen;
  }

  // Wind-based brightness smooth step / 바람 밝기 스무스 보간
  currentBrightnessMul +=
      (targetBrightnessMul - currentBrightnessMul) * brightnessSmooth;
  if (currentBrightnessMul < brightnessMin)
    currentBrightnessMul = brightnessMin;
  if (currentBrightnessMul > 1.0)
    currentBrightnessMul = 1.0;

  // Flicker: time-based brightness wobble (candle feel, independent of wind)
  // 일렁임: 시간에 따른 밝기 오르내림
  float t = now / 1000.0;
  float f1 = 0.5 + 0.5 * sin(t * 2.7);
  float f2 = 0.5 + 0.5 * sin(t * 1.15 + 1.3);
  float flickerMul = flickerMin + (flickerMax - flickerMin) * f1 * f2;

  // Run fade only after gap (current pixel out, next pixel in) / 텀 끝난 뒤에만 페이드
  if (gapUntil > 0 && now >= gapUntil) {
    if (fadeOutProgress < 1.0) {
      fadeOutProgress += fadeOutSpeed;
      if (fadeOutProgress > 1.0)
        fadeOutProgress = 1.0;
    }
    if (fadeInProgress < 1.0) {
      fadeInProgress += fadeInSpeed;
      if (fadeInProgress > 1.0)
        fadeInProgress = 1.0;
    }
  }

  // Pixel 0: wick color (RGBW) × brightness × flicker / 0번 심지: 색 × 밝기 × 일렁임
  float wickMul = (wickBrightness / 255.0) * currentBrightnessMul * flickerMul;
  int wr = (int)(wickR * wickMul);
  int wg = (int)(wickG * wickMul);
  int wb = (int)(wickB * wickMul);
  int ww = (int)(wickW * wickMul);
  if (wr > 255) wr = 255; if (wg > 255) wg = 255;
  if (wb > 255) wb = 255; if (ww > 255) ww = 255;
  if (wr < 0) wr = 0; if (wg < 0) wg = 0; if (wb < 0) wb = 0; if (ww < 0) ww = 0;
  pixels.setPixelColor(0, pixels.Color(wr, wg, wb, ww));

  // Pixels 1–6: wind brightness × flicker (one lit, crossfade) / 1~6번: 밝기 × 일렁임
  for (int i = 1; i <= 6; i++) {
    int b = 0;
    if (gapUntil > 0 && now < gapUntil) {
      if (i == currentFlame)
        b = (int)(flameBrightness * currentBrightnessMul * flickerMul);
    } else {
      if (i == currentFlame)
        b = (int)(flameBrightness * (1.0 - fadeOutProgress) *
                  currentBrightnessMul * flickerMul);
      else if (i == targetFlame)
        b = (int)(flameBrightness * fadeInProgress * currentBrightnessMul *
                  flickerMul);
    }
    if (b > 255)
      b = 255;
    if (b < 0)
      b = 0;
    pixels.setPixelColor(i, pixels.Color(0, 0, 0, b));
  }

  pixels.show();
  delay(30);
}
