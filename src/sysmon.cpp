#include <sysmon/sysmon.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace sysmon {
namespace {

// Read entire file into string, trimming trailing whitespace
[[nodiscard]] std::optional<std::string> read_file(std::string_view path) {
  std::ifstream ifs(path.data());
  if (!ifs) return std::nullopt;
  std::ostringstream oss;
  oss << ifs.rdbuf();
  if (ifs.fail() && !ifs.eof()) return std::nullopt;
  std::string s = oss.str();
  // Trim trailing whitespace
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' ||
                         s.back() == '\n' || s.back() == '\r')) {
    s.pop_back();
  }
  return s;
}

// Split string by delimiter
[[nodiscard]] std::vector<std::string_view> split(std::string_view s, char delim) {
  std::vector<std::string_view> parts;
  size_t start = 0;
  while (start < s.size()) {
    auto end = s.find(delim, start);
    if (end == std::string_view::npos) {
      parts.push_back(s.substr(start));
      break;
    }
    parts.push_back(s.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

// Parse uint64_t from string_view
[[nodiscard]] std::optional<uint64_t> parse_u64(std::string_view sv) {
  uint64_t val{};
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  if (ec != std::errc{}) return std::nullopt;
  return val;
}

// Trim trailing whitespace from string_view
[[nodiscard]] std::string_view trim_trailing(std::string_view sv) {
  while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' ||
                          sv.back() == '\n' || sv.back() == '\r')) {
    sv.remove_suffix(1);
  }
  return sv;
}

// Trim leading whitespace from string_view
[[nodiscard]] std::string_view trim_leading(std::string_view sv) {
  while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) {
    sv.remove_prefix(1);
  }
  return sv;
}

// Split a line like "key: value" into key/value
[[nodiscard]] std::pair<std::string_view, std::string_view> split_kv(std::string_view line) {
  auto colon = line.find(':');
  if (colon == std::string_view::npos) return {line, {}};
  auto key = trim_trailing(line.substr(0, colon));
  auto val = trim_leading(line.substr(colon + 1));
  return {key, val};
}

} // anonymous namespace

// ── CPU ──

CpuInfo read_cpu_info() {
  CpuInfo info;

  // Read /proc/stat
  auto stat_content = read_file("/proc/stat");
  if (!stat_content) {
    spdlog::warn("Cannot read /proc/stat");
    return info;
  }

  std::vector<CpuSnapshot> cpus;
  std::istringstream stream(*stat_content);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.starts_with("cpu")) continue;
    auto parts = split(line, ' ');
    if (parts.size() < 5) continue;

    CpuTimes ct;
    ct.cpu_id = parts[0];
    // Parse the numeric values (parts[1] onwards)
    auto try_parse = [&](int idx, auto& field) {
      if (idx < static_cast<int>(parts.size())) {
        auto v = parse_u64(parts[idx]);
        if (v) field = *v;
      }
    };
    try_parse(1, ct.user);
    try_parse(2, ct.nice);
    try_parse(3, ct.system);
    try_parse(4, ct.idle);
    try_parse(5, ct.iowait);
    try_parse(6, ct.irq);
    try_parse(7, ct.softirq);
    try_parse(8, ct.steal);

    CpuSnapshot snap;
    snap.current = ct;
    cpus.push_back(snap);
  }

  if (cpus.empty()) return info;

  info.cpus = std::move(cpus);
  info.core_count = std::max(1, static_cast<int>(info.cpus.size()) - 1); // first is total

  // Read CPU model
  auto cpuinfo = read_file("/proc/cpuinfo");
  if (cpuinfo) {
    std::istringstream cs(*cpuinfo);
    std::string cl;
    while (std::getline(cs, cl)) {
      auto [key, val] = split_kv(cl);
      if (key == "model name") {
        info.model = std::string(trim_leading(val));
        break;
      }
    }
  }

  return info;
}

double CpuSnapshot::usage() const noexcept {
  auto prev_total = previous.total();
  auto curr_total = current.total();
  if (prev_total == 0 || curr_total <= prev_total) return 0.0;
  auto total_delta = static_cast<double>(curr_total - prev_total);
  auto busy_delta = static_cast<double>(current.busy() - previous.busy());
  if (total_delta <= 0.0) return 0.0;
  return (busy_delta / total_delta) * 100.0;
}

// ── Memory ──

MemInfo read_mem_info() {
  MemInfo mem{};
  auto content = read_file("/proc/meminfo");
  if (!content) {
    spdlog::warn("Cannot read /proc/meminfo");
    return mem;
  }

  std::istringstream stream(*content);
  std::string line;
  while (std::getline(stream, line)) {
    auto [key, val_str] = split_kv(line);
    // Remove " kB" suffix
    while (!val_str.empty() && val_str.back() == ' ') val_str.remove_suffix(1);
    if (val_str.size() >= 3 && val_str.substr(val_str.size() - 3) == " kB") {
      val_str = val_str.substr(0, val_str.size() - 3);
    }
    auto val = parse_u64(trim_leading(val_str));
    if (!val) continue;

    if (key == "MemTotal")       mem.total_kb = *val;
    else if (key == "MemFree")   mem.free_kb = *val;
    else if (key == "MemAvailable") mem.available_kb = *val;
    else if (key == "Buffers")   mem.buffers_kb = *val;
    else if (key == "Cached")    mem.cached_kb = *val;
    else if (key == "SwapTotal") mem.swap_total_kb = *val;
    else if (key == "SwapFree")  mem.swap_free_kb = *val;
  }

  return mem;
}

double MemInfo::used_percent() const noexcept {
  if (total_kb == 0) return 0.0;
  return (static_cast<double>(used_kb()) / static_cast<double>(total_kb)) * 100.0;
}

uint64_t MemInfo::used_kb() const noexcept {
  if (available_kb > 0) {
    return total_kb > available_kb ? total_kb - available_kb : 0;
  }
  return total_kb > (free_kb + buffers_kb + cached_kb)
             ? total_kb - (free_kb + buffers_kb + cached_kb)
             : 0;
}

// ── Processes ──

ProcessList read_process_list(int max_procs) {
  ProcessList pl;

  // Read loadavg for total process count
  auto load_content = read_file("/proc/loadavg");
  if (load_content) {
    auto parts = split(*load_content, ' ');
    if (parts.size() >= 3) {
      if (auto v = parse_u64(parts[0])) pl.total = static_cast<int>(*v);
    }
  }

  // Iterate /proc/[pid]/ directories
  std::error_code ec;
  for (auto& entry : fs::directory_iterator("/proc", ec)) {
    if (!entry.is_directory()) continue;
    auto dirname = entry.path().filename().string();
    int pid{};
    auto [ptr, ec_parse] = std::from_chars(dirname.data(), dirname.data() + dirname.size(), pid);
    if (ec_parse != std::errc{}) continue;
    if (static_cast<int>(pl.processes.size()) >= max_procs) break;

    Process proc;
    proc.pid = pid;

    // Read /proc/[pid]/stat
    auto stat_path = entry.path() / "stat";
    auto stat = read_file(stat_path.string());
    if (!stat) continue;

    // Parse: pid (comm) state ppid ... 
    // comm is in parentheses, skip to after ")"
    auto comm_start = stat->find('(');
    auto comm_end = stat->rfind(')');
    if (comm_start == std::string_view::npos || comm_end == std::string_view::npos) continue;
    proc.name = stat->substr(comm_start + 1, comm_end - comm_start - 1);
    if (comm_end + 2 < stat->size()) {
      proc.state = (*stat)[comm_end + 2];
    }

    // Parse fields after ")"
    auto after_comm = std::string_view(*stat).substr(comm_end + 2);
    auto fields = split(after_comm, ' ');
    if (fields.size() > 22) {
      // field 22 (0-indexed) is RSS in pages -> convert to KB
      // We skip complex parsing for now and use /proc/[pid]/status for RSS
    }

    // Read /proc/[pid]/status for RSS and other info
    auto status_path = entry.path() / "status";
    auto status = read_file(status_path.string());
    if (status) {
      std::istringstream ss(*status);
      std::string sl;
      while (std::getline(ss, sl)) {
        auto [key, val] = split_kv(sl);
        if (key == "VmRSS") {
          // format: "12345 kB"
          auto trimmed = trim_leading(val);
          auto space = trimmed.find(' ');
          if (space != std::string_view::npos) {
            trimmed = trimmed.substr(0, space);
          }
          if (auto v = parse_u64(trimmed)) proc.vm_rss_kb = *v;
        }
      }
    }

    pl.processes.push_back(std::move(proc));
  }

  // Sort by RSS descending
  std::ranges::sort(pl.processes, [](const Process& a, const Process& b) {
    return a.vm_rss_kb > b.vm_rss_kb;
  });

  // Cap at max_procs
  if (static_cast<int>(pl.processes.size()) > max_procs) {
    pl.processes.resize(max_procs);
  }

  // Count total by reading /proc/loadavg differently
  pl.total = static_cast<int>(pl.processes.size());

  return pl;
}

// ── Uptime ──

UptimeInfo read_uptime() {
  UptimeInfo info{};
  auto content = read_file("/proc/uptime");
  if (!content) return info;

  auto parts = split(*content, ' ');
  if (parts.size() >= 2) {
    auto parse_double = [](std::string_view sv) -> std::optional<double> {
      double val{};
      auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
      if (ec != std::errc{}) return std::nullopt;
      return val;
    };
    if (auto v = parse_double(parts[0])) info.uptime_seconds = *v;
    if (auto v = parse_double(parts[1])) info.idle_seconds = *v;
  }
  return info;
}

std::string UptimeInfo::formatted() const {
  auto total_sec = static_cast<uint64_t>(uptime_seconds);
  auto days = total_sec / 86400;
  auto hours = (total_sec % 86400) / 3600;
  auto mins = (total_sec % 3600) / 60;
  auto secs = total_sec % 60;

  std::string result;
  if (days > 0) result += std::to_string(days) + "d ";
  if (hours > 0) result += std::to_string(hours) + "h ";
  result += std::to_string(mins) + "m " + std::to_string(secs) + "s";
  return result;
}

// ── Load Average ──

LoadAvg read_load_avg() {
  LoadAvg la{};
  auto content = read_file("/proc/loadavg");
  if (!content) return la;

  auto parts = split(*content, ' ');
  if (parts.size() >= 5) {
    auto parse_double = [](std::string_view sv) -> std::optional<double> {
      double val{};
      auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
      if (ec != std::errc{}) return std::nullopt;
      return val;
    };
    if (auto v = parse_double(parts[0])) la.load1 = *v;
    if (auto v = parse_double(parts[1])) la.load5 = *v;
    if (auto v = parse_double(parts[2])) la.load15 = *v;

    // parts[3] = running/total
    auto rt = split(parts[3], '/');
    if (rt.size() == 2) {
      if (auto v = parse_u64(rt[0])) la.running = static_cast<int>(*v);
      if (auto v = parse_u64(rt[1])) la.total_threads = static_cast<int>(*v);
    }

    if (auto v = parse_u64(parts[4])) la.last_pid = static_cast<int>(*v);
  }
  return la;
}

} // namespace sysmon
