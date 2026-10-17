
from pathlib import Path
import subprocess
import sys

if __name__=='__main__':
    root=Path(__file__).resolve().parent
    raise SystemExit(subprocess.run([sys.executable,str(root/'desktop_probe.py'),
        '--gpu-input','--controller','--selection','--watchdog','--occlusion'],cwd=root).returncode)
