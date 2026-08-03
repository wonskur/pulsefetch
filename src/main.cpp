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
    "\033[31m",
    "\033[32m",
    "\033[33m",
    "\033[34m",
    "\033[35m",
    "\033[36m",
    "\033[37m"
};
const std::string RESET_COLOR = "\033[0m";

size_t visible_length(const std::string& str) {
    size_t len = 0;
    bool in_escape = false;
    for (char c : str) {
        if (c == '\033') {
            in_escape = true;
        } else if (in_escape) {
            if (c == 'm') in_escape = false;
        } else {
            len++;
        }
    }
    return len;
}

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string parse_logo_line(const std::string& line) {
    std::string result;
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '$' && i + 1 < line.length() && std::isdigit(static_cast<unsigned char>(line[i + 1]))) {
            int color_index = (line[i + 1] - '1');
            if (color_index >= 0 && color_index < static_cast<int>(PALETTE.size())) {
                result += PALETTE[color_index];
            }
            ++i;
        } else if (line[i] == '$' && i + 1 < line.length() && line[i + 1] == '$') {
            result += '$';
            ++i;
        } else {
            result += line[i];
        }
    }
    return result + RESET_COLOR;
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

std::string resolve_logo_path(const std::string& requested_path) {
    if (!requested_path.empty()) return requested_path;

    std::string os_id = detect_os_id();
    std::string normalized = os_id;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return c == '-' ? '_' : static_cast<char>(std::tolower(c));
    });

    std::vector<fs::path> candidates = {
        fs::current_path() / "src" / "logos",
        fs::current_path() / "logos",
        fs::path("src") / "logos"
    };

    for (const auto& base_dir : candidates) {
        if (!fs::exists(base_dir)) continue;

        fs::path exact = base_dir / (normalized + ".txt");
        if (fs::exists(exact)) return exact.string();

        for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().filename().string() == normalized + ".txt") {
                return entry.path().string();
            }
        }
    }

    return (fs::current_path() / "src" / "logos" / "_" / "unknown.txt").string();
}

std::vector<std::string> collect_info_lines() {
    std::vector<std::string> lines;
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
        lines.push_back("\033[36mCPU:\033[0m " + std::string(cpu));
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
#if OS_WINDOWS
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    std::string logo_path = (argc > 1) ? argv[1] : "";
    std::string resolved_logo = resolve_logo_path(logo_path);

    std::ifstream file(resolved_logo);
    if (!file.is_open()) {
        std::cerr << "Failed to open logo file: " << resolved_logo << std::endl;
        return 1;
    }

    std::vector<std::string> logo_lines;
    std::string line;
    while (std::getline(file, line)) {
        logo_lines.push_back(parse_logo_line(line));
    }
    file.close();

    std::vector<std::string> info_lines = collect_info_lines();

    size_t max_logo_width = 0;
    for (const auto& logo_line : logo_lines) {
        max_logo_width = std::max(max_logo_width, visible_length(logo_line));
    }

    size_t max_lines = std::max(logo_lines.size(), info_lines.size());
    for (size_t i = 0; i < max_lines; ++i) {
        std::string left = (i < logo_lines.size()) ? logo_lines[i] : "";
        std::string right = (i < info_lines.size()) ? info_lines[i] : "";

        size_t current_vis_len = visible_length(left);
        size_t pad = (max_logo_width > current_vis_len) ? (max_logo_width - current_vis_len) : 0;

        std::cout << left << std::string(pad + 4, ' ') << right << std::endl;
    }

    return 0;
}