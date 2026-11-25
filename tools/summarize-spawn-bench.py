#!/usr/bin/env python3
"""Extract and summarize the 40 benchmark batches from a QEMU transcript."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path
import statistics

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', nargs='?', default='results/single-cpu.log')
parser.add_argument('--csv', default='results/benchmark.csv')
parser.add_argument('--report', default='docs/BENCHMARK_RESULTS.md')
args = parser.parse_args()
log = Path(args.log)
rows = []
for line in log.read_text().splitlines():
    if line.startswith('BENCH,') and not line.startswith('BENCH,heap'):
        _, size, trial, method, launches, ticks = line.split(',')
        rows.append((int(size), int(trial), method, int(launches), int(ticks)))
expected = {(s, t, m) for s in (0, 1, 4, 8) for t in range(1, 6)
            for m in ('fork_exec', 'spawn')}
if len(rows) != 40 or {r[:3] for r in rows} != expected:
    raise SystemExit('Incomplete or duplicate benchmark data; expected 40 unique batches')
if any(r[3] != 100 for r in rows):
    raise SystemExit('Unexpected batch size')
if 'spawnbench: COMPLETE' not in log.read_text():
    raise SystemExit('Benchmark did not complete')
with open(args.csv, 'w', newline='') as out:
    writer = csv.writer(out)
    writer.writerow(('heap_MiB', 'trial', 'method', 'launches', 'ticks'))
    writer.writerows(rows)
groups = defaultdict(list)
for size, trial, method, launches, ticks in rows:
    groups[size, method].append(ticks)
lines = ['# Recorded benchmark results', '',
         f'Source: `{log}`. Five batches of 100 launches per cell. All times are xv6 uptime ticks.', '',
         '| Extra parent heap (MiB) | Method | Mean batch ticks | Median | Range | Launches/tick (aggregate) |',
         '| ---: | --- | ---: | ---: | --- | ---: |']
for (size, method), values in sorted(groups.items()):
    throughput = 500 / sum(values) if sum(values) else float('inf')
    lines.append(f'| {size} | {method} | {statistics.mean(values):.1f} | '
                 f'{statistics.median(values)} | {min(values)}–{max(values)} | {throughput:.2f} |')
lines += ['', 'These are end-to-end launch/execute/wait measurements, including scheduling and cleanup.',
          'A tick is coarse: small differences at the smallest heap size should not be treated as precise speedups.',
          'Throughput is sequential completed launches per tick, not concurrent capacity.',
          'See BENCHMARK.md for controls, memory analysis, and limitations.', '']
Path(args.report).write_text('\n'.join(lines))
print('\n'.join(lines))
