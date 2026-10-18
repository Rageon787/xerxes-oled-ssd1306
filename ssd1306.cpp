#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <cstdint>

// Added for live system info
#include <arpa/inet.h>   // inet_ntop
#include <ifaddrs.h>     // getifaddrs
#include <net/if.h>      // IFF_UP, IFF_LOOPBACK
#include <netinet/in.h>  // sockaddr_in
#include <chrono>
#include <csignal>
#include <cstdio>        // snprintf
#include <cstdlib>       // labs
#include <fstream>
#include <sstream>
#include <string>

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

    // Turn the panel off (0xAE). Its RAM keeps the last image.
    void displayOff() {
        command(0xAE);
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

// ===================================================================
// Live system information (FR-3)
// Everything below replaces the old "Main program" section.
// ===================================================================

// Set to 0 by the signal handler; the main loop checks it.
volatile std::sig_atomic_t running = 1;

void handleSignal(int) {
    running = 0;   // only set the flag - no drawing or printing in here
}

// Hostname 
std::string getHostname() {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0)
        return "unknown";
    buf[sizeof(buf) - 1] = '\0';   // gethostname may not terminate on truncation
    return buf;
}

// IPv4 address 
// Returns the first IPv4 address on an interface that is up and is not
// loopback (127.0.0.1). The interface name (e.g. "eth0") is written to
// ifaceOut so the traffic counters can use the same interface.
std::string getIPv4(std::string& ifaceOut) {
    struct ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0)
        return "No IP";

    std::string result = "No IP";
    ifaceOut.clear();

    for (struct ifaddrs* ifa = list; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)              continue; // no address
        if (ifa->ifa_addr->sa_family != AF_INET)   continue; // not IPv4
        if (!(ifa->ifa_flags & IFF_UP))            continue; // interface down
        if (ifa->ifa_flags & IFF_LOOPBACK)         continue; // 127.0.0.1

        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
        if (inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip)) != nullptr) {
            result = ip;
            ifaceOut = ifa->ifa_name;
            break;   // take the first match
        }
    }

    freeifaddrs(list);   // getifaddrs allocates memory - must free it
    return result;
}

// DNS server
// Returns the first "nameserver" entry in /etc/resolv.conf.
std::string getDNS() {
    std::ifstream file("/etc/resolv.conf");
    if (!file)
        return "-";

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream words(line);
        std::string key, value;
        words >> key >> value;
        if (key == "nameserver" && !value.empty())
            return value;
    }
    return "-";
}

// CPU temperature 
// The file holds millidegrees, e.g. 55900 = 55.9 C.
// The ` character is drawn as a degree symbol by this driver's font.
std::string getCpuTemp() {
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    long milli;
    if (!(file >> milli))
        return "--.-`C";

    char buf[32];
    snprintf(buf, sizeof(buf), "%ld.%ld`C", milli / 1000, std::labs(milli % 1000) / 100);
    return buf;
}

//  Network traffic
// Reads a byte counter such as /sys/class/net/eth0/statistics/rx_bytes.
// Returns false if the file can't be read (e.g. interface disappeared).
bool readCounter(const std::string& iface, const char* name, unsigned long long& out) {
    std::ifstream file("/sys/class/net/" + iface + "/statistics/" + name);
    return static_cast<bool>(file >> out);
}

// The counters are running totals since boot, so a rate needs two
// readings: (bytes now - bytes before) / seconds between them.
struct TrafficMeter {
    std::string iface;
    unsigned long long lastRx = 0, lastTx = 0;
    std::chrono::steady_clock::time_point lastTime;
    bool havePrevious = false;

    // Updates rxKBs / txKBs (kilobytes per second). Returns false on the
    // first call or if the counters can't be read.
    bool update(const std::string& currentIface, double& rxKBs, double& txKBs) {
        rxKBs = txKBs = 0.0;

        unsigned long long rx, tx;
        if (currentIface.empty() ||
            !readCounter(currentIface, "rx_bytes", rx) ||
            !readCounter(currentIface, "tx_bytes", tx)) {
            havePrevious = false;
            return false;
        }

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        // First reading, or the interface changed: just store a baseline.
        if (!havePrevious || currentIface != iface) {
            iface = currentIface;
            lastRx = rx; lastTx = tx; lastTime = now;
            havePrevious = true;
            return false;
        }

        double seconds = std::chrono::duration<double>(now - lastTime).count();
        if (seconds > 0.0 && rx >= lastRx && tx >= lastTx) {
            rxKBs = (rx - lastRx) / 1024.0 / seconds;
            txKBs = (tx - lastTx) / 1024.0 / seconds;
        }

        lastRx = rx; lastTx = tx; lastTime = now;
        return true;
    }
};

// --- Drawing helper -------------------------------------------------
// Draws one line of text at the left edge of a page, cut to 21
// characters. drawChar() has no bounds check, so a longer string would
// write past the end of the row (or past the end of the buffer on page 7).
void drawLine(SSD1306& oled, int page, const std::string& text) {
    const size_t maxChars = 21;   // 128 px / 6 px per character
    oled.drawString(0, page, text.substr(0, maxChars).c_str());
}

// --- Main program ---------------------------------------------------
int main() {
    std::signal(SIGINT,  handleSignal);   // Ctrl+C
    std::signal(SIGTERM, handleSignal);   // kill / systemctl stop

    SSD1306 oled;
    if (!oled.begin()) {
        std::cerr << "Failed to initialize display\n";
        return 1;
    }

    TrafficMeter traffic;

    while (running) {
        std::string iface;
        std::string hostname = getHostname();
        std::string ip       = getIPv4(iface);
        std::string dns      = getDNS();
        std::string temp     = getCpuTemp();

        double rxKBs, txKBs;
        traffic.update(iface, rxKBs, txKBs);

        // [ is drawn as an up arrow (sent), ] as a down arrow (received)
        char trafficLine[32];
        snprintf(trafficLine, sizeof(trafficLine), "[%.0f ]%.0f KB/s", txKBs, rxKBs);

        oled.clear();
        drawLine(oled, 0, hostname);
        drawLine(oled, 2, "IP  " + ip);
        drawLine(oled, 3, "DNS " + dns);
        drawLine(oled, 5, temp);
        drawLine(oled, 7, trafficLine);
        oled.display();

        sleep(1);   // returns early if a signal arrives
    }

    // Only reached after SIGINT / SIGTERM: leave the screen blank and off.
    oled.clear();
    oled.display();
    oled.displayOff();
    return 0;
}
