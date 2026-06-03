# Stage 1: Builder (The Workshop)
FROM ubuntu:22.04 AS builder
# 💡 FIX: Added 'git' here so CMake can download GoogleTest!
RUN apt-get update && apt-get install -y cmake g++ make git
WORKDIR /app
COPY . .
# We build the application here
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# Stage 2: Runtime (The Showroom)
FROM ubuntu:22.04 AS runtime
WORKDIR /app
# We only copy the finished product and the config file!
COPY --from=builder /app/build/ThreadPoolApp .
COPY config.json .
CMD ["./ThreadPoolApp"]