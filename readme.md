## Idea

This project started as an experimental project for my Civilization Simulation. However, it turned out that GPU–CPU data synchronization wasn't worth the overhead, and the human simulation logic wasn't easy to implement in a completely branchless way.

I decided to return to the original CPU-based CivSim version without Vulkan, but was left with the `"VK"` folder.

---

# Benchmarks

## Test System Specifications

### 1. Processor — AMD Ryzen 5 5500U

* **Architecture:** Zen 2 (7 nm)
* **Cores / Threads:** 6 cores / 12 threads
* **Clock Speed:** 2.10 GHz base — 4.00 GHz boost
* **Power / TDP:** 15 W
* **Cache:**

  * L1: 384 KB (6 × 64 KB)
  * L2: 3 MB (6 × 512 KB)
  * L3: 8 MB (shared)
* **Vector Extensions:** AVX, AVX2, FMA3
* **Theoretical Peak Compute:** ~768 GFLOPS (FP32)

### 2. Graphics — Integrated AMD Radeon Vega 7

* **Shading Units / Cores:** 448 ALUs (7 Compute Units)
* **Clock Speed:** up to 1800 MHz
* **Peak Compute:** ~1.61 TFLOPS (FP32) / ~3.23 TFLOPS (FP16)
* **Memory:** Shared VRAM (system RAM)

### 3. Memory — 16 GB LPDDR4x

* **Configuration:** Dual-channel (2 × 8 GB soldered)
* **Speed:** 4266 MHz / MT/s

### 4. System

* **OS:** Windows 11 Home

> **Note:** This project uses WASAPI and Windows-specific extensions for Vulkan.

---

## Modules & Benchmarks

### 1. MouseParticles

Particles are steered using the mouse as a gravity source.

**Stress tests:**

* **300k particles** — particle size 10, tail length 50 → ~60 FPS
* **2M particles** — particle size 1, no tail → ~60 FPS

---

### 2. MusicParticles

Particles are steered in a circular motion based on music.

---

### 3. FluidMouse

A `FluidSystem` simulation where fluid color and velocity are injected using the mouse.

---

### 4. FluidParticles

`FluidSystem` merged with `ParticleSystem`.

Particles inject color and velocity into the fluid, while the fluid velocity is injected back into the particles.

> **Note:** Performance depends heavily on particle speed. For example, if a particle travels 35 tiles in a single frame, it needs to inject its contribution along the entire trajectory.

**Stress tests:**

* **500k fluid particles** → ~60 FPS
* **5M fluid particles** → ~25 FPS

---

### 5. FluidParticlesMusic

Particles inject velocity and color into the fluid, while particle movement is steered by music.

**Stress tests:**

* **4K UHD fluid resolution** — 60 FPS, full screen simulated
* **5K UHD fluid resolution** — 60 FPS, pressure simulation at half resolution; unused sides are cut