# UE5 RDG Cloth Simulation Plugin

A personal Unreal Engine 5 experiment implementing **GPU cloth simulation** as a custom **Plugin**, using the **Render Dependency Graph (RDG)** and compute shaders.

This project intentionally *reinvents the wheel* to better understand:

* How cloth solvers work internally
* How to write and dispatch compute shaders in UE5
* How to integrate GPU simulation into the RDG pipeline
* How rendering, buffers, and readback interact in practice

## Features

* Custom UE5 Plugin-based architecture
* Compute shader cloth solver
* RDG pass integration for GPU execution
* Test scene for quick verification

## Purpose

This is a learning-focused technical project exploring low-level graphics and simulation workflow in Unreal Engine, rather than relying on built-in cloth systems.

## How to Run

1. Clone the repository
2. Open the `.uproject` file in Unreal Engine 5
3. Build the project if prompted
4. Open the demo level and press **Play**

## Notes

* Built for experimentation and understanding GPU simulation pipelines
* Not intended as a production-ready cloth system
