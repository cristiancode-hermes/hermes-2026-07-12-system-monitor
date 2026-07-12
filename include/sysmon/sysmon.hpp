#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sysmon {

// ── CPU ──
struct CpuTimes {
  std::string cpu_id;   // "cpu" (total) or "cpu0", "cpu1", ...
  uint64_t user{};
  uint64_t nice{};
  uint64_t system{};
  uint64_t idle{};
  uint64_t iowait{};
  uint64_t irq{};
  uint64_t softirq{};
  uint64_t steal{};

  [[nodiscard]] uint64_t total() const noexcept {
    return user + nice + system + idle + iowait + irq + softirq + steal;
  }

  [[nodiscard]] uint64_t busy() const noexcept {
    return total() - idle - iowait;
  }
};

struct CpuSnapshot {
  CpuTimes current;
  CpuTimes previous;
  [[nodiscard]] double usage() const noexcept;
};

struct CpuInfo {
  std::vector<CpuSnapshot> cpus;
  int core_count{};
  std::string model;
};

[[nodiscard]] CpuInfo read_cpu_info();

// ── Memory ──
struct MemInfo {
  uint64_t total_kb{};
  uint64_t free_kb{};
  uint64_t available_kb{};
  uint64_t buffers_kb{};
  uint64_t cached_kb{};
  uint64_t swap_total_kb{};
  uint64_t swap_free_kb{};

  [[nodiscard]] double used_percent() const noexcept;
  [[nodiscard]] uint64_t used_kb() const noexcept;
};

[[nodiscard]] MemInfo read_mem_info();

// ── Processes ──
struct Process {
  int pid{};
  std::string name;
  char state{};
  uint64_t vm_rss_kb{};
  double cpu_percent{};
  std::string user;
};

struct ProcessList {
  std::vector<Process> processes;
  int total{};
};

[[nodiscard]] ProcessList read_process_list(int max_procs = 50);

// ── Uptime ──
struct UptimeInfo {
  double uptime_seconds{};
  double idle_seconds{};
  [[nodiscard]] std::string formatted() const;
};

[[nodiscard]] UptimeInfo read_uptime();

// ── Load average ──
struct LoadAvg {
  double load1{};
  double load5{};
  double load15{};
  int running{};
  int total_threads{};
  int last_pid{};
};

[[nodiscard]] LoadAvg read_load_avg();

} // namespace sysmon
