"""Summarize actual runtime CSV; never infers UI/power verification from metrics."""
import argparse
import csv
import json
import math
from pathlib import Path


def percentile(values, fraction):
    return sorted(values)[max(0, math.ceil(len(values) * fraction) - 1)] if values else None


def summarize(path, warmup):
    rows = [{key: float(value) for key, value in row.items()} for row in csv.DictReader(path.open(encoding='utf-8'))]
    if not rows:
        raise ValueError('no metric samples')
    active, cpu, weights, paused_deltas = [], [], [], []
    for previous, row in zip(rows, rows[1:]):
        delta = row['elapsed_seconds'] - previous['elapsed_seconds']
        if delta <= 0:
            raise ValueError('non-increasing metric timestamps')
        if previous['elapsed_seconds'] >= warmup:
            active.append(row)
            cpu.append(row['cpu_one_core_percent'])
            weights.append(delta)
        if previous['render_paused'] and row['render_paused']:
            paused_deltas.append(row['render_submissions'] - previous['render_submissions'])
    last = rows[-1]
    return {'source': str(path), 'warmup_seconds': warmup, 'duration_seconds': last['elapsed_seconds'],
            'measured_seconds_after_warmup': sum(weights),
            'cpu_one_core_average_percent': sum(c*w for c, w in zip(cpu, weights))/sum(weights) if weights else None,
            'cpu_one_core_p95_percent': percentile(cpu, .95), 'cpu_one_core_max_percent': max(cpu) if cpu else None,
            'working_set_max_bytes_after_warmup': max((r['working_set_bytes'] for r in active), default=None),
            'private_max_bytes_after_warmup': max((r['private_bytes'] for r in active), default=None),
            'peak_working_set_bytes_including_startup': max(r['peak_working_set_bytes'] for r in rows),
            'decode_count': last['decode_count'], 'render_submissions': last['render_submissions'],
            'switch_count': last['switch_count'], 'process_handles_first_last': [rows[0]['process_handles'], last['process_handles']],
            'gdi_objects_first_last': [rows[0]['gdi_objects'], last['gdi_objects']],
            'observed_paused_interval_submission_deltas': paused_deltas,
            'submissions_while_paused': last['submissions_while_paused'],
            'validation_scope': 'CSV observations only; actual drag/DPI/lock/suspend and A-3 require separate evidence'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('csv', type=Path)
    parser.add_argument('--warmup', type=float, default=60)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.warmup < 0:
        parser.error('warmup must be nonnegative')
    result = json.dumps(summarize(args.csv, args.warmup), indent=2)
    if args.output:
        args.output.write_text(result + '\n', encoding='utf-8')
    print(result)


if __name__ == '__main__':
    main()
