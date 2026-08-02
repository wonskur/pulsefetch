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
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

const std::vector<std::string> PALETTE = {
    "\033[36m", // blue(cyan)
    "\033[37m", // white
    "\033[31m", // red
    "\033[32m", // green
    "\033[33m", // yellow
    "\033[35m"  // magenta
};
const std::string RESET_COLOR = "\033[0m";

std::string trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
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
    if (!file.is_open()) {
        return "";
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string detect_os_name() {
    std::string content = read_text_file("/etc/os-release");
    std::stringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            std::string value = line.substr(13);
            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }
            return trim(value);
        }
    }
    return "Linux";
}

std::string detect_os_id() {
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
}

std::string detect_host() {
    char buffer[256] = {0};
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        return std::string(buffer);
    }
    return "unknown";
}

std::string detect_kernel() {
    utsname info;
    if (uname(&info) == 0) {
        return std::string(info.release);
    }
    return "unknown";
}

std::string format_uptime() {
    std::ifstream file("/proc/uptime");
    if (!file.is_open()) {
        return "unknown";
    }
    double seconds = 0.0;
    file >> seconds;
    unsigned int total_seconds = static_cast<unsigned int>(seconds);
    unsigned int days = total_seconds / 86400;
    unsigned int hours = (total_seconds % 86400) / 3600;
    unsigned int minutes = (total_seconds % 3600) / 60;
    std::ostringstream out;
    if (days > 0) {
        out << days << "d ";
    }
    out << hours << "h " << minutes << "m";
    return out.str();
}

std::string detect_shell() {
    const char* shell = std::getenv("SHELL");
    if (shell && *shell) {
        return shell;
    }
    return "/bin/sh";
}

std::string detect_cpu() {
    std::ifstream file("/proc/cpuinfo");
    if (!file.is_open()) {
        return "unknown";
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("model name", 0) == 0 || line.rfind("Model", 0) == 0) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                return trim(line.substr(pos + 1));
            }
        }
    }
    return "unknown";
}

std::string detect_gpu() {
    std::FILE* pipe = popen("lspci 2>/dev/null | grep -iE 'vga|3d|display' | head -n 1", "r");
    if (!pipe) {
        return "unknown";
    }
    char buffer[512] = {0};
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    result = trim(result);
    if (!result.empty()) {
        return result;
    }
    return "unknown";
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

std::string detect_memory() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return "unknown";
    }
    long long mem_total = 0;
    long long mem_available = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            mem_total = std::atoll(line.substr(10).c_str());
        } else if (line.rfind("MemAvailable:", 0) == 0) {
            mem_available = std::atoll(line.substr(13).c_str());
        }
    }
    if (mem_total > 0) {
        return format_bytes(mem_total * 1024) + " total, " + format_bytes(mem_available * 1024) + " free";
    }
    return "unknown";
}

std::string detect_swap() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return "unknown";
    }
    long long swap_total = 0;
    long long swap_free = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("SwapTotal:", 0) == 0) {
            swap_total = std::atoll(line.substr(10).c_str());
        } else if (line.rfind("SwapFree:", 0) == 0) {
            swap_free = std::atoll(line.substr(9).c_str());
        }
    }
    if (swap_total > 0) {
        return format_bytes(swap_total * 1024) + " total, " + format_bytes(swap_free * 1024) + " free";
    }
    return "unknown";
}

std::string detect_disk() {
    struct statvfs stats;
    if (statvfs("/", &stats) == 0) {
        long long total = static_cast<long long>(stats.f_blocks) * stats.f_frsize;
        long long free = static_cast<long long>(stats.f_bavail) * stats.f_frsize;
        return format_bytes(total) + " total, " + format_bytes(free) + " free";
    }
    return "unknown";
}

std::string resolve_logo_path(const std::string& requested_path) {
    if (!requested_path.empty()) {
        return requested_path;
    }

    std::string os_id = detect_os_id();
    std::string normalized = os_id;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return c == '-' ? '_' : static_cast<char>(std::tolower(c));
    });

    std::vector<fs::path> candidates;
    candidates.push_back(fs::current_path() / "src" / "logos");
    candidates.push_back(fs::current_path() / "logos");
    candidates.push_back(fs::path("src") / "logos");

    for (const auto& base_dir : candidates) {
        if (!fs::exists(base_dir)) {
            continue;
        }

        fs::path exact = base_dir / (normalized + ".txt");
        if (fs::exists(exact)) {
            return exact.string();
        }

        std::string first = normalized.substr(0, 1);
        fs::path dir = base_dir / first;
        if (fs::exists(dir)) {
            fs::path candidate = dir / (normalized + ".txt");
            if (fs::exists(candidate)) {
                return candidate.string();
            }
        }

        for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
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
    lines.push_back("\033[36mOS:\033[0m " + detect_os_name());
    lines.push_back("\033[36mHost:\033[0m " + detect_host());
    lines.push_back("\033[36mKernel:\033[0m " + detect_kernel());
    lines.push_back("\033[36mUptime:\033[0m " + format_uptime());
    lines.push_back("\033[36mShell:\033[0m " + detect_shell());
    lines.push_back("\033[36mCPU:\033[0m " + detect_cpu());
    lines.push_back("\033[36mGPU:\033[0m " + detect_gpu());
    lines.push_back("\033[36mMemory:\033[0m " + detect_memory());
    lines.push_back("\033[36mSwap:\033[0m " + detect_swap());
    lines.push_back("\033[36mDisk:\033[0m " + detect_disk());
    return lines;
}

int main(int argc, char* argv[]) {
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
        max_logo_width = std::max(max_logo_width, logo_line.size());
    }

    size_t max_lines = std::max(logo_lines.size(), info_lines.size());
    for (size_t i = 0; i < max_lines; ++i) {
        std::string left = (i < logo_lines.size()) ? logo_lines[i] : std::string(max_logo_width, ' ');
        std::string right = (i < info_lines.size()) ? info_lines[i] : "";
        std::cout << left;
        if (!right.empty()) {
            std::cout << std::string(6, ' ') << right;
        }
        std::cout << std::endl;
    }

    return 0;
}