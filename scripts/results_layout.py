"""Canonical result tables and lossless consolidation of archival views."""
import collections
import copy
import csv
import hashlib
import io
import json
import shutil
import zipfile
from pathlib import Path

KEY = ('query_id', 'algorithm', 'route_rank')


def rows(path):
    with Path(path).open(newline='', encoding='utf-8-sig') as stream:
        return list(csv.DictReader(stream))


def write(path, records):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fields = list(dict.fromkeys(k for row in records for k in row))
    with path.open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(records)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def merge(base, extra, keys=KEY, prefix='', require_all=False):
    lookup = {tuple(r[k] for k in keys): r for r in extra}
    assert len(lookup) == len(extra), 'Duplicate join key'
    base_keys = {tuple(r[k] for k in keys) for r in base}
    assert set(lookup) <= base_keys, 'Join would discard an unmatched record'
    if require_all:
        assert set(lookup) == base_keys
    for row in base:
        found = lookup.get(tuple(row[k] for k in keys), {})
        for name, value in found.items():
            if name in keys:
                continue
            target = prefix + name
            if target in row:
                assert str(row[target]) == str(value), (target, row[target], value)
            row[target] = value
    return base


def bl_archive_rows(final, suffix):
    result = []
    with zipfile.ZipFile(final / 'source/bl_recovery.zip') as archive:
        for name in sorted(archive.namelist()):
            if name.endswith(suffix):
                result.extend(csv.DictReader(io.StringIO(archive.read(name).decode('utf-8-sig'))))
    return result


def consolidate_bl_archives(final, manifest):
    directory = final / 'source/bl_return_archives'
    if not directory.exists():
        return
    destination = final / 'source/bl_recovery.zip'
    source_info = []
    known = {Path(x['path']).name: x for x in manifest['supplementary_BL']['source_archives']}
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as combined:
        for index, path in enumerate(sorted(directory.glob('*.zip')), 1):
            prefix = f'part{index}/'
            entry = known[path.name]
            source_info.append({'original_archive_name': path.name,
                                'original_archive_sha256': entry.get('original_archive_sha256', entry['sha256']),
                                'member_prefix': prefix})
            with zipfile.ZipFile(path) as archive:
                for info in archive.infolist():
                    if info.is_dir():
                        continue
                    item = copy.copy(info)
                    item.filename = prefix + info.filename
                    combined.writestr(item, archive.read(info.filename))
    with zipfile.ZipFile(destination) as combined:
        assert combined.testzip() is None
    manifest['supplementary_BL']['original_batches'] = source_info
    manifest['archival_publication_copies'] = [x for x in manifest.get('archival_publication_copies', [])
                                              if '/bl_return_archives/' not in x.get('path', '')]
    shutil.rmtree(directory)


def arrange_final(root):
    final = Path(root) / 'results/final'
    if not (final / 'main_study.runtime.csv').exists():
        return
    manifest = json.loads((final / 'main_study.manifest.json').read_text())
    old_supp = final / 'bl_supplementary'
    recovery = rows(old_supp / 'bl_combined_703.runtime.csv') if old_supp.exists() else rows(final / 'bl_recovery/runtime.csv')
    if old_supp.exists():
        missing = rows(old_supp / 'queries_without_returned_routes.csv')
        merge(recovery, missing, ('query_id',), require_all=False)
    consolidate_bl_archives(final, manifest)
    write(final / 'bl_recovery/runtime.csv', recovery)

    for old, new in [('main_study.runtime.csv', 'runtime.csv'),
                     ('all_methods.routes.csv', 'routes.csv'),
                     ('main_study.runtime_summary.csv', 'runtime_summary.csv'),
                     ('main_study.validation.csv', 'validation/queries.csv')]:
        destination = final / new
        destination.parent.mkdir(parents=True, exist_ok=True)
        (final / old).replace(destination)

    route_checks = rows(final / 'audit/all_routes.time_and_score_audit.csv')
    merge(route_checks, rows(final / 'audit/score_rule_comparison.csv'), prefix='node_dwell_')
    merge(route_checks, bl_archive_rows(final, '.validation.csv'), prefix='bl_')
    write(final / 'validation/routes.csv', route_checks)
    quality = rows(final / 'coverage_quality.csv')
    merge(quality, rows(final / 'audit/coverage_quality.recorded_score_audit.csv'),
          ('query_id',), prefix='audit_', require_all=True)
    write(final / 'quality.csv', quality)

    summary = json.loads((final / 'main_study.summary.json').read_text())
    summary['audits'] = {
        'time_and_score': json.loads((final / 'audit/time_and_score_audit.summary.json').read_text()),
        'node_dwell': json.loads((final / 'audit/score_provenance.json').read_text()),
        'original_aap_schema': json.loads((final / 'audit/original_source_schema.json').read_text())}
    if old_supp.exists():
        summary['bl_recovery'] = json.loads((old_supp / 'summary.json').read_text())
    elif (final / 'summary.json').exists():
        summary['bl_recovery'] = json.loads((final / 'summary.json').read_text()).get('bl_recovery', {})
    summary['bl_recovery'].pop('supplementary_example_query_id', None)
    (final / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    manifest['schema_version'] = 2
    manifest['artifacts'] = {
        'historical_runtime': 'results/final/runtime.csv',
        'all_archived_routes': 'results/final/routes.csv',
        'runtime_groups': 'results/final/runtime_summary.csv',
        'historical_quality': 'results/final/quality.csv',
        'query_validation': 'results/final/validation/queries.csv',
        'route_validation': 'results/final/validation/routes.csv',
        'bl_recovery_runtime': 'results/final/bl_recovery/runtime.csv'}
    manifest['supplementary_BL']['metadata_location'] = 'results/final/source/bl_recovery.zip'
    manifest['supplementary_BL']['source_archives'] = [
        {'path': 'results/final/source/bl_recovery.zip', 'sha256': sha(final / 'source/bl_recovery.zip')}]
    manifest['normalizer_sha256'] = sha(Path(root) / manifest['normalizer'])
    manifest['integration_script_sha256'] = sha(Path(root) / manifest['integration_script'])
    manifest['layout_script'] = 'scripts/results_layout.py'
    manifest['layout_script_sha256'] = sha(__file__)
    (final / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

    for name in ['main_study.jsonl', 'main_study.routes.csv', 'main_study.manifest.json',
                 'main_study.summary.json', 'coverage_quality.csv', 'route_level.routes.csv',
                 'route_level.validation_details.csv', 'route_level.query_summary.csv',
                 'route_level.group_summary.csv', 'FORMAL_RESULT_MAPPING.md']:
        path = final / name
        if path.exists():
            path.unlink()
    for name in ['audit', 'bl_supplementary']:
        if (final / name).exists():
            shutil.rmtree(final / name)


def current_quality_summary(runtime, quality):
    ratios = sorted(float(r['ap_astar_ratio']) for r in quality if str(r['quality_eligible']) == '1')
    def percentile(p):
        if not ratios:
            return None
        position = (len(ratios) - 1) * p
        lower = int(position)
        return ratios[lower] + (ratios[min(lower + 1, len(ratios) - 1)] - ratios[lower]) * (position - lower)
    result = {'eligible': len(ratios), 'mean': sum(ratios) / len(ratios) if ratios else None,
              'median': percentile(.5), 'q1': percentile(.25), 'q3': percentile(.75),
              'equal': sum(abs(r - 1) <= 1e-10 for r in ratios), 'zero': sum(r == 0 for r in ratios)}
    for alg in ('A', 'AP'):
        completed = [r for r in runtime if r['algorithm'] == alg and r['status'] == 'ok']
        result[alg] = {'full_k': sum(int(r['returned_distinct_routes']) == int(r['requested_k']) for r in completed),
                       'route_producing_queries': len(completed),
                       'positive_score_queries': sum(float(r['best_kc']) > 0 for r in completed),
                       'zero_score_queries': sum(float(r['best_kc']) == 0 for r in completed)}
    return result


def arrange_current(root):
    directory = Path(root) / 'results/rechecked_aap'
    summary = json.loads((directory / 'summary.json').read_text())
    summary['quality'] = current_quality_summary(rows(directory / 'aap.runtime.csv'),
                                                  rows(directory / 'coverage_quality.all_queries.csv'))
    (directory / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    metadata = json.loads((directory / 'run_metadata.json').read_text())
    if (directory / 'input_identity.json').exists():
        identity = json.loads((directory / 'input_identity.json').read_text())
        assert all(metadata[k] == v for k, v in identity.items())
        (directory / 'input_identity.json').unlink()
    if (directory / 'quality_summary.json').exists():
        (directory / 'quality_summary.json').unlink()


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    arrange_final(root)
    arrange_current(root)
