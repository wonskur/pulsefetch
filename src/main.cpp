#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <filesystem>

#include "generated_logos.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #define NOMINMAX
    #include <windows.h>
#else
    #define OS_WINDOWS 0
    #include <sys/utsname.h>
    #include <unistd.h>
#endif

namespace fs = std::filesystem;

const std::vector<std::string> PALETTE = {
    "\033[31m", "\033[32m", "\033[33m", "\033[34m",
    "\033[35m", "\033[36m", "\033[37m"
};
const std::string RESET_COLOR = "\033[0m";
std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}
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
std::string parse_logo_line(const std::string& line, std::string& active_color, const std::vector<std::string>& palette) {
    std::string result;
    result.reserve(line.length() + 32);
    result += active_color;
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '$' && i + 1 < line.length()) {
            if (std::isdigit(static_cast<unsigned char>(line[i + 1]))) {
                int color_index = (line[i + 1] - '1');
                if (color_index >= 0 && color_index < static_cast<int>(palette.size())) {
                    active_color = palette[color_index];
                    result += active_color;
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
std::string detect_os_id() {
#if OS_WINDOWS
    return "windows";
#else
    std::ifstream os_release("/etc/os-release");
    if (os_release.is_open()) {
        std::string line;
        while (std::getline(os_release, line)) {
            if (line.rfind("ID=", 0) == 0) {
                std::string val = line.substr(3);
                if (!val.empty() && (val.front() == '"' || val.front() == '\'')) val.erase(0, 1);
                if (!val.empty() && (val.back() == '"' || val.back() == '\'')) val.pop_back();
                return val;
            }
        }
    }
    return "linux";
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
    if (logos.count("linux")) return logos.at("linux");
    if (logos.count("windows")) return logos.at("windows");
    return {};
}
std::vector<std::string> collect_info_lines() {
    std::vector<std::string> lines;
    lines.reserve(12);
    lines.push_back("\033[1mSystem Information\033[0m");

#if OS_WINDOWS
    char username[256]; DWORD uSize = sizeof(username);
    if (GetUserNameA(username, &uSize)) {
        lines.push_back("\033[32mUser:\033[0m " + std::string(username));
    }
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char cpu[256]; DWORD bSize = sizeof(cpu);
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpu, &bSize);
        lines.push_back("\033[36mCPU:\033[0m " + trim(std::string(cpu)));
        RegCloseKey(hKey);
    }
#else
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        char* user = getlogin();
        if (user) lines.push_back("\033[32mUser:\033[0m " + std::string(user) + "@" + std::string(hostname));
    }
    struct utsname sysinfo_data;
    if (uname(&sysinfo_data) == 0) {
        lines.push_back("\033[33mOS:\033[0m " + std::string(sysinfo_data.sysname) + " " + std::string(sysinfo_data.release));
        lines.push_back("\033[35mKernel:\033[0m " + std::string(sysinfo_data.release));
        lines.push_back("\033[34mArch:\033[0m " + std::string(sysinfo_data.machine));
    }
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.rfind("model name", 0) == 0) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    lines.push_back("\033[36mCPU:\033[0m " + trim(line.substr(colon + 1)));
                    break;
                }
            }
        }
    }
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
    std::vector<std::string> current_palette = PALETTE;
    if (os_id == "debian") {
        current_palette = { "\033[31m", "\033[37m" };
    } else if (os_id == "arch" || os_id == "cachyos") {
        current_palette = { "\033[36m", "\033[34m", "\033[37m" };
    }
    size_t max_lines = std::max(raw_logo_lines.size(), info_lines.size());
    std::string output_buffer;
    output_buffer.reserve(4096);
    std::string active_color = current_palette.empty() ? "" : current_palette[0];
    for (size_t i = 0; i < max_lines; ++i) {
        std::string raw_left = (i < raw_logo_lines.size()) ? raw_logo_lines[i] : "";
        std::string right = (i < info_lines.size()) ? info_lines[i] : "";
        size_t current_width = get_clean_width(raw_left);
        size_t pad = (max_logo_width > current_width) ? (max_logo_width - current_width) : 0;
        output_buffer += parse_logo_line(raw_left, active_color, current_palette);
        output_buffer.append(pad + 4, ' ');
        output_buffer += right;
        output_buffer += '\n';
    }

    std::cout << output_buffer;

    return 0;
}
