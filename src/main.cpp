#include <sysmon/sysmon.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <format>
#include <thread>

using namespace ftxui;
using namespace std::chrono_literals;

namespace {

std::atomic<bool> g_running{true};

void handle_signal(int) {
  g_running = false;
}

// Build a styled horizontal bar for percentage values
Element percent_bar(double pct, Decorator fg, int width = 20) {
  int filled = std::clamp(static_cast<int>(std::round(pct / 100.0 * width)), 0, width);
  auto fill = text(std::string(filled, '|')) | fg;
  auto empty = text(std::string(width - filled, '.')) | color(Color::GrayDark);
  return hbox(Elements{std::move(fill), std::move(empty)});
}

// Format bytes to human-readable string
std::string format_bytes(uint64_t kb) {
  if (kb < 1024) return std::format("{} KB", kb);
  auto mb = static_cast<double>(kb) / 1024.0;
  if (mb < 1024.0) return std::format("{:.1f} MB", mb);
  auto gb = mb / 1024.0;
  return std::format("{:.2f} GB", gb);
}

// Select color decorator based on percentage
Decorator pct_color(double pct) {
  return (pct > 80.0) ? color(Color::Red) :
         (pct > 50.0) ? color(Color::Yellow) : color(Color::Green);
}

class SysmonApp {
public:
  SysmonApp() {
    setup_logging();
    // Initial CPU snapshot (two reads to get delta)
    cpu_info_ = sysmon::read_cpu_info();
    for (auto& cpu : cpu_info_.cpus) {
      cpu.previous = cpu.current;
    }
    std::this_thread::sleep_for(100ms);
    cpu_info_ = sysmon::read_cpu_info();
  }

  void run() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer([this] {
      return render_ui();
    });

    // Background thread for refreshing data every 1s
    std::thread refresh_thread([&] {
      while (g_running) {
        std::this_thread::sleep_for(1000ms);
        refresh();
        screen.PostEvent(Event::Custom);
      }
    });

    screen.Loop(renderer);
    g_running = false;
    if (refresh_thread.joinable()) {
      refresh_thread.join();
    }
  }

private:
  sysmon::CpuInfo cpu_info_;
  sysmon::MemInfo mem_info_;
  sysmon::LoadAvg load_avg_;
  sysmon::UptimeInfo uptime_;
  sysmon::ProcessList procs_;

  void setup_logging() {
    auto console = spdlog::stdout_color_mt("sysmon");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::warn);
  }

  void refresh() {
    // Rotate CPU snapshots
    for (auto& cpu : cpu_info_.cpus) {
      cpu.previous = cpu.current;
    }
    cpu_info_ = sysmon::read_cpu_info();

    mem_info_ = sysmon::read_mem_info();
    load_avg_ = sysmon::read_load_avg();
    uptime_ = sysmon::read_uptime();
    procs_ = sysmon::read_process_list(20);
  }

  Element render_ui() {
    // ── CPU Section ──
    Elements cpu_lines;
    for (auto& cpu : cpu_info_.cpus) {
      double pct = cpu.usage();
      auto label = cpu.current.cpu_id;
      if (label == "cpu") label = "total";

      // Color the percentage number too
      auto pct_str = std::format("{:6} {:5.1f}% ", label, pct);
      auto num_text = text(pct_str) | pct_color(pct);

      cpu_lines.push_back(
        hbox(Elements{
          num_text | size(WIDTH, GREATER_THAN, 18),
          percent_bar(pct, pct_color(pct), 30),
        })
      );
    }

    auto cpu_section = window(
      text(" CPU Usage ") | bold | color(Color::Cyan),
      vbox(std::move(cpu_lines))
    );

    // ── Memory Section ──
    double mem_pct = mem_info_.used_percent();
    auto mem_name = format_bytes(mem_info_.used_kb());
    auto mem_total = format_bytes(mem_info_.total_kb);

    auto mem_section = window(
      text(" Memory ") | bold | color(Color::Cyan),
      vbox(Elements{
        text(std::format(" Used:  {:>7} / {:<7} ({:5.1f}%)", mem_name, mem_total, mem_pct)),
        percent_bar(mem_pct, pct_color(mem_pct), 40),
        hbox(Elements{
          text(std::format(" Free:  {}", format_bytes(mem_info_.free_kb))),
          separator(),
          text(std::format(" Avail: {}", format_bytes(mem_info_.available_kb))),
        }),
        hbox(Elements{
          text(std::format(" Buff:  {}", format_bytes(mem_info_.buffers_kb))),
          separator(),
          text(std::format(" Cache: {}", format_bytes(mem_info_.cached_kb))),
        }),
        separator(),
        text(std::format(" Swap:  {}/{} ({:.1f}%)",
            format_bytes(mem_info_.swap_total_kb - mem_info_.swap_free_kb),
            format_bytes(mem_info_.swap_total_kb),
            mem_info_.swap_total_kb > 0
              ? 100.0 * (1.0 - static_cast<double>(mem_info_.swap_free_kb) / mem_info_.swap_total_kb)
              : 0.0)),
      })
    );

    // ── System Info Section ──
    auto sys_section = window(
      text(" System ") | bold | color(Color::Cyan),
      vbox(Elements{
        text(std::format(" Uptime: {}", uptime_.formatted())),
        text(std::format(" Load:   {:.2f} / {:.2f} / {:.2f}",
            load_avg_.load1, load_avg_.load5, load_avg_.load15)),
        text(std::format(" Procs:  {} running, {} total",
            load_avg_.running, load_avg_.total_threads)),
        text(std::format(" Model:  {}", cpu_info_.model)),
        text(std::format(" Cores:  {}", cpu_info_.core_count)),
      })
    );

    // ── Processes Section ──
    Elements proc_rows;
    proc_rows.push_back(
      hbox(Elements{
        text(std::format("{:>6}", "PID")) | size(WIDTH, GREATER_THAN, 7),
        text(std::format("{:<24}", "NAME")) | size(WIDTH, GREATER_THAN, 25),
        text(std::format("{:>3}", "ST")) | size(WIDTH, GREATER_THAN, 4),
        text(std::format("{:>9}", "RSS")),
      }) | bold | color(Color::GrayLight)
    );
    proc_rows.push_back(separator());
    for (auto& p : procs_.processes) {
      proc_rows.push_back(
        hbox(Elements{
          text(std::format("{:>6} ", p.pid)) | size(WIDTH, GREATER_THAN, 7),
          text(std::format("{:<24}", p.name)) | size(WIDTH, GREATER_THAN, 25),
          text(std::format("{:>3}", p.state)) | size(WIDTH, GREATER_THAN, 4),
          text(std::format("{:>9}", format_bytes(p.vm_rss_kb))),
        })
      );
    }

    auto proc_section = window(
      text(std::format(" Processes ({}) ", procs_.processes.size())) | bold | color(Color::Cyan),
      vbox(std::move(proc_rows))
    );

    // ── Layout ──
    auto main_layout = hbox(Elements{
      vbox(Elements{
        cpu_section,
        mem_section,
        sys_section,
      }) | flex_grow,
      separator(),
      proc_section | flex_grow,
    });

    // Header
    auto header = hbox(Elements{
      text(" ⚡ System Monitor ") | bold | color(Color::Yellow),
      separator(),
      text(std::format(" {} ", uptime_.formatted())) | color(Color::GrayDark),
      filler(),
      text(" Ctrl+C to exit ") | color(Color::GrayDark),
    });

    auto content = vbox(Elements{
      header | bold,
      separator(),
      main_layout | flex,
    });

    return content | border | size(WIDTH, GREATER_THAN, 100);
  }
};

} // anonymous namespace

int main() {
  spdlog::info("System Monitor starting...");
  SysmonApp app;
  app.run();
  spdlog::info("System Monitor stopped.");
  return 0;
}
