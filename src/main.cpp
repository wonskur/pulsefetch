#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "generated_logos.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #include <windows.h>
#else
    #define OS_WINDOWS 0
    #include <unistd.h>
    #include <sys/utsname.h>
    #include <sys/statvfs.h>
#endif

namespace fs = std::filesystem;

const std::vector<std::string> PALETTE = {
    "\033[31m", "\033[32m", "\033[33m", "\033[34m",
    "\033[35m", "\033[36m", "\033[37m"
};
const std::string RESET_COLOR = "\033[0m";

size_t get_clean_width(const std::string& line) {
    size_t len = 0;
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '$' && i + 1 < line.length()) {
            if (std::isdigit(static_cast<unsigned char>(line[i + 1]))) {
                i++;
                continue;
            } else if (line[i + 1] == '$') {
                len++;
                i++;
                continue;
            }
        }
        len++;
    }
    return len;
}

std::string parse_logo_line(const std::string& line) {
    std::string result;
    result.reserve(line.length() + 16);
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '$' && i + 1 < line.length()) {
            if (std::isdigit(static_cast<unsigned char>(line[i + 1]))) {
                int color_index = (line[i + 1] - '1');
                if (color_index >= 0 && color_index < static_cast<int>(PALETTE.size())) {
                    result += PALETTE[color_index];
                }
                i++;
                continue;
            } else if (line[i + 1] == '$') {
                result += '$';
                i++;
                continue;
            }
        }
        result += line[i];
    }
    result += RESET_COLOR;
    return result;
}

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string read_text_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string format_bytes(long long value) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double size = static_cast<double>(value);
    int unit = 0;
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << size << " " << units[unit];
    return out.str();
}

std::string detect_os_id() {
#if OS_WINDOWS
    return "windows";
#else
    std::string content = read_text_file("/etc/os-release");
    std::stringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("ID=", 0) == 0) {
            std::string value = line.substr(3);
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            return trim(value);
        }
    }
    return "unknown";
#endif
}

std::vector<std::string> get_logo_lines(const std::string& os_id) {
    const auto& logos = get_embedded_logos();
    
    std::string key = os_id;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return c == '-' ? '_' : static_cast<char>(std::tolower(c));
    });

    auto it = logos.find(key);
    if (it != logos.end()) return it->second;

    if (logos.count("windows")) return logos.at("windows");
    if (logos.count("unknown")) return logos.at("unknown");

    return {};
}

std::vector<std::string> collect_info_lines() {
    std::vector<std::string> lines;
    lines.reserve(12);
    lines.push_back("\033[1mSystem Information\033[0m");

#if OS_WINDOWS
    lines.push_back("\033[36mOS:\033[0m Windows");
    
    char host[256]; DWORD hSize = sizeof(host);
    GetComputerNameA(host, &hSize);
    lines.push_back("\033[36mHost:\033[0m " + std::string(host));
    lines.push_back("\033[36mKernel:\033[0m NT Kernel");

    int uptime_m = (GetTickCount64() / 1000) / 60;
    lines.push_back("\033[36mUptime:\033[0m " + std::to_string(uptime_m / 60) + "h " + std::to_string(uptime_m % 60) + "m");

    const char* shell = std::getenv("ComSpec");
    lines.push_back("\033[36mShell:\033[0m " + std::string(shell ? shell : "cmd.exe"));

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char cpu[256]; DWORD bSize = sizeof(cpu);
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpu, &bSize);
        lines.push_back("\033[36mCPU:\033[0m " + trim(std::string(cpu)));
        RegCloseKey(hKey);
    }

    lines.push_back("\033[36mGPU:\033[0m Windows Display Device");

    MEMORYSTATUSEX mem; mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        lines.push_back("\033[36mMemory:\033[0m " + format_bytes(mem.ullTotalPhys) + " total, " + format_bytes(mem.ullAvailPhys) + " free");
        lines.push_back("\033[36mSwap:\033[0m " + format_bytes(mem.ullTotalPageFile) + " total, " + format_bytes(mem.ullAvailPageFile) + " free");
    }

    ULARGE_INTEGER freeB, totalB, totalFreeB;
    if (GetDiskFreeSpaceExA("C:\\", &freeB, &totalB, &totalFreeB)) {
        lines.push_back("\033[36mDisk:\033[0m " + format_bytes(totalB.QuadPart) + " total, " + format_bytes(totalFreeB.QuadPart) + " free");
    }
#else
    lines.push_back("\033[36mOS:\033[0m Linux");
#endif

    return lines;
}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

#if OS_WINDOWS
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    std::string os_id = detect_os_id();
    if (argc > 1) os_id = argv[1];

    std::vector<std::string> raw_logo_lines = get_logo_lines(os_id);

    while (!raw_logo_lines.empty() && get_clean_width(raw_logo_lines.front()) == 0) {
        raw_logo_lines.erase(raw_logo_lines.begin());
    }

    std::vector<std::string> info_lines = collect_info_lines();

    size_t max_logo_width = 0;
    for (const auto& l : raw_logo_lines) {
        max_logo_width = std::max(max_logo_width, get_clean_width(l));
    }

    std::string output_buffer;
    output_buffer.reserve(4096);

    size_t max_lines = std::max(raw_logo_lines.size(), info_lines.size());
    for (size_t i = 0; i < max_lines; ++i) {
        std::string raw_left = (i < raw_logo_lines.size()) ? raw_logo_lines[i] : "";
        std::string right = (i < info_lines.size()) ? info_lines[i] : "";

        size_t current_width = get_clean_width(raw_left);
        size_t pad = (max_logo_width > current_width) ? (max_logo_width - current_width) : 0;

        output_buffer += parse_logo_line(raw_left);
        output_buffer.append(pad + 4, ' ');
        output_buffer += right;
        output_buffer += '\n';
    }

    std::cout << output_buffer;

    return 0;
}