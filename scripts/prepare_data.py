"""Restore the released network snapshots and verify their original SHA-256 values."""
import argparse
import hashlib
import json
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / 'data/archive'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check restored files without extracting.')
    args = parser.parse_args()
    manifest = json.loads((ARCHIVE / 'manifest.json').read_text())
    targets = {}
    for item in manifest['files']:
        target = (ROOT / item['path']).resolve()
        if ROOT not in target.parents or not item['path'].startswith(('data/raw/', 'data/processed/')):
            raise SystemExit('Unexpected archive path: ' + item['path'])
        targets[item['path']] = (target, item)
        if target.exists() and (target.stat().st_size != item['size'] or digest(target) != item['sha256']):
            raise SystemExit('Existing file differs from the release: ' + item['path'])
    if args.check:
        missing = [name for name, (path, _) in targets.items() if not path.is_file()]
        if missing:
            raise SystemExit('Missing released files; run python3 scripts/prepare_data.py: ' + ', '.join(missing))
        print('Verified %d released network files.' % len(targets))
        return
    with tempfile.TemporaryFile() as combined:
        archive_hash = hashlib.sha256()
        for part in manifest['parts']:
            path = ARCHIVE / part['name']
            if path.parent != ARCHIVE or path.stat().st_size != part['size'] or digest(path) != part['sha256']:
                raise SystemExit('Archive part failed verification: ' + part['name'])
            with path.open('rb') as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b''):
                    archive_hash.update(block)
                    combined.write(block)
        if archive_hash.hexdigest() != manifest['archive_sha256']:
            raise SystemExit('Combined archive failed verification.')
        combined.seek(0)
        with zipfile.ZipFile(combined) as source:
            if set(source.namelist()) != set(targets) or len(source.namelist()) != len(targets):
                raise SystemExit('Archive member list differs from the manifest.')
            for name, (target, item) in targets.items():
                if target.exists():
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                temporary = target.with_name(target.name + '.extracting')
                h = hashlib.sha256()
                try:
                    with source.open(name) as incoming, temporary.open('wb') as outgoing:
                        for block in iter(lambda: incoming.read(1024 * 1024), b''):
                            h.update(block)
                            outgoing.write(block)
                    if temporary.stat().st_size != item['size'] or h.hexdigest() != item['sha256']:
                        raise SystemExit('Restored file failed verification: ' + name)
                    temporary.replace(target)
                finally:
                    temporary.unlink(missing_ok=True)
    print('Prepared and verified %d released network files.' % len(targets))


if __name__ == '__main__':
    main()
