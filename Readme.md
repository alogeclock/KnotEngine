# Knot Engine

> ⚠️ **This project is in its early stages.** Core systems are still being built, and many features described below are planned or in progress.

A 3D game engine project for Windows, built with Direct3D 11, ImGui, and modern C++20.

---

## Overview

SimpleEngine is a personal project aimed at building a fully-featured 3D game engine from scratch. The focus is on clean architecture, performance, and progressive complexity — starting from stable core systems and expanding toward advanced rendering and data-oriented design.

**Platform:** Windows only  
**Renderer:** Direct3D 11  
**Language:** C++20 (Visual Studio 2022)  
**SIMD:** SSE/AVX for raycast and culling optimization

---

## Features

### ✅ Key Implementation (Core Goals)

These are the foundational systems the engine must have to run.

- **Core Framework & Memory System** — Custom allocators, pooling, logging, and a high-performance math library (vectors, matrices, quaternions)
- **Scene Graph & Basic Rendering** — Transform hierarchy, RHI abstraction, mesh rendering, material system, and frustum culling
- **Resource & Serialization System** — Asset loading/management, property reflection, and serialization for editor-engine data consistency
- **Physics & Input System** — Collision detection, basic rigid body simulation, raycasting, and an event-driven input abstraction layer

### 🎯 Target Implementation (Stretch Goals)

Advanced features planned for future milestones.

- **Advanced Rendering** — PBR, Ray Marching, Ray Tracing, Render Graph, GPU Driven Rendering, dynamic LOD, and BVH acceleration structures
- **Multi-threaded Architecture** — Separated game/render threads with Render Proxy system, ECS, and a Job System for multi-core utilization
- **Workflow & Editor Tools** — Virtual File System (VFS), hot reloading, and a visual editor for shaders, materials, and meshes

---

## Getting Started

**Requirements:** Visual Studio 2022, Windows SDK

```bash
# 1. Clone with submodules
git clone --recursive https://github.com/your-repo/SimpleEngine.git

# 2. Install dependencies
scripts/Setup.bat

# 3. Generate solution and build
GenerateProjectFiles.bat
```

Open the generated `.sln` file in Visual Studio 2022 and build.

---

## Roadmap

| Phase | Focus |
|-------|-------|
| Phase 1 | Core systems, basic rendering, scene graph |
| Phase 2 | Physics, input, serialization |
| Phase 3 | Advanced rendering (PBR, Render Graph) |
| Phase 4 | Multi-threading, ECS, Job System |
| Phase 5 | Editor tooling, VFS |

Code readability and architectural integrity are maintained as first-class priorities throughout all phases.

---

## License

This project is for personal/educational use.