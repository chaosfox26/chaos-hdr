
import json
import re
import subprocess


def scan():
    result = subprocess.run(['kscreen-doctor', '-o'], capture_output=True, text=True,
                            timeout=3, check=True)
    text = re.sub(r'\x1b\[[0-9;]*m', '', result.stdout)
    displays = []
    for block in re.split(r'(?=Output: )', text):
        header = re.match(r'Output: (\d+) (\S+)', block)
        if not header or not re.search(r'^\s*enabled\s*$', block, re.M):
            continue
        if not re.search(r'^\s*connected\s*$', block, re.M):
            continue
        if not re.search(r'^\s*HDR:\s*enabled\s*$', block, re.M):
            continue
        peak = re.search(r'Peak brightness:\s*([\d.]+)\s*nits(?:[^\n]*?overridden with:\s*([\d.]+)\s*nits)?', block)
        if not peak:
            continue
        brightness = round(float(peak[2] or peak[1]))
        if brightness <= 0:
            continue
        priority = re.search(r'^\s*priority\s+(\d+)', block, re.M)
        geometry = re.search(r'Geometry:\s*(-?\d+),(-?\d+)\s+(\d+)x(\d+)', block)
        displays.append(dict(connector=header[2], peak_brightness=brightness,
                             source='OS override' if peak[2] else 'Monitor reported',
                             priority=int(priority[1]) if priority else 999,
                             geometry=[int(x) for x in geometry.groups()] if geometry else None))
    return sorted(displays, key=lambda item: item['priority'])


if __name__=='__main__':
    print(json.dumps(scan()))
