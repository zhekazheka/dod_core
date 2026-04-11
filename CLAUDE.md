# dod_core

A foundational, project-agnostic ECS (Entity-Component-System) library. Uses EnTT as the storage backend.

## Architecture

- **Entity** — Thin wrapper over `entt::entity`
- **World** — Thin wrapper over `entt::registry`
- **Query types** — `Read<T>`, `Write<T>`, `WorldRead`, `WorldWrite`
- **Systems** — Stateless functions whose parameters declare resource access
- **SystemGraph** — Builds a DAG from resource conflicts for automatic parallelism
- **Scheduler** — Parallel topological execution via `std::jthread` thread pool

Components and systems are fully extensible from outside the library.

## Build

- Static library output: `libdod_core.a`
- Dependencies: EnTT, Google Test (both via CMake FetchContent)
- Downstream consumption: `add_subdirectory()` or `find_package(dod_core)`

## Implementation Plan

Detailed plan with directory structure, API design, and phased implementation: `.claude/claude-plans/implementation_plan.md` (in the solution root).
