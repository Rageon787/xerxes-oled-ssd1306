// Xerxes Pi OLED display program.
//
// Shows either live system information or a QR code on a 128x64 SSD1306
// OLED. The admin software controls what is shown through the command-line
// options below. Run with --help to see them.

#include "qrcode.h"
#include "ssd1306.h"
#include "sysinfo.h"

#include <unistd.h>   // sleep

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------

struct Options {
    std::string mode = "info";                  // "info" or "qr"
    std::string fields = "hostname,ip,dns,temp,uptime,traffic";
    std::string message;                        // extra line on the info screen
    std::string qrText = "http://{ip}";         // {ip} and {hostname} are filled in
    std::string qrCaption = "Scan to open admin panel";
    int brightness = 127;
    bool flip = false;
    std::string device = "/dev/i2c-1";
    int address = 0x3C;
    bool preview = false;                       // draw in the terminal, not on the OLED
    bool once = false;                          // draw one frame, then exit
};

void printUsage(const char* program) {
    printf(
        "Usage: %s [options]\n"
        "\n"
        "What to show:\n"
        "  --mode info|qr       info = live system information (default)\n"
        "                       qr   = a QR code with a caption\n"
        "  --fields LIST        info lines to show, comma separated, in order.\n"
        "                       Choose from: hostname,ip,dns,temp,uptime,traffic\n"
        "  --message TEXT       extra line at the bottom of the info screen\n"
        "  --qr-text TEXT       text for the QR code (max %d bytes).\n"
        "                       {ip} and {hostname} are replaced with live values.\n"
        "                       Default: http://{ip}\n"
        "  --qr-caption TEXT    text shown next to the QR code\n"
        "\n"
        "Display:\n"
        "  --brightness 0-255   default 127\n"
        "  --flip               rotate the image 180 degrees\n"
        "  --device PATH        I2C device, default /dev/i2c-1\n"
        "  --address ADDR       I2C address, default 0x3C\n"
        "\n"
        "Testing:\n"
        "  --preview            draw in the terminal instead of on the OLED\n"
        "  --once               draw one frame and exit\n",
        program, QR_MAX_TEXT_BYTES);
}

// Returns false (after printing why) if the arguments are invalid.
bool parseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Options without a value.
        if (arg == "--help" || arg == "-h") { printUsage(argv[0]); exit(0); }
        if (arg == "--flip")    { opt.flip = true;    continue; }
        if (arg == "--preview") { opt.preview = true; continue; }
        if (arg == "--once")    { opt.once = true;    continue; }

        // Every other option takes one value.
        if (i + 1 >= argc) {
            fprintf(stderr, "Missing value for %s\n", arg.c_str());
            return false;
        }
        std::string value = argv[++i];

        if      (arg == "--mode")       opt.mode = value;
        else if (arg == "--fields")     opt.fields = value;
        else if (arg == "--message")    opt.message = value;
        else if (arg == "--qr-text")    opt.qrText = value;
        else if (arg == "--qr-caption") opt.qrCaption = value;
        else if (arg == "--device")     opt.device = value;
        else if (arg == "--brightness") opt.brightness = atoi(value.c_str());
        else if (arg == "--address")    opt.address = strtol(value.c_str(), nullptr, 0);   // accepts 0x3C
        else {
            fprintf(stderr, "Unknown option %s (try --help)\n", arg.c_str());
            return false;
        }
    }

    if (opt.mode != "info" && opt.mode != "qr") {
        fprintf(stderr, "--mode must be info or qr\n");
        return false;
    }
    if (opt.brightness < 0 || opt.brightness > 255) {
        fprintf(stderr, "--brightness must be between 0 and 255\n");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator))
        parts.push_back(part);
    return parts;
}

void replaceAll(std::string& text, const std::string& from, const std::string& to) {
    for (size_t pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos + to.size()))
        text.replace(pos, from.size(), to);
}

// Breaks text into lines of at most maxChars, splitting at spaces.
// Words longer than a line are cut.
std::vector<std::string> wrapText(const std::string& text, size_t maxChars) {
    std::vector<std::string> lines;
    std::string line;
    for (std::string word : split(text, ' ')) {
        if (word.empty())
            continue;
        word = word.substr(0, maxChars);
        if (line.empty())
            line = word;
        else if (line.size() + 1 + word.size() <= maxChars)
            line += " " + word;
        else {
            lines.push_back(line);
            line = word;
        }
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

// ---------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------

// Live values read once per frame, shared by both screens.
struct SystemInfo {
    std::string hostname, ip, iface, dns, temp, uptime;
    double rxKBs = 0, txKBs = 0;
};

void drawSplash(SSD1306& oled) {
    oled.clear();
    std::string title = "Xerxes Pi";
    int titleWidth = title.size() * SSD1306::CHAR_WIDTH * 2;
    oled.drawText((SSD1306::WIDTH - titleWidth) / 2, 18, title, 2);

    std::string subtitle = "starting...";
    int subtitleWidth = subtitle.size() * SSD1306::CHAR_WIDTH;
    oled.drawText((SSD1306::WIDTH - subtitleWidth) / 2, 44, subtitle);
}

// One line of text per row, in the order given by --fields.
void drawInfoScreen(SSD1306& oled, const Options& opt, const SystemInfo& info) {
    std::vector<std::string> lines;
    for (const std::string& field : split(opt.fields, ',')) {
        if      (field == "hostname") lines.push_back(info.hostname);
        else if (field == "ip")       lines.push_back("IP  " + info.ip);
        else if (field == "dns")      lines.push_back("DNS " + info.dns);
        else if (field == "temp")     lines.push_back("CPU " + info.temp);
        else if (field == "uptime")   lines.push_back("Up  " + info.uptime);
        else if (field == "traffic") {
            // The font draws [ as an up arrow (sent) and ] as a down arrow (received).
            char buf[32];
            snprintf(buf, sizeof(buf), "[%.0f ]%.0f KB/s", info.txKBs, info.rxKBs);
            lines.push_back(buf);
        }
    }
    if (!opt.message.empty())
        lines.push_back(opt.message);

    const size_t maxRows = SSD1306::HEIGHT / SSD1306::CHAR_HEIGHT;   // 8
    for (size_t row = 0; row < lines.size() && row < maxRows; row++)
        oled.drawText(0, row * SSD1306::CHAR_HEIGHT, lines[row]);
}

// QR code in a lit 64x64 square on the left, caption on the right.
// The square is lit because scanners expect dark modules on a light
// background, and on an OLED "light" means pixels switched on.
void drawQrScreen(SSD1306& oled, const Options& opt, const SystemInfo& info) {
    std::string text = opt.qrText;
    replaceAll(text, "{ip}", info.ip);
    replaceAll(text, "{hostname}", info.hostname);

    QrCode qr;
    if (!encodeQr(text, qr)) {
        oled.drawText(0, 0, "QR text too long");
        oled.drawText(0, 16, "Max " + std::to_string(QR_MAX_TEXT_BYTES) + " characters");
        return;
    }

    const int area = SSD1306::HEIGHT;   // 64x64 square
    oled.fillRect(0, 0, area, area, true);

    // Largest whole-pixel size per module that leaves at least one module of
    // light border (the "quiet zone") on each side.
    int scale = area / (qr.size + 2);
    int offset = (area - qr.size * scale) / 2;
    for (int y = 0; y < qr.size; y++)
        for (int x = 0; x < qr.size; x++)
            if (qr.isDark(x, y))
                oled.fillRect(offset + x * scale, offset + y * scale, scale, scale, false);

    // Caption, wrapped to fit the right half and centred vertically.
    const int captionX = area + 4;
    const size_t charsPerLine = (SSD1306::WIDTH - captionX) / SSD1306::CHAR_WIDTH;
    std::vector<std::string> lines = wrapText(opt.qrCaption, charsPerLine);
    int y = (SSD1306::HEIGHT - (int)lines.size() * SSD1306::CHAR_HEIGHT) / 2;
    for (const std::string& line : lines) {
        oled.drawText(captionX, y, line);
        y += SSD1306::CHAR_HEIGHT;
    }
}

// ---------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------

// Set to 0 by Ctrl+C or "systemctl stop"; the main loop checks it.
volatile std::sig_atomic_t running = 1;

void handleSignal(int) {
    running = 0;   // only set the flag: no drawing or printing in a signal handler
}

void show(SSD1306& oled, const Options& opt) {
    if (opt.preview) {
        printf("\033[H\033[2J");   // clear the terminal so frames replace each other
        oled.printToTerminal();
    } else {
        oled.display();
    }
}

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt))
        return 1;

    std::signal(SIGINT, handleSignal);    // Ctrl+C
    std::signal(SIGTERM, handleSignal);   // kill / systemctl stop

    SSD1306 oled;
    if (!opt.preview) {
        if (!oled.begin(opt.device, opt.address)) {
            fprintf(stderr, "Failed to initialise the display on %s\n", opt.device.c_str());
            return 1;
        }
        oled.setBrightness(opt.brightness);
        oled.setFlipped(opt.flip);
    }

    if (!opt.once) {
        drawSplash(oled);
        show(oled, opt);
        sleep(2);
    }

    SystemInfo info;
    TrafficMeter traffic;

    while (running) {
        info.hostname = getHostname();
        info.ip = getIPv4(info.iface);
        info.dns = getDNS();
        info.temp = getCpuTemp();
        info.uptime = getUptime();
        traffic.update(info.iface, info.rxKBs, info.txKBs);

        oled.clear();
        if (opt.mode == "qr")
            drawQrScreen(oled, opt, info);
        else
            drawInfoScreen(oled, opt, info);
        show(oled, opt);

        if (opt.once)
            break;
        sleep(1);   // returns early if a signal arrives
    }

    // Leave the screen blank and off, unless we were only drawing one frame.
    if (!opt.once) {
        oled.clear();
        oled.display();
        oled.powerOff();
    }
    return 0;
}
