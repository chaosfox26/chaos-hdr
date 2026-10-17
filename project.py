from pathlib import Path
import os

ROOT = Path(__file__).resolve().parent
SETTINGS = ROOT / 'launch-settings.json'


def atomic_write(path, text, mode=0o600):
    temporary = path.with_name(path.name + '.truehdr-tmp')
    descriptor = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_TRUNC | os.O_NOFOLLOW, mode)
    with os.fdopen(descriptor, 'w') as stream:
        os.fchmod(stream.fileno(), mode)
        stream.write(text)
    temporary.replace(path)
