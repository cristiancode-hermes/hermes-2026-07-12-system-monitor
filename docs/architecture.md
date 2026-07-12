# Architecture — System Monitor

```
┌─────────────────────────────────────────────────────────────┐
│                     FTXUI TUI Screen                        │
│  ┌─────── CPU ───────┐  ┌──── Processes ────────────────┐  │
│  │  total   45.2% ███ │  │  PID  NAME                  ST│  │
│  │  cpu0    12.3% ██  │  │    1  systemd               S │  │
│  │  cpu1    78.9% ████│  │  789  chromium               S │  │
│  └────────────────────┘  └──────────────────────────────┘  │
│  ┌──── Memory ─────────┐                                   │
│  │  Used: 2.5/8.0 GB   │                                   │
│  └─────────────────────┘                                   │
└─────────────────────────────────────────────────────────────┘
```

## Layers

1. **sysmon::read_* functions** (`src/sysmon.cpp`)
   - Parse `/proc/stat`, `/proc/meminfo`, `/proc/uptime`, `/proc/loadavg`, `/proc/[pid]/`
   - Return plain structs with no external dependencies beyond `<fstream>`, `<charconv>`
   - No error handling via exceptions — returns empty/default structs on failure

2. **SysmonApp class** (`src/main.cpp`)
   - Owns the data snapshots and refresh loop
   - Converts raw data into FTXUI `Element` trees
   - Runs in a background thread calling `refresh()` every 1s

3. **FTXUI rendering**
   - Fullscreen interactive screen
   - `Renderer` component rebuilds the DOM tree each frame
   - Timer-based refresh via `ScreenInteractive::PostEvent(Event::Custom)`

## Data flow

```
/proc/* files → read_file() → parse → struct → FTXUI Element → Screen
                    ↑                                ↓
              refresh() ← background thread    Event::Custom
```

## Design decisions

- **No exceptions in /proc parsing**: If a file doesn't exist or can't parse, the struct returns zeros/empty rather than crashing
- **Snapshots for CPU delta**: CPU usage requires two readings with an interval. The first reading initializes `previous`, the second computes usage
- **Process list sorted by RSS**: Most memory-hungry processes first for quick identification
- **FetchContent for dependencies**: No system package manager needed — CI-friendly
