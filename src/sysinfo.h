#pragma once

#include <chrono>
#include <string>

// Small helpers that read live system information from Linux.
// Each one returns a placeholder like "-" if the information is missing,
// so the display never shows garbage.

std::string getHostname();

// First IPv4 address on an interface that is up and not loopback.
// The interface name (e.g. "eth0") is written to ifaceOut.
std::string getIPv4(std::string& ifaceOut);

// First "nameserver" in /etc/resolv.conf.
std::string getDNS();

// CPU temperature, e.g. "55.9`C" (the font draws ` as a degree sign).
std::string getCpuTemp();

// Time since boot, e.g. "3d 4h 12m".
std::string getUptime();

// Network speed on one interface. The kernel only gives running byte
// totals, so a speed needs two readings: (bytes now - bytes before) / time.
class TrafficMeter {
public:
    // Sets rxKBs / txKBs in kilobytes per second. Returns false (and sets
    // both to 0) on the first call or if the counters can't be read.
    bool update(const std::string& iface, double& rxKBs, double& txKBs);

private:
    std::string lastIface;
    unsigned long long lastRx = 0, lastTx = 0;
    std::chrono::steady_clock::time_point lastTime;
    bool havePrevious = false;
};
