#pragma once

#include <cstdint>
#include <string>

// Driver for a 128x64 SSD1306 OLED connected over I2C.
//
// All drawing goes into an in-memory framebuffer. Nothing appears on the
// screen until display() sends that framebuffer to the panel.
//
// Framebuffer layout (same as the SSD1306's own RAM): the screen is split
// into 8 horizontal "pages" of 8 pixel rows. Each byte is one column of
// one page, with bit 0 as the top pixel.
//   byte index = x + (y / 8) * WIDTH,   bit = y % 8
class SSD1306 {
public:
    static const int WIDTH = 128;
    static const int HEIGHT = 64;
    static const int CHAR_WIDTH = 6;    // 5 px glyph + 1 px gap
    static const int CHAR_HEIGHT = 8;   // 7 px glyph + 1 px gap

    ~SSD1306();

    // Opens the I2C device (e.g. "/dev/i2c-1") and initialises the panel.
    bool begin(const std::string& device, int address);

    // Sends the framebuffer to the panel. Does nothing if begin() was not
    // called, which is what preview mode relies on.
    void display();

    // Draws the framebuffer in the terminal, for testing without hardware.
    void printToTerminal() const;

    void setBrightness(uint8_t level);   // 0 = dimmest, 255 = brightest
    void setFlipped(bool flipped);       // rotate the image 180 degrees
    void powerOff();                     // panel off; RAM keeps the last image

    // ---- Drawing (framebuffer only) ----
    // Anything outside the screen is silently clipped.
    void clear();
    void setPixel(int x, int y, bool on);
    bool getPixel(int x, int y) const;
    void fillRect(int x, int y, int w, int h, bool on);
    void drawHLine(int x, int y, int w);

    // Draws text with its top-left corner at (x, y). scale 2 doubles the
    // size of each character, and so on. on = false draws dark text,
    // for use on a lit background.
    void drawText(int x, int y, const std::string& text, int scale = 1, bool on = true);

private:
    int fd = -1;
    uint8_t buffer[WIDTH * HEIGHT / 8] = {};

    void command(uint8_t cmd);
    void drawChar(int x, int y, char c, int scale, bool on);
};
