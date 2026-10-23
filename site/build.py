from pathlib import Path
import shutil

root = Path(__file__).resolve().parent.parent
destination = root / 'output' / 'site'
files = {name: 'site/' + name for name in (
    'index.html', 'style.css', 'site.js', 'assets/favicon.svg',
    'assets/chaos-hdr-social-preview.jpg',
)}
files['assets/chaos-hdr-sidebar.png'] = 'docs/images/chaos-hdr-sidebar.png'
for name in ('Barlow-Regular.ttf', 'Barlow-SemiBold.ttf', 'Audiowide-Regular.ttf',
             'OFL-Barlow.txt', 'OFL-Audiowide.txt'):
    files['assets/fonts/' + name] = 'vendor/fonts/' + name
if destination.exists():
    shutil.rmtree(destination)
for name, source in files.items():
    path = destination / name
    path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / source, path)
print(f'Built {len(files)} allowlisted website files.')
