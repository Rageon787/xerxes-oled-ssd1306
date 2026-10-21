#pragma once

#include <string>
#include <vector>

// Minimal QR code encoder for small OLED screens.
//
// Supports QR versions 1-3 (21x21 to 29x29 modules) in byte mode with
// error correction level L. That is the largest size that still fits on a
// 64 pixel tall screen at 2 pixels per module.
//
// Reference: ISO/IEC 18004, and https://www.nayuki.io/page/creating-a-qr-code-step-by-step

// Longest text that fits in a version 3, level L code.
const int QR_MAX_TEXT_BYTES = 53;

struct QrCode {
    int size = 0;                              // width and height in modules
    std::vector<std::vector<bool>> modules;    // modules[y][x], true = dark

    bool isDark(int x, int y) const { return modules[y][x]; }
};

// Encodes text into a QR code. Returns false if the text is too long.
// mask = -1 picks the best mask automatically (normal use); 0-7 forces one,
// which is only useful for testing against other encoders.
bool encodeQr(const std::string& text, QrCode& out, int mask = -1);
