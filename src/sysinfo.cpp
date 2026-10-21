#include "sysinfo.h"

#include <arpa/inet.h>    // inet_ntop
#include <ifaddrs.h>      // getifaddrs
#include <net/if.h>       // IFF_UP, IFF_LOOPBACK
#include <netinet/in.h>   // sockaddr_in
#include <unistd.h>       // gethostname

#include <cstdio>         // snprintf
#include <cstdlib>        // labs
#include <fstream>
#include <sstream>

std::string getHostname() {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0)
        return "unknown";
    buf[sizeof(buf) - 1] = '\0';   // gethostname may not terminate on truncation
    return buf;
}

std::string getIPv4(std::string& ifaceOut) {
    ifaceOut.clear();

    struct ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0)
        return "No IP";

    std::string result = "No IP";
    for (struct ifaddrs* ifa = list; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)             continue;   // no address
        if (ifa->ifa_addr->sa_family != AF_INET)  continue;   // not IPv4
        if (!(ifa->ifa_flags & IFF_UP))           continue;   // interface down
        if (ifa->ifa_flags & IFF_LOOPBACK)        continue;   // 127.0.0.1

        char ip[INET_ADDRSTRLEN];
        auto* sin = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
        if (inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip)) != nullptr) {
            result = ip;
            ifaceOut = ifa->ifa_name;
            break;
        }
    }

    freeifaddrs(list);   // getifaddrs allocates memory, so free it
    return result;
}

std::string getDNS() {
    std::ifstream file("/etc/resolv.conf");
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

std::string getCpuTemp() {
    // The file holds millidegrees, e.g. 55900 = 55.9 C.
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    long milli;
    if (!(file >> milli))
        return "--.-`C";

    char buf[32];
    snprintf(buf, sizeof(buf), "%ld.%ld`C", milli / 1000, std::labs(milli % 1000) / 100);
    return buf;
}

std::string getUptime() {
    // First number in /proc/uptime is seconds since boot.
    std::ifstream file("/proc/uptime");
    double seconds;
    if (!(file >> seconds))
        return "-";

    long total = static_cast<long>(seconds);
    long days = total / 86400;
    long hours = total % 86400 / 3600;
    long minutes = total % 3600 / 60;

    char buf[32];
    if (days > 0)
        snprintf(buf, sizeof(buf), "%ldd %ldh %ldm", days, hours, minutes);
    else
        snprintf(buf, sizeof(buf), "%ldh %ldm", hours, minutes);
    return buf;
}

// Reads a counter such as /sys/class/net/eth0/statistics/rx_bytes.
static bool readCounter(const std::string& iface, const char* name, unsigned long long& out) {
    std::ifstream file("/sys/class/net/" + iface + "/statistics/" + name);
    return static_cast<bool>(file >> out);
}

bool TrafficMeter::update(const std::string& iface, double& rxKBs, double& txKBs) {
    rxKBs = txKBs = 0.0;

    unsigned long long rx, tx;
    if (iface.empty() || !readCounter(iface, "rx_bytes", rx) || !readCounter(iface, "tx_bytes", tx)) {
        havePrevious = false;
        return false;
    }

    auto now = std::chrono::steady_clock::now();

    // First reading, or the interface changed: just store a baseline.
    if (!havePrevious || iface != lastIface) {
        lastIface = iface;
        lastRx = rx;
        lastTx = tx;
        lastTime = now;
        havePrevious = true;
        return false;
    }

    double seconds = std::chrono::duration<double>(now - lastTime).count();
    if (seconds > 0.0 && rx >= lastRx && tx >= lastTx) {
        rxKBs = (rx - lastRx) / 1024.0 / seconds;
        txKBs = (tx - lastTx) / 1024.0 / seconds;
    }

    lastRx = rx;
    lastTx = tx;
    lastTime = now;
    return true;
}
