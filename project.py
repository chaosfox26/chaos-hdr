from pathlib import Path
import os
import tempfile

ROOT = Path(__file__).resolve().parent
SETTINGS = ROOT / 'launch-settings.json'


def atomic_write(path, text, mode=0o600):
    path=Path(path)
    descriptor, name=tempfile.mkstemp(prefix='.'+path.name+'.',suffix='.truehdr-tmp',dir=path.parent)
    temporary=Path(name)
    try:
        with os.fdopen(descriptor,'w') as stream:
            os.fchmod(stream.fileno(),mode)
            stream.write(text);stream.flush();os.fsync(stream.fileno())
        temporary.replace(path)
        directory=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(directory)
        finally:os.close(directory)
    finally:temporary.unlink(missing_ok=True)
