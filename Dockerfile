# Two-stage build: gcc:14 → debian slim
FROM gcc:14-bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt CMakePresets.json ./
COPY src/ include/ tests/ ./

RUN cmake --preset debug && cmake --build --preset debug

# Runtime stage
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/debug/sysmon /usr/local/bin/sysmon

ENTRYPOINT ["sysmon"]
