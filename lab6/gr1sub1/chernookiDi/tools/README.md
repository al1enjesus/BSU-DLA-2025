Tools for benchmarking and plotting

1) `benchmark.sh` — minimal script to collect timings and write CSV.
2) `plot_stats.py` — simple Python script to plot CSV results (requires matplotlib).

Usage:

```bash
# run benchmark (ensure FS mounted at /mnt/fuse)
bash tools/benchmark.sh

# then plot results
python3 tools/plot_stats.py bench_results.csv
```
