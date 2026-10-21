#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <cstdint>

#define SSD1306_I2C_ADDR 0x3C

// compile with:
// g++ -O2 ssd1306.cpp -o oled
// run with: ./oled

class SSD1306 {
public:
    SSD1306() : fd(-1) {
        memset(buffer, 0, sizeof(buffer));
    }

    ~SSD1306() {
        if (fd >= 0)
            close(fd);
    }

    bool begin() {
        fd = open("/dev/i2c-1", O_RDWR);
        if (fd < 0) {
            perror("open");
            return false;
        }

        if (ioctl(fd, I2C_SLAVE, SSD1306_I2C_ADDR) < 0) {
            perror("ioctl");
            return false;
        }

        const uint8_t init[] = {
            0xAE,       // Display OFF
            0x20, 0x00, // Horizontal addressing mode
            0xB0,
            0xC8,
            0x00,
            0x10,
            0x40,
            0x81, 0x7F,
            0xA1,
            0xA6,
            0xA8, 0x3F,
            0xA4,
            0xD3, 0x00,
            0xD5, 0x80,
            0xD9, 0xF1,
            0xDA, 0x12,
            0xDB, 0x40,
            0x8D, 0x14,
            0xAF        // Display ON
        };

        for (uint8_t c : init)
            command(c);

        clear();
        display();

        return true;
    }

    void clear() {
        memset(buffer, 0, sizeof(buffer));
    }

    void display() {
        for (int page = 0; page < 8; page++) {
            command(0xB0 + page);
            command(0x00);
            command(0x10);

            uint8_t data[129];
            data[0] = 0x40;

            memcpy(&data[1], &buffer[page * 128], 128);

            write(fd, data, sizeof(data));
        }
    }

    void drawChar(int x, int page, char c) {
        if (c < 32 || c > 127)
            c = '?';

        const uint8_t* glyph = font[c - 32];

        for (int i = 0; i < 5; i++) {
            buffer[page * 128 + x + i] = glyph[i];
        }

        buffer[page * 128 + x + 5] = 0x00; // spacing
    }

    void drawString(int x, int page, const char* str) {
        while (*str) {
            drawChar(x, page, *str++);
            x += 6;
        }
    }

    void rotateFramebuffer90(const uint8_t src[1024], uint8_t dst[1024])
    {
        // Clear destination buffer
        memset(dst, 0, 1024);

        for (int y = 0; y < 64; y++)
        {
            for (int x = 0; x < 128; x++)
            {
                // Read pixel from source
                int srcByte = x + (y / 8) * 128;
                bool pixel = src[srcByte] & (1 << (y & 7));

                if (pixel)
                {
                    // Rotate clockwise
                    int newX = 63 - y;
                    int newY = x;

                    // Write pixel into destination
                    int dstByte = newX + (newY / 8) * 128;
                    dst[dstByte] |= (1 << (newY & 7));
                }
            }
        }
    }

private:
    int fd;
    uint8_t buffer[1024];

    void command(uint8_t cmd) {
        uint8_t buf[2] = {0x00, cmd};
        write(fd, buf, 2);
    }

    // 5x7 font (ASCII 32-127)
    static const uint8_t font[96][5];
};

const uint8_t SSD1306::font[96][5] = {
    {0,0,0,0,0},                 // space
    {0,0,95,0,0},                // !
    {0,7,0,7,0},                 // "
    {20,127,20,127,20},          // #
    {36,42,127,42,18},           // $
    {35,19,8,100,98},            // %
    {54,73,85,34,80},            // &
    {0,5,3,0,0},                 // '
    {0,28,34,65,0},              // (
    {0,65,34,28,0},              // )
    {20,8,62,8,20},              // *
    {8,8,62,8,8},                // +
    {0,80,48,0,0},               // ,
    {8,8,8,8,8},                 // -
    {0,96,96,0,0},               // .
    {32,16,8,4,2},               // /

    {62,81,73,69,62},            // 0
    {0,66,127,64,0},             // 1
    {66,97,81,73,70},            // 2
    {33,65,69,75,49},            // 3
    {24,20,18,127,16},           // 4
    {39,69,69,69,57},            // 5
    {60,74,73,73,48},            // 6
    {1,113,9,5,3},               // 7
    {54,73,73,73,54},            // 8
    {6,73,73,41,30},             // 9

    {0,54,54,0,0},               // :
    {0,86,54,0,0},               // ;
    {8,20,34,65,0},              // <
    {20,20,20,20,20},            // =
    {0,65,34,20,8},              // >
    {2,1,81,9,6},                // ?
    {50,73,121,65,62},           // @

    {126,17,17,17,126},          // A
    {127,73,73,73,54},           // B
    {62,65,65,65,34},            // C
    {127,65,65,34,28},           // D
    {127,73,73,73,65},           // E
    {127,9,9,9,1},               // F
    {62,65,73,73,122},           // G
    {127,8,8,8,127},             // H
    {0,65,127,65,0},             // I
    {32,64,65,63,1},             // J
    {127,8,20,34,65},            // K
    {127,64,64,64,64},           // L
    {127,2,12,2,127},            // M
    {127,4,8,16,127},            // N
    {62,65,65,65,62},            // O
    {127,9,9,9,6},               // P
    {62,65,81,33,94},            // Q
    {127,9,25,41,70},            // R
    {70,73,73,73,49},            // S
    {1,1,127,1,1},               // T
    {63,64,64,64,63},            // U
    {31,32,64,32,31},            // V
    {63,64,56,64,63},            // W
    {99,20,8,20,99},             // X
    {7,8,112,8,7},               // Y
    {97,81,73,69,67},            // Z (90)

    {4,2,31,2,4},      // [ bracket replaced by up arrow
    {0,0,0,0,0},       // backslash
    {4,8,31,8,4},      // ] bracket replaced by down arrow
    {28,54,54,28,28},  // ^ replaced by warning
    {0,0,0,0,0},       // _
    {6,9,9,6,0},       // ` replaced by degree symbol

    {0, 32, 84, 84, 120},   // a (97)
    {127, 72, 68, 68, 56},  // b
    {56, 68, 68, 68, 32},   // c
    {56, 68, 68, 72, 127},  // d
    {56, 84, 84, 84, 24},   // e
    {8, 126, 9, 1, 2},      // f
    {12, 82, 82, 82, 62},   // g
    {127, 8, 4, 4, 120},    // h
    {0, 68, 125, 64, 0},    // i
    {32, 64, 68, 61, 0},    // j
    {127, 16, 40, 68, 0},   // k
    {0, 65, 127, 64, 0},    // l
    {124, 4, 24, 4, 120},   // m
    {124, 8, 4, 4, 120},    // n
    {56, 68, 68, 68, 56},   // o
    {126, 18, 18, 18, 12},  // p
    {12, 18, 18, 18, 126},  // q
    {124, 8, 4, 4, 8},      // r
    {72, 84, 84, 84, 32},   // s
    {4, 63, 68, 64, 32},    // t
    {60, 64, 64, 32, 124},  // u
    {28, 32, 64, 32, 28},   // v
    {60, 64, 48, 64, 60},   // w
    {68, 40, 16, 40, 68},   // x
    {12, 80, 80, 80, 60},   // y
    {68, 100, 84, 76, 68},  // z (122)
};

// Main program
int main() {
    SSD1306 oled;

    if (!oled.begin()) {
        std::cerr << "Failed to initialize display\n";
        return 1;
    }

    oled.clear();
  
    // will be replaced by dynamic attributes in next version
    oled.drawString(10, 1, "Xerxes-07");
    oled.drawString(10, 3, "192.168.1.167");
  
    // ` is shown as degree symbol in display
    oled.drawString(10, 5, "55.9`C");
  
    // brackets are shown as arrows in display
    oled.drawString(10, 7, "[23 ]123 KB/s");
    oled.display();

    return 0;
}
