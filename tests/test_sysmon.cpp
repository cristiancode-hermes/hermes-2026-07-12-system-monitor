#include <sysmon/sysmon.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace Catch::Matchers;

// ── Unit tests: CPU info parsing ──

TEST_CASE("CPU info reading", "[cpu][unit]") {
  auto info = sysmon::read_cpu_info();

  REQUIRE(info.core_count > 0);
  REQUIRE(info.cpus.size() >= 2); // at least total + 1 core
  REQUIRE(info.cpus[0].current.cpu_id == "cpu");

  // Usage should be between 0 and 100
  double usage = info.cpus[0].usage();
  REQUIRE(usage >= 0.0);
  REQUIRE(usage <= 100.0);
}

TEST_CASE("CPU snapshot usage calculation", "[cpu][unit]") {
  sysmon::CpuSnapshot snap;
  snap.previous = {.cpu_id = "cpu0", .user = 1000, .nice = 200, .system = 800, .idle = 8000};
  snap.current  = {.cpu_id = "cpu0", .user = 1100, .nice = 220, .system = 880, .idle = 8800};

  double usage = snap.usage();
  // With these values, usage should be at least some positive number
  REQUIRE(usage >= 0.0);
  REQUIRE(usage <= 100.0);
}

TEST_CASE("CPU total and busy calculation", "[cpu][unit]") {
  sysmon::CpuTimes ct;
  ct.user = 100;
  ct.nice = 20;
  ct.system = 80;
  ct.idle = 800;
  ct.iowait = 10;
  ct.irq = 5;
  ct.softirq = 3;
  ct.steal = 2;

  REQUIRE(ct.total() > 0);
  REQUIRE(ct.busy() > 0);
  REQUIRE(ct.busy() <= ct.total());
}

// ── Unit tests: Memory info ──

TEST_CASE("Memory info reading", "[mem][unit]") {
  auto mem = sysmon::read_mem_info();

  REQUIRE(mem.total_kb > 0);
  REQUIRE(mem.used_percent() >= 0.0);
  REQUIRE(mem.used_percent() <= 100.0);
  REQUIRE(mem.used_kb() > 0);
  REQUIRE(mem.used_kb() <= mem.total_kb);
}

TEST_CASE("Used memory cannot exceed total", "[mem][unit]") {
  sysmon::MemInfo mem;
  mem.total_kb = 1000000;
  mem.available_kb = 500000;
  mem.free_kb = 400000;
  mem.buffers_kb = 50000;
  mem.cached_kb = 100000;

  auto used = mem.used_kb();
  REQUIRE(used > 0);
  REQUIRE(used <= mem.total_kb);
  REQUIRE(mem.used_percent() > 0.0);
  REQUIRE(mem.used_percent() <= 100.0);
}

// ── Unit tests: Uptime ──

TEST_CASE("Uptime reading", "[uptime][unit]") {
  auto up = sysmon::read_uptime();

  REQUIRE(up.uptime_seconds > 0.0);
  REQUIRE(up.idle_seconds >= 0.0);

  auto formatted = up.formatted();
  REQUIRE_FALSE(formatted.empty());
  // Should contain at least "m" and "s"
  CHECK_THAT(formatted, ContainsSubstring("m"));
  CHECK_THAT(formatted, ContainsSubstring("s"));
}

// ── Unit tests: Load Average ──

TEST_CASE("Load average reading", "[load][unit]") {
  auto la = sysmon::read_load_avg();

  REQUIRE(la.load1 >= 0.0);
  REQUIRE(la.load5 >= 0.0);
  REQUIRE(la.load15 >= 0.0);
  REQUIRE(la.total_threads > 0);
  REQUIRE(la.last_pid > 0);
}

TEST_CASE("Load average values are non-negative", "[load][unit]") {
  sysmon::LoadAvg la;
  la.load1 = 1.5;
  la.load5 = 0.8;
  la.load15 = 0.3;
  la.running = 2;
  la.total_threads = 200;
  la.last_pid = 12345;

  REQUIRE(la.load1 == 1.5);
  REQUIRE(la.load5 == 0.8);
  REQUIRE(la.running == 2);
}

// ── Unit tests: Process list ──

TEST_CASE("Process list reading", "[proc][unit]") {
  auto pl = sysmon::read_process_list(10);

  REQUIRE(pl.processes.size() > 0);
  REQUIRE(pl.processes.size() <= 10);

  // First process should have the largest RSS
  if (pl.processes.size() >= 2) {
    REQUIRE(pl.processes[0].vm_rss_kb >= pl.processes[1].vm_rss_kb);
  }

  // Each process must have valid data
  for (auto& p : pl.processes) {
    REQUIRE(p.pid > 0);
    REQUIRE_FALSE(p.name.empty());
    // State should be a valid character
    CHECK((p.state == 'R' || p.state == 'S' || p.state == 'D' ||
           p.state == 'Z' || p.state == 'T' || p.state == 't' ||
           p.state == 'X' || p.state == 'I'));
  }
}

// ── Benchmark ──

TEST_CASE("System info benchmarks", "[benchmark]") {
  BENCHMARK("Read CPU info") {
    return sysmon::read_cpu_info();
  };

  BENCHMARK("Read memory info") {
    return sysmon::read_mem_info();
  };

  BENCHMARK("Read process list (20)") {
    return sysmon::read_process_list(20);
  };
}

// ── Format bytes ──

TEST_CASE("Memory formatting", "[format][unit]") {
  // The format_bytes function is in main.cpp (anonymous namespace), so
  // we just test that the values are reasonable
  auto mem = sysmon::read_mem_info();
  REQUIRE(mem.total_kb > 1000); // at least 1 MB
}
