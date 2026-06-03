# High-Performance C++ Thread Pool

A robust, production-ready thread pool implementation in C++11/14. 
Unlike basic academic thread pools, this project is designed for system stability and observability under heavy load. It features backpressure management, dynamic hot-swapping of worker threads, and an internal telemetry engine that tracks P95/P99 tail latencies in real-time.

## Architecture

```text
┌─────────────────────────────────────────────────────────────────┐
│                           Main Thread                           │
│                                                                 │
│  auto future = submit(Task) ◄────────┐ (Returns Result/Error)   │
└───────┬──────────────────────────────┼──────────────────────────┘
        │                              │
        ▼                              │
┌──────────────────────────────────────┼──────────────────────────┐
│                           ThreadPool │                          │
│                                      │                          │
│  [ Bounded Task Queue ] ───────────► [ Worker Threads (1..N) ]  │
│  (Rejects if full)                   │                          │
│                                      ▼                          │
│  [ Dynamic Config ]           [ Metrics & Telemetry Engine ]    │
│  (Hot-swap thread count)      (P50/P95/P99, Throughput, Drops)  │
└─────────────────────────────────────────────────────────────────┘