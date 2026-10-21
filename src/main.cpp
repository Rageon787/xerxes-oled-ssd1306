#include "ssd1306.h"
#include <iostream>
#include <unistd.h>


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
