## Context & Goals

This project started as an attempt to offload my Civilization Simulation to the GPU. It quickly became clear that this was the wrong tool for the job: memory transfer overhead killed performance, and trying to write human logic in a GPU-friendly, branchless way was difficult. I moved the simulation entirely back to the CPU and kept the Vulkan codebase as a learning sandbox.

By far the hardest part of this project wasn't building any individual subsystem in isolation, but the cognitive load of holding all of them in my head at once—connecting them together, managing synchronization, pipeline barriers, and layout transitions. 

I approached this as a systems programming learning project rather than as a domain expert in fluid dynamics, audio DSP, or Vulkan internals. Taking on all three simultaneously meant constantly running into blind spots—often stumbling over things that an experienced graphics or engine programmer would consider second nature. To make sense of it all and keep track of how the hardware actually ticks, I ended up filling ~40 pages of notes and diagrams.

This is not a polished production engine; it is a personal sandbox built to hit those walls head-on and build an intuitive mental model of low-level systems.

### Goals:
* **Subsystem Integration:** Connecting Windows WASAPI audio capture, real-time FFT, fluid solver, and particle physics into a single frame loop.
* **Vulkan Compute Pipeline:** Getting hands-on experience with Vulkan 1.3 compute pipelines, synchronization, and performance optimization.
* **Performance on Integrated Graphics:** Exploring how far I can push the project on integrated GPU

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

> **Video quality:** Videos are compressed for web presentation and may not represent the original rendering quality. This is particularly noticeable in the `FluidParticlesMusic` demo.

### 1. MouseParticles

Particles are steered using the mouse as a gravity source.

**Stress tests:**

* **300k particles** — particle size 10, tail length 50 → ~60 FPS
[▶️ Watch demo(Streamable)](https://streamable.com/ap9luv)

* **3M particles** — particle size 1, no tail → ~60 FPS
[▶️ Watch demo(Streamable)](https://streamable.com/z5cwsn)

---

### 2. MusicParticles

Particles are steered in a circular motion based on music.
[▶️ Watch demo(Streamable)](https://streamable.com/5gp2dn)

---

### 3. FluidMouse

A `FluidSystem` simulation where fluid color and velocity are injected using the mouse.
[▶️ Watch demo(Streamable)](https://streamable.com/idljxs)

---

### 4. FluidParticles

`FluidSystem` merged with `ParticleSystem`.

Particles inject color and velocity into the fluid, while the fluid velocity is injected back into the particles.

> **Note:** Performance depends heavily on particle speed. For example, if a particle travels 35 tiles in a single frame, it needs to inject its contribution along the entire trajectory.

**Stress tests:**

* **500k fluid particles** → ~60 FPS
[▶️ Watch demo(Streamable)](https://streamable.com/ujhf34)

* **5M fluid particles** → ~25 FPS
[▶️ Watch demo(Streamable)](https://streamable.com/8a6hr8)

---

### 5. FluidParticlesMusic

Particles inject velocity and color into the fluid, while particle movement is steered by music.

**Stress tests:**

* **4K UHD fluid resolution** — 60 FPS, full screen simulated
* **5K fluid resolution** — 60 FPS, pressure simulation at half resolution; unused sides are cut

[▶️ Watch demo(Streamable)](https://streamable.com/6ucqer)
