#!/usr/bin/env python3
"""Run shell commands in QEMU and preserve their complete console output."""
import argparse
import os
from pathlib import Path
import selectors
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cpus', type=int, default=1)
parser.add_argument('--timeout', type=int, default=1200,
                    help='timeout per shell command, in seconds')
parser.add_argument('--log', default='results/qemu.log')
parser.add_argument('commands', nargs='*', default=['spawntest', 'spawnbench', 'usertests -q'])
args = parser.parse_args()
Path(args.log).parent.mkdir(parents=True, exist_ok=True)
proc = subprocess.Popen(['make', 'qemu', f'CPUS={args.cpus}'],
                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, start_new_session=True)
selector = selectors.DefaultSelector()
selector.register(proc.stdout, selectors.EVENT_READ)

with open(args.log, 'wb') as log:
    def prompt():
        output = bytearray()
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            for key, _ in selector.select(timeout=1):
                data = os.read(key.fileobj.fileno(), 65536)
                if not data:
                    raise RuntimeError('QEMU exited before shell prompt')
                log.write(data)
                log.flush()
                print(data.decode(errors='replace'), end='', flush=True)
                output.extend(data)
                if b'panic:' in output:
                    raise RuntimeError('kernel panic')
                if output.endswith(b'$ '):
                    return output.decode(errors='replace')
        raise TimeoutError('No shell prompt before timeout')

    try:
        prompt()
        for command in args.commands:
            proc.stdin.write((command + '\n').encode())
            proc.stdin.flush()
            output = prompt()
            expected = {'spawntest': 'spawntest: ALL PASSED',
                        'spawnbench': 'spawnbench: COMPLETE',
                        'usertests': 'ALL TESTS PASSED',
                        'usertests -q': 'ALL TESTS PASSED',
                        'forktest': 'fork test OK'}
            if command in expected and expected[command] not in output:
                raise RuntimeError(f'{command}: missing success marker')
    finally:
        proc.stdin.write(b'\x01x')
        proc.stdin.flush()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            import signal
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=5)
