"""Check canonical release tables, source identities and archived measurements."""
import ast
import collections
import json
import zipfile
from pathlib import Path
from results_layout import rows, sha, bl_archive_rows

root=Path(__file__).resolve().parents[1]
final=root/'results/final'
source={(r['query_id'],'A' if r['algorithm']=='A*' else r['algorithm']):r for r in rows(final/'source/runtime_results_3600.csv')}
runtime=rows(final/'runtime.csv')
assert len(source)==len(runtime)==3600
assert len({(r['query_id'],r['algorithm']) for r in runtime})==3600
for r in runtime:
    for field in ('status','runtime_sec','timeout_threshold_sec'):
        assert r[field]==source[r['query_id'],r['algorithm']][field]
assert collections.Counter((r['algorithm'],r['status']) for r in runtime)=={('BL','completed'):653,('BL','timeout'):547,('A','completed'):1163,('A','timeout'):37,('AP','completed'):1200}
key=lambda r:(r['query_id'],r['algorithm'],r['route_rank'])
routes=rows(final/'routes.csv')
assert len(routes)==len({key(r) for r in routes})==28342
assert collections.Counter(r['algorithm'] for r in routes)=={'A':11687,'AP':11088,'BL':5567}
main_keys={(r['query_id'],r['algorithm']) for r in runtime}
main=[r for r in routes if (r['query_id'],r['algorithm']) in main_keys]
assert len(main)==27181
counts=collections.Counter((r['query_id'],r['algorithm']) for r in main)
for r in runtime:
    if r['route_sequences_available']=='1':assert int(r['returned_distinct_routes'])==counts[r['query_id'],r['algorithm']]
assert len(rows(final/'validation/queries.csv'))==3600
checks=rows(final/'validation/routes.csv')
assert len(checks)==28342 and {key(r) for r in checks}=={key(r) for r in routes}
assert sum(int(r['raw_integer_clock_structural_pass']) for r in checks)==28342
assert sum(int(r['processed_seconds_structural_pass']) for r in checks)==0
assert sum(r['node_dwell_query_match']=='0' for r in checks if r['algorithm'] in ('A','AP'))==1146
bl=rows(final/'bl_recovery/runtime.csv')
assert len(bl)==len({r['query_id'] for r in bl})==703
assert collections.Counter(r['status'] for r in bl)=={'ok':634,'no_route':36,'timeout':33}
assert sorted(collections.Counter(r['cohort'] for r in bl).values())==[50,653]
raw_bl={key(r):r for r in bl_archive_rows(final,'.routes.csv')}
assert len(raw_bl)==5567
for r in (r for r in routes if r['algorithm']=='BL'):
    assert r['edge_ids']==raw_bl[key(r)]['edge_ids']
    for field in ('kc','budget','arrival_time_sec'):assert abs(float(r[field])-float(raw_bl[key(r)][field]))<1e-10
summary_rows=rows(final/'runtime_summary.csv')
assert len(summary_rows)==120 and sum(int(r['n']) for r in summary_rows)==3600
assert sum(int(r['completed']) for r in summary_rows)==3016
assert sum(int(r['timeout']) for r in summary_rows)==584
quality=rows(final/'quality.csv')
assert len(quality)==240
assert abs(sum(float(r['ap_astar_ratio']) for r in quality)/240-.936833585471725)<1e-12
assert sum(int(r['audit_any_raw_clock_route_score_mismatch']) for r in quality)==81
summary=json.loads((final/'summary.json').read_text())
assert summary['audits']['time_and_score']['recorded_quality_queries_with_changed_best_score']==56
manifest=json.loads((final/'manifest.json').read_text())
for item in manifest['source_files']+manifest['published_implementation_files']:assert sha(root/item['path'])==item['sha256'],item['path']
for name in ('normalizer','integration_script','layout_script'):assert sha(root/manifest[name])==manifest[name+'_sha256']
for path in manifest['artifacts'].values():assert (root/path).is_file(),path
for item in manifest['supplementary_BL']['source_archives']:
    assert sha(root/item['path'])==item['sha256']
    with zipfile.ZipFile(root/item['path']) as z:assert z.testzip() is None
current=root/'results/rechecked_aap'
fresh=json.loads((current/'summary.json').read_text())
assert fresh['executions']==2520 and fresh['query_count']==1260
assert fresh['returned_routes']==2080 and fresh['quality_eligible_queries']==110
assert fresh['validation_failures']==fresh['ratios_above_one']==0
assert fresh['quality']['eligible']==fresh['quality']['equal']==110
assert all(fresh['quality'][k]==1 for k in ('mean','median','q1','q3'))
rr=rows(current/'aap.runtime.csv');cc=rows(current/'aap.validation.csv')
assert len(rr)==len(cc)==2520 and len(rows(current/'aap.routes.csv'))==2080
assert collections.Counter(r['status'] for r in rr)=={'ok':362,'no_route':2104,'resource_limit':54}
assert sum(int(r['route_records_checked']) for r in cc)==2080
assert collections.Counter(r['validation_status'] for r in cc)=={'passed':2466,'not_checked':54}
identity=json.loads((current/'run_metadata.json').read_text())
assert identity['source_sha256']==sha(root/'src/main.cpp')
assert identity['query_sha256']==sha(root/'queries/final_queries_1260.csv')
for city,inputs in identity['datasets'].items():
    for name,value in inputs.items():assert value==sha(root/f'data/processed/{city}/{name}')
small=json.loads((root/'validation/seconds_instances/summary.json').read_text())
assert small['instances']==small['A_matches']==small['BL_matches']==small['AP_feasibility_pass']==20
assert small['compared_ranks']==len(rows(root/'validation/seconds_instances/topk_comparison.csv'))==200
for p in root.rglob('*.py'):ast.parse(p.read_text(encoding='utf-8-sig'))
report={'canonical_layout':'passed','historical_runtime_records':3600,'main_routes':27181,'all_archived_routes':28342,'BL_recovery_attempts':703,'BL_recovery_routes':5567,'historical_quality_pairs':240,'source_identities':'passed','python_syntax':'passed','current_aap_attempts':2520,'current_routes':2080,'current_quality_pairs':110,'current_validation_failures':0,'exhaustive_instances':20,'compared_ranks':200,'historical_experiment_provenance_resolved':False,'paper_figures_included':False}
(root/'docs/INTEGRATION_QA.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
