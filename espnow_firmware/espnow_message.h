// espnow_message.h
// Shared message format between sword boards and hub.
// Copy this exact file into both the sword and hub sketch folders
// so the struct layout always matches on both ends.

#pragma once
#include <stdint.h>

typedef struct {
  uint8_t swordId;        // 1 or 2 — which sword sent this
  float impactMagnitude;  // peak accel magnitude at moment of hit (g's)
  uint32_t timestampMs;   // millis() on the sending board, used to match clashes
} HitMessage;
