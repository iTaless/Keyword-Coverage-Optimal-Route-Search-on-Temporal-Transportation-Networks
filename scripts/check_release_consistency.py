import ast
import collections
import csv
import hashlib
import json
import zipfile
from pathlib import Path

root=Path(__file__).resolve().parents[1]
final=root/'results/final'
def rows(p):
    with p.open(newline='',encoding='utf-8-sig') as f:return list(csv.DictReader(f))
source=rows(final/'source/runtime_results_3600.csv')
runtime=rows(final/'main_study.runtime.csv')
original={(r['query_id'],'A' if r['algorithm']=='A*' else r['algorithm']):r for r in source}
assert len(original)==len(runtime)==3600
assert len({(r['query_id'],r['algorithm']) for r in runtime})==3600
for r in runtime:
    s=original[r['query_id'],r['algorithm']]
    for field in ('status','runtime_sec','timeout_threshold_sec'):assert r[field]==s[field]
counts=collections.Counter((r['algorithm'],r['status']) for r in runtime)
assert counts=={('BL','completed'):653,('BL','timeout'):547,('A','completed'):1163,('A','timeout'):37,('AP','completed'):1200}
routes=rows(final/'main_study.routes.csv');all_routes=rows(final/'all_methods.routes.csv')
assert len(routes)==27181 and len(all_routes)==28342
assert len({(r['query_id'],r['algorithm'],r['route_rank']) for r in all_routes})==28342
assert collections.Counter(r['algorithm'] for r in all_routes)=={'A':11687,'AP':11088,'BL':5567}
keys=collections.Counter((r['query_id'],r['algorithm']) for r in routes)
records=[json.loads(line) for line in (final/'main_study.jsonl').read_text().splitlines() if line.strip()]
assert len(records)==3600
for r in records:assert len(r['solutions'])==keys[r['query_id'],r['algorithm']]
assert sum(len(r['solutions']) for r in records)==27181
for r in runtime:
    if r['route_sequences_available']=='1':assert int(r['returned_distinct_routes'])==keys[r['query_id'],r['algorithm']]
bl=[r for r in runtime if r['algorithm']=='BL']
assert sum(r['supplementary_status']=='ok' for r in bl)==634
assert sum(r['supplementary_status']=='no_route' for r in bl)==36
assert sum(r['supplementary_status']=='timeout' for r in bl)==33
assert sum(r['supplementary_status']=='' for r in bl)==497
summary=rows(final/'main_study.runtime_summary.csv')
assert len(summary)==120 and sum(int(r['n']) for r in summary)==3600
assert sum(int(r['completed']) for r in summary)==3016
assert sum(int(r['timeout']) for r in summary)==584
audit=json.loads((final/'audit/time_and_score_audit.summary.json').read_text())
assert audit['recorded_quality_queries_with_any_raw_clock_score_mismatch']==81
assert audit['recorded_quality_queries_with_changed_best_score']==56
for alg,n in [('A',11687),('AP',11088),('BL',5567)]:
    assert audit['route_audit_by_algorithm'][alg]['raw_integer_clock_structural_pass']==n
    assert audit['route_audit_by_algorithm'][alg]['processed_seconds_structural_pass']==0
quality=rows(final/'coverage_quality.csv')
assert len(quality)==240
assert abs(sum(float(r['ap_astar_ratio']) for r in quality)/240-.936833585471725)<1e-12
for p in root.rglob('*.py'):ast.parse(p.read_text(encoding='utf-8-sig'))
manifest=json.loads((final/'main_study.manifest.json').read_text())
for r in manifest['source_files']+manifest['published_implementation_files']:
    assert hashlib.sha256((root/r['path']).read_bytes()).hexdigest()==r['sha256']
assert hashlib.sha256((root/manifest['normalizer']).read_bytes()).hexdigest()==manifest['normalizer_sha256']
assert hashlib.sha256((root/manifest['integration_script']).read_bytes()).hexdigest()==manifest['integration_script_sha256']
figures=list((final/'figures').glob('*'))
if figures:
    assert len(figures)==28 and len(list((final/'figures').glob('*.eps')))==8
for archive in manifest['supplementary_BL']['source_archives']:
    path=root/archive['path']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==archive['sha256']
    with zipfile.ZipFile(path) as source_zip:
        assert source_zip.testzip() is None
fresh=json.loads((root/'results/rechecked_aap/summary.json').read_text())
assert fresh['executions']==2520 and fresh['query_count']==1260
assert fresh['validation_failures']==fresh['ratios_above_one']==0
new_runtime=rows(root/'results/rechecked_aap/aap.runtime.csv')
new_routes=rows(root/'results/rechecked_aap/aap.routes.csv')
new_validation=rows(root/'results/rechecked_aap/aap.validation.csv')
assert len(new_runtime)==len(new_validation)==2520
assert len(new_routes)==fresh['returned_routes']==2080
assert len({(r['query_id'],r['algorithm']) for r in new_runtime})==2520
assert collections.Counter(r['status'] for r in new_runtime)=={'ok':362,'no_route':2104,'resource_limit':54}
assert sum(int(r['route_records_checked']) for r in new_validation)==2080
assert collections.Counter(r['validation_status'] for r in new_validation)=={'passed':2466,'not_checked':54}
identity=json.loads((root/'results/rechecked_aap/input_identity.json').read_text())
assert identity['source_sha256']==hashlib.sha256((root/'src/main.cpp').read_bytes()).hexdigest()
assert identity['query_sha256']==hashlib.sha256((root/'queries/final_queries_1260.csv').read_bytes()).hexdigest()
for city,inputs in identity['datasets'].items():
    for name,value in inputs.items():
        assert value==hashlib.sha256((root/f'data/processed/{city}/{name}').read_bytes()).hexdigest()
small=json.loads((root/'validation/seconds_instances/summary.json').read_text())
assert small['instances']==small['A_matches']==small['BL_matches']==small['AP_feasibility_pass']==20
assert small['compared_ranks']==200
assert len(rows(root/'validation/seconds_instances/topk_comparison.csv'))==200
report={'archive_table_consistency':'passed','formal_runtime_cells_preserved':3600,
        'main_routes':27181,'all_methods_routes':28342,'BL_supplementary_routes':5567,
        'source_and_implementation_hashes':'passed','python_syntax':'passed',
        'regression_command':'make test',
        'fresh_aap':fresh,
        'current_exhaustive_instances':small,
        'archive_raw_clock_audit':'28342 structural passes; 1146 original A/AP score mismatches remain',
        'processed_seconds_audit':'0/28342 archived routes pass with unchanged released query windows',
        'historical_experiment_provenance_resolved':False,
        'current_manuscript_scope':'Current verification and archived timings are separately scoped',
        'known_issues':'docs/KNOWN_ISSUES.md',
        'figures':{'included':bool(figures),'count':len(figures),'optional_generator':'scripts/plot_results.py','runtime_source':'unchanged formal runtime_sec only'}}
(root/'docs/INTEGRATION_QA.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
