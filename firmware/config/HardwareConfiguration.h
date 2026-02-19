#pragma once

#include <Arduino.h>

namespace HardwareConfiguration
{
    // Display configuration (FastLED NeoMatrix)
    static const uint8_t DISPLAY_PIN = 25; // Data pin

    // Physical tile size — standard 32x32 WS2812B panels
    static const uint16_t DISPLAY_TILE_PIXEL_W = 32;
    static const uint16_t DISPLAY_TILE_PIXEL_H = 32;

    // Tile arrangement: 4x2 grid of 32x32 tiles = 128x64 total
    static const uint8_t DISPLAY_TILES_X = 4;
    static const uint8_t DISPLAY_TILES_Y = 2;

    // Derived matrix dimensions: 128x64
    static const uint16_t DISPLAY_MATRIX_WIDTH  = DISPLAY_TILE_PIXEL_W * DISPLAY_TILES_X;
    static const uint16_t DISPLAY_MATRIX_HEIGHT = DISPLAY_TILE_PIXEL_H * DISPLAY_TILES_Y;
}
