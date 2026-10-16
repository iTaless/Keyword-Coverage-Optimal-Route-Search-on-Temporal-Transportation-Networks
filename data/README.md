# Released network snapshots

Run from the repository root before experiments or release validation:

```bash
python3 scripts/prepare_data.py
```

`archive/` stores the complete raw and processed Austin/Houston inputs as ordered compressed parts. `archive/manifest.json` records the part checksums and the original paths, sizes and SHA-256 values of all network files. The script verifies the parts and restores the exact original bytes under `data/raw/` and `data/processed/`; it refuses to overwrite a differing local file. No network download or third-party Python package is needed.

Check restored files later with `python3 scripts/prepare_data.py --check`. Keep all archive parts together. This changes storage only; query files, algorithms, scoring rules and measurements are unchanged.
