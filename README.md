# ⚡ System Monitor — Terminal TUI para Linux

Monitor de sistema en tiempo real para terminal, desarrollado en **C++23** con **FTXUI**.

![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus)
![FTXUI](https://img.shields.io/badge/FTXUI-5.0-00BCD4)
![License](https://img.shields.io/badge/license-MIT-green)

## 📋 Características

- **CPU**: Uso por núcleo en tiempo real con barras de colores
- **Memoria**: RAM usada, libre, disponible, buffers, caché, swap
- **Sistema**: Uptime, load average, modelo de CPU, número de núcleos
- **Procesos**: Lista ordenada por consumo de memoria (PID, nombre, estado, RSS)
- **Actualización**: Refresh automático cada 1 segundo
- **Interfaz**: TUI limpia con particiones, colores y bordes vía FTXUI

## 🚀 Compilación

### Requisitos
- GCC 14+ o Clang 18+
- CMake 3.28+
- Ninja

### Compilar y ejecutar

```bash
# Configurar (debug)
cmake --preset debug

# Compilar
cmake --build --preset debug

# Ejecutar
./build/debug/sysmon
```

### Tests

```bash
ctest --preset debug
```

### Docker

```bash
docker build -t sysmon .
docker run --rm -it --pid=host sysmon
```

## 🏗️ Estructura

```
2026-07-12-system-monitor/
├── CMakeLists.txt          # Build principal (C++23, CMake 3.28+)
├── CMakePresets.json       # v9, debug (asan+ubsan), release (LTO)
├── Dockerfile              # multi-stage gcc:14 → debian slim
├── vcpkg.json              # manifest mode
├── .gitignore
├── README.md
├── src/
│   ├── main.cpp            # Entry point + TUI con FTXUI
│   └── sysmon.cpp          # Implementación del lector de /proc
├── include/
│   └── sysmon/
│       └── sysmon.hpp      # API pública
├── tests/
│   ├── CMakeLists.txt
│   └── test_sysmon.cpp     # Tests + benchmarks con Catch2
└── docs/
    └── architecture.md
```

## 🔧 APIs expuestas

| Función | Descripción |
|---------|-------------|
| `read_cpu_info()` | Uso de CPU total y por núcleo con snapshots |
| `read_mem_info()` | Memoria: total, free, available, buffers, cached, swap |
| `read_uptime()` | Tiempo de actividad del sistema |
| `read_load_avg()` | Load average 1/5/15 min, procesos activos |
| `read_process_list(n)` | Top-N procesos por consumo de memoria |

## 📊 Dependencias (FetchContent)

- **FTXUI** — Terminal UI library (v5.0.0)
- **spdlog** — Logging header-only
- **Catch2** — Test framework (v3.8.0)

## 📸 Screenshot

![System Monitor](https://raw.githubusercontent.com/cristiancode-hermes/hermes-2026-07-12-system-monitor/main/docs/screenshot.png)
