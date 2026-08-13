# Embla OS

Embla is a userspace operating system inspired by Unix-like systems. It provides an environment where processes can be managed, scheduled, and executed with their own memory space and communication channels.

## Vision

The goal of Embla is to provide:
- **Process Management**: A robust system for creating, transitioning, and managing process states (Ready, Running, Waiting, etc.).
- **Scheduling**: Efficient scheduling algorithms to handle concurrent task execution.
- **Memory Management**: Dedicated memory management systems within the userspace environment.
- **File System & Syscalls**: A Virtual File System (VFS) and a syscall interface for standard OS interactions.

## Status

Currently, the project is in its initial scaffold phase with:
- Core Process Manager and Scheduler infrastructure.
- Memory Management and String Handling utilities.
- Basic logging system.
- Demonstration of task dispatching in `main.c`.

## Getting Started

To build the current snapshot:
```bash
make
```
The binary will be located in `build/embla`.
