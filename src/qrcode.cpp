#include "qrcode.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

// Building a QR code takes five steps:
//   1. Turn the text into a bit stream and pad it to the version's capacity.
//   2. Add Reed-Solomon error correction bytes.
//   3. Draw the fixed patterns (finders, timing, alignment).
//   4. Fill the remaining modules with the data bits in a zigzag.
//   5. XOR the data with a mask pattern and record which mask was used.

namespace {

// Numbers from the QR spec for error correction level L.
struct VersionInfo {
    int dataBytes;      // bytes available for data
    int eccBytes;       // bytes of error correction
    int alignmentPos;   // centre of the alignment pattern (0 = none)
};

const VersionInfo VERSIONS[] = {
    {0, 0, 0},     // index 0 unused: versions start at 1
    {19, 7, 0},    // version 1: 21x21
    {34, 10, 18},  // version 2: 25x25
    {55, 15, 22},  // version 3: 29x29
};
const int MAX_VERSION = 3;

// Format bits for error correction level L (the spec's 2-bit code is 01).
const int ECC_LEVEL_L_BITS = 1;

bool getBit(int value, int i) {
    return (value >> i) & 1;
}

// How many text bytes a version holds: 4 bits mode + 8 bits length + data.
int textCapacity(int version) {
    return (VERSIONS[version].dataBytes * 8 - 12) / 8;
}

// ---------------------------------------------------------------------
// Step 1: data bytes
// ---------------------------------------------------------------------

void appendBits(std::vector<bool>& bits, unsigned value, int count) {
    for (int i = count - 1; i >= 0; i--)
        bits.push_back(getBit(value, i));
}

std::vector<uint8_t> makeDataBytes(const std::string& text, int capacityBytes) {
    std::vector<bool> bits;
    appendBits(bits, 0x4, 4);            // mode indicator: byte mode
    appendBits(bits, text.size(), 8);    // character count
    for (unsigned char c : text)
        appendBits(bits, c, 8);

    // Terminator: up to four 0 bits, then pad to a whole byte.
    int capacityBits = capacityBytes * 8;
    appendBits(bits, 0, std::min(4, capacityBits - (int)bits.size()));
    while (bits.size() % 8 != 0)
        bits.push_back(false);

    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t b = 0;
        for (int j = 0; j < 8; j++)
            b = (b << 1) | bits[i + j];
        bytes.push_back(b);
    }

    // Fill any remaining space with the spec's alternating pad bytes.
    for (uint8_t pad = 0xEC; (int)bytes.size() < capacityBytes; pad ^= 0xEC ^ 0x11)
        bytes.push_back(pad);

    return bytes;
}

// ---------------------------------------------------------------------
// Step 2: Reed-Solomon error correction
// ---------------------------------------------------------------------

// Multiplication in GF(256), the number system QR error correction uses.
// Addition in this field is XOR; 0x11D is the field's reducing polynomial.
uint8_t gfMultiply(uint8_t x, uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; i--) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= getBit(y, i) * x;
    }
    return z;
}

// The error correction bytes are the remainder of dividing the data
// (as a polynomial) by a fixed "generator" polynomial.
std::vector<uint8_t> reedSolomon(const std::vector<uint8_t>& data, int eccCount) {
    // Generator = (x - 1)(x - 2)(x - 2^2)...(x - 2^(eccCount-1)).
    // Stored without its leading 1, highest power first.
    std::vector<uint8_t> generator(eccCount, 0);
    generator[eccCount - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < eccCount; i++) {
        for (int j = 0; j < eccCount; j++) {
            generator[j] = gfMultiply(generator[j], root);
            if (j + 1 < eccCount)
                generator[j] ^= generator[j + 1];
        }
        root = gfMultiply(root, 2);
    }

    // Polynomial long division, keeping only the remainder.
    std::vector<uint8_t> remainder(eccCount, 0);
    for (uint8_t b : data) {
        uint8_t factor = b ^ remainder[0];
        remainder.erase(remainder.begin());
        remainder.push_back(0);
        for (int i = 0; i < eccCount; i++)
            remainder[i] ^= gfMultiply(generator[i], factor);
    }
    return remainder;
}

// ---------------------------------------------------------------------
// Steps 3-5: the module grid
// ---------------------------------------------------------------------

struct Grid {
    int size;
    std::vector<std::vector<bool>> dark;        // [y][x]
    std::vector<std::vector<bool>> isFixed;     // part of a fixed pattern?

    explicit Grid(int n)
        : size(n),
          dark(n, std::vector<bool>(n, false)),
          isFixed(n, std::vector<bool>(n, false)) {}

    void setFixed(int x, int y, bool on) {
        dark[y][x] = on;
        isFixed[y][x] = true;
    }
};

// 7x7 square in a corner, plus its 1-module light border.
void drawFinder(Grid& g, int cx, int cy) {
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= g.size || y < 0 || y >= g.size)
                continue;
            int ring = std::max(std::abs(dx), std::abs(dy));
            g.setFixed(x, y, ring != 2 && ring != 4);   // rings 2 and 4 are light
        }
    }
}

// Smaller 5x5 square that helps scanners correct for distortion.
void drawAlignment(Grid& g, int cx, int cy) {
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            g.setFixed(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
}

// 15 bits saying which error correction level and mask are used, drawn
// twice (once near the top-left finder, once split between the others).
void drawFormatBits(Grid& g, int mask) {
    int data = (ECC_LEVEL_L_BITS << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; i++)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);   // BCH error correction
    int bits = ((data << 10) | rem) ^ 0x5412;      // fixed XOR from the spec

    // Copy 1: around the top-left finder.
    for (int i = 0; i <= 5; i++)
        g.setFixed(8, i, getBit(bits, i));
    g.setFixed(8, 7, getBit(bits, 6));
    g.setFixed(8, 8, getBit(bits, 7));
    g.setFixed(7, 8, getBit(bits, 8));
    for (int i = 9; i < 15; i++)
        g.setFixed(14 - i, 8, getBit(bits, i));

    // Copy 2: under the top-right finder and beside the bottom-left one.
    for (int i = 0; i < 8; i++)
        g.setFixed(g.size - 1 - i, 8, getBit(bits, i));
    for (int i = 8; i < 15; i++)
        g.setFixed(8, g.size - 15 + i, getBit(bits, i));
    g.setFixed(8, g.size - 8, true);   // the spec's "always dark" module
}

void drawFixedPatterns(Grid& g, int version) {
    // Timing patterns: alternating dark/light lines along row 6 and column 6.
    for (int i = 0; i < g.size; i++) {
        g.setFixed(6, i, i % 2 == 0);
        g.setFixed(i, 6, i % 2 == 0);
    }

    drawFinder(g, 3, 3);
    drawFinder(g, g.size - 4, 3);
    drawFinder(g, 3, g.size - 4);

    // Versions 2-6 have a single alignment pattern near the bottom right.
    int pos = VERSIONS[version].alignmentPos;
    if (pos != 0)
        drawAlignment(g, pos, pos);

    drawFormatBits(g, 0);   // placeholder, so the data skips these modules
}

// Data fills the grid in 2-module wide columns, starting at the bottom
// right, snaking up then down, and skipping all fixed modules.
void placeData(Grid& g, const std::vector<uint8_t>& bytes) {
    size_t bitIndex = 0;
    size_t totalBits = bytes.size() * 8;

    for (int right = g.size - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;   // skip the vertical timing pattern column

        bool upward = ((right + 1) & 2) == 0;
        for (int i = 0; i < g.size; i++) {
            int y = upward ? g.size - 1 - i : i;
            for (int j = 0; j < 2; j++) {
                int x = right - j;
                if (g.isFixed[y][x] || bitIndex >= totalBits)
                    continue;   // leftover modules stay light
                g.dark[y][x] = getBit(bytes[bitIndex / 8], 7 - bitIndex % 8);
                bitIndex++;
            }
        }
    }
}

// The 8 mask patterns from the spec. Masking breaks up large blocks of one
// colour that would confuse a scanner.
bool maskFlips(int mask, int x, int y) {
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return (x / 3 + y / 2) % 2 == 0;
        case 5: return x * y % 2 + x * y % 3 == 0;
        case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
        default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}

// XOR is its own inverse, so calling this twice undoes it.
void applyMask(Grid& g, int mask) {
    for (int y = 0; y < g.size; y++)
        for (int x = 0; x < g.size; x++)
            if (!g.isFixed[y][x] && maskFlips(mask, x, y))
                g.dark[y][x] = !g.dark[y][x];
}

// Scores how hard a grid is to scan (lower is better), using the four
// rules from the spec.
int penalty(const Grid& g) {
    int n = g.size;
    int score = 0;
    auto at = [&](int x, int y, bool transpose) {
        return transpose ? g.dark[x][y] : g.dark[y][x];
    };

    // Rules 1 and 3 look along every row, then along every column.
    for (int pass = 0; pass < 2; pass++) {
        bool transpose = pass == 1;
        for (int y = 0; y < n; y++) {
            // Rule 1: runs of 5+ same-coloured modules.
            int run = 1;
            for (int x = 1; x <= n; x++) {
                if (x < n && at(x, y, transpose) == at(x - 1, y, transpose)) {
                    run++;
                } else {
                    if (run >= 5)
                        score += 3 + (run - 5);
                    run = 1;
                }
            }

            // Rule 3: patterns that look like a finder (1:1:3:1:1) next
            // to 4 light modules.
            const bool A[11] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
            const bool B[11] = {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1};
            for (int x = 0; x + 11 <= n; x++) {
                bool matchA = true, matchB = true;
                for (int k = 0; k < 11; k++) {
                    bool d = at(x + k, y, transpose);
                    matchA = matchA && d == A[k];
                    matchB = matchB && d == B[k];
                }
                if (matchA) score += 40;
                if (matchB) score += 40;
            }
        }
    }

    // Rule 2: 2x2 blocks of one colour.
    for (int y = 0; y + 1 < n; y++)
        for (int x = 0; x + 1 < n; x++) {
            bool c = g.dark[y][x];
            if (c == g.dark[y][x + 1] && c == g.dark[y + 1][x] && c == g.dark[y + 1][x + 1])
                score += 3;
        }

    // Rule 4: overall balance of dark and light, 10 points per 5% away from 50%.
    int darkCount = 0;
    for (const auto& row : g.dark)
        darkCount += std::count(row.begin(), row.end(), true);
    int percent = darkCount * 100 / (n * n);
    score += std::abs(percent - 50) / 5 * 10;

    return score;
}

} // namespace

bool encodeQr(const std::string& text, QrCode& out, int mask) {
    // Pick the smallest version that fits the text.
    int version = 1;
    while (version <= MAX_VERSION && textCapacity(version) < (int)text.size())
        version++;
    if (version > MAX_VERSION)
        return false;

    const VersionInfo& info = VERSIONS[version];

    // Steps 1 and 2.
    std::vector<uint8_t> bytes = makeDataBytes(text, info.dataBytes);
    std::vector<uint8_t> ecc = reedSolomon(bytes, info.eccBytes);
    bytes.insert(bytes.end(), ecc.begin(), ecc.end());

    // Steps 3 and 4.
    Grid g(17 + 4 * version);
    drawFixedPatterns(g, version);
    placeData(g, bytes);

    // Step 5: try every mask and keep the one with the lowest penalty.
    if (mask < 0) {
        int bestScore = -1;
        for (int m = 0; m < 8; m++) {
            applyMask(g, m);
            drawFormatBits(g, m);
            int score = penalty(g);
            if (bestScore < 0 || score < bestScore) {
                bestScore = score;
                mask = m;
            }
            applyMask(g, m);   // undo
        }
    }
    applyMask(g, mask);
    drawFormatBits(g, mask);

    out.size = g.size;
    out.modules = g.dark;
    return true;
}
