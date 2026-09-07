# Embla OS

Embla is a userspace operating system inspired by Unix-like systems. It provides an environment where processes can be created, supervised, and communicated with, built directly on top of the host operating system's own process primitives (`fork`/`exec`/`wait`, signals, `setrlimit`).

## Vision

The goal of Embla is to provide:

- **Process Management** — creating, transitioning, and supervising processes and process groups. ✅ Built.
- **Service Supervision** — a real init-system layer on top of process management: named services, restart policy, dependency-aware start/shutdown ordering, and a top-level supervisor loop. ✅ Built.
- **Scheduling** — dispatching ready processes for execution. ✅ Built (currently a simple ready-queue dispatcher, not a multi-algorithm scheduler).
- **IPC** — pipes, socket pairs, named Unix sockets, and a service-notification protocol for processes to report their own readiness/status. ✅ Built.
- **Resource Limits & Accounting** — memory/CPU/file-descriptor limits and per-group process-count caps, plus CPU-time and peak-memory accounting for supervised processes. ✅ Built (see "Platform notes" below — not every limit is enforceable on every platform).
- **Memory Management** — a dedicated memory-management subsystem within the userspace environment, distinct from the rlimit-based memory *limits* above. ❌ Not built.
- **File System & Syscalls** — a Virtual File System (VFS) and a syscall interface for standard OS interactions. ❌ Not built.

## Status

Embla currently provides:

- **Process & group management** — process creation, state transitions, process groups, orphan reparenting.
- **Scheduling & execution** — a ready-queue scheduler and an executor handling `fork`/`exec`/signal delivery/reaping.
- **IPC** — `Pipe`, `SocketPair`, `UnixListener`, `UnixDatagramSocket`, and a `sd_notify`-style service notification protocol (`READY=1`, `STOPPING=1`, `STATUS=<text>`).
- **Resource limits & accounting** — memory, CPU, and file-descriptor limits on individual processes; a process-count cap per group; CPU-time and peak-resident-memory accounting captured at reap time.
- **Service supervision** — the layer that makes Embla an actual init system, not just a process library:
  - **Registration**: named `Service` definitions, independent of any single running instance.
  - **Lifecycle**: start/stop/reap for one service, with a full state and last-exit history.
  - **Restart policy**: `RESTART_NEVER` / `RESTART_ON_FAILURE` / `RESTART_ALWAYS`, with real exponential backoff, a stability window that resets the backoff after a service has run stably, and a give-up threshold so a genuinely broken program doesn't retry forever.
  - **Dependency ordering**: services declare dependencies by name; a topological sort determines start order, with cycle and missing-dependency detection. Shutdown order is the exact reverse.
  - **Root-process behavior**: Embla's own host process installs `SIGTERM`/`SIGINT` handlers for orderly shutdown, and is robust to processes it never directly spawned showing up in its own wait queue (e.g. a supervised service's own grandchild, orphaned after the service itself exits) — the behavior expected of anything acting as PID 1.

Run `./build/embla` (after `make`) to see this in action: it registers one example service (a heartbeat printed every 2 seconds) and supervises it until you press Ctrl-C or send it `SIGTERM`, at which point it shuts down the service gracefully and exits.

**Not yet built:** there is no configuration-file format for defining services — they're currently defined directly against the public API in C, the way `main.c` does. Dependency ordering controls *sequencing* only (start B before A, stop A before B); it does not propagate failures (a crashing dependency does not automatically stop what depends on it — that's a materially different, unbuilt feature). Memory management and a filesystem/syscall layer remain future vision items, not yet started.

### Platform notes

Embla is built and tested on **Linux and macOS (Darwin, arm64)**. Almost everything behaves identically on both, with one confirmed exception: virtual-memory limiting (`RLIMIT_AS`) is unsupported on Darwin at the kernel level — `setrlimit()` rejects it outright, for any value. Embla surfaces this as a queryable capability rather than silently failing: code asking for a memory limit on a platform that can't enforce one gets a clear, immediate failure, never a silent no-op.

## Getting Started

Build:

```bash
make
```

Run the example program (registers and supervises a small heartbeat service until stopped):

```bash
./build/embla
```

Run the test suite:

```bash
make test
```

Clean:

```bash
make clean
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
