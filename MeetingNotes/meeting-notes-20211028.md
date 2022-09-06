# Oct 28th, 2021

### Quantitative I/O resource management

- I/O cycles, N cycles, 1.7 us one-way latency on Frontera, IB HDR

- Cycles per operation
  - buffer size, 600K, turning point from latency dominant to throughput dominant
  - dependency, need a way to quantify the dependency impact on I/O cycles needed, as it has to preserve the partial order and can not be parallelized

### Consistency Model

- go over the IO trace data, 
- identify which one would benefit

### Applications

- Specfem3D