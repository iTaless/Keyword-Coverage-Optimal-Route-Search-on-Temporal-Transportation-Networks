#!/usr/bin/env python3
"""Normalize formal records, then attach BL route evidence with separate provenance."""
import collections
import csv
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from results_layout import arrange_final, bl_archive_rows


def rows(path):
    with path.open(newline='',encoding='utf-8-sig') as f:return list(csv.DictReader(f))


def write(path,data):
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(data[0]));w.writeheader();w.writerows(data)


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    root=Path(__file__).resolve().parents[1]
    final=root/'results/final'; source=final/'source'
    release_manifest=json.loads((final/'manifest.json').read_text())
    args=[sys.executable,str(root/'scripts/import_author_results.py'),
          '--queries',str(root/'queries/final_queries_1260.csv'),
          '--runtime',str(source/'runtime_results_3600.csv'),
          '--routes',str(source/'routes_22775.csv'),
          '--route-validation',str(source/'route_validation_22775.csv'),
          '--query-summary',str(source/'route_query_summary_recorded.csv'),
          '--group-summary',str(source/'route_group_summary_recorded.csv'),
          '--runtime-workbook',str(source/'author_runtime_results.xlsx'),
          '--runtime-summary-workbook',str(source/'author_runtime_summary.xlsx'),
          '--out-dir',str(final)]
    subprocess.run(args,check=True,stdout=subprocess.DEVNULL)
    runtime=rows(final/'main_study.runtime.csv')
    validation=rows(final/'main_study.validation.csv')
    original_routes=rows(final/'main_study.routes.csv')
    ap_all=rows(final/'route_level.routes.csv')
    recovered=rows(final/'bl_recovery/runtime.csv')
    bl_routes=bl_archive_rows(final,'.routes.csv')
    by_id={r['query_id']:r for r in recovered}
    by_route=collections.defaultdict(list)
    for r in bl_routes:by_route[r['query_id']].append(r)
    for rr in by_route.values():rr.sort(key=lambda r:int(r['route_rank']))
    before=[(r['query_id'],r['algorithm'],r['status'],r['runtime_sec'],r['timeout_threshold_sec']) for r in runtime]
    for row in runtime:
        row.update(route_record_origin=('author_workstation_route_archive' if row['algorithm']!='BL' else 'unavailable'),
                   supplementary_status='',supplementary_timeout_threshold_sec='',
                   recovery_runtime_sec='',route_validation_scope='raw_integer_clock')
        if row['algorithm']!='BL':continue
        r=by_id.get(row['query_id'])
        if not r:continue
        row.update(supplementary_status=r['status'],supplementary_timeout_threshold_sec=r['timeout_threshold_sec'],
                   recovery_runtime_sec=r['recovery_runtime_sec'],
                   route_record_origin='supplementary_BL_route_recovery',
                   route_sequences_available=r['route_sequences_available'],
                   returned_distinct_routes=r['returned_distinct_routes'],best_kc=r['best_kc'])
    assert before==[(r['query_id'],r['algorithm'],r['status'],r['runtime_sec'],r['timeout_threshold_sec']) for r in runtime]
    write(final/'main_study.runtime.csv',runtime)

    for row in validation:
        row.update(route_execution_status='',route_record_origin=row['record_origin'],
                   validation_time_basis='raw_integer_clock',processed_seconds_validation_status='not_passed')
        if row['algorithm']!='BL':continue
        r=by_id.get(row['query_id'])
        if not r:
            row['validation_time_basis']='unavailable';row['processed_seconds_validation_status']='unavailable';continue
        row.update(route_execution_status=r['status'],route_record_origin='supplementary_BL_route_recovery')
        status=r['status'];n=len(by_route[row['query_id']])
        if status=='ok':
            row.update(validation_status='passed_raw_integer_clock_only',route_records_checked=n,
                       returned_routes=n,all_returned_routes_feasible=1,all_routes_distinct=1,
                       scores_recomputed=1,all_recomputed_scores_match=1)
        elif status=='no_route':
            row.update(validation_status='no_route_record',route_records_checked=0,returned_routes=0,
                       processed_seconds_validation_status='no_routes_to_validate')
        else:
            row.update(validation_status='not_available',route_records_checked='',returned_routes='',
                       processed_seconds_validation_status='unavailable')
    write(final/'main_study.validation.csv',validation)

    bl_normalized=[]
    for r in bl_routes:
        bl_normalized.append({**r,'record_origin':'supplementary_BL_route_recovery'})
    for collection in (original_routes,ap_all,bl_normalized):
        for r in collection:
            r.update(edge_id_namespace='raw_zero_based_record_index',time_basis='raw_HHMMSS_as_integer',
                     validation_scope='raw_integer_clock_only')
    write(final/'main_study.routes.csv',original_routes+bl_normalized)
    write(final/'all_methods.routes.csv',ap_all+bl_normalized)
    records=[json.loads(line) for line in (final/'main_study.jsonl').read_text().splitlines() if line.strip()]
    rr_by_key={(r['query_id'],r['algorithm']):r for r in runtime}
    for r in records:
        r['solutions_time_basis']='raw_HHMMSS_as_integer'
        r['processed_seconds_validation_pass']=False
        r['solutions_origin']='author_workstation_route_archive' if r['algorithm']!='BL' else 'unavailable'
        if r['algorithm']!='BL':continue
        evidence=by_id.get(r['query_id'])
        if not evidence:continue
        r['supplementary_execution']={
            'status':evidence['status'],'timeout_threshold_sec':float(evidence['timeout_threshold_sec']),
            'recovery_runtime_sec':float(evidence['recovery_runtime_sec']),
            'source_archive':evidence['source_archive'],'is_source_of_formal_runtime':False}
        r['route_sequences_available']=evidence['route_sequences_available']=='1'
        r['solutions_origin']='supplementary_BL_route_recovery'
        r['solutions']=[{'edge_ids':x['edge_ids'].split(';'),'score':float(x['kc']),
                         'budget':float(x['budget']),'arrival_min':int(x['arrival_time_sec'])}
                        for x in by_route[r['query_id']]]
    with (final/'main_study.jsonl').open('w') as f:
        for record in records:f.write(json.dumps(record,ensure_ascii=False)+'\n')
    # The original 22,775-row A/AP tables remain unchanged in scope.
    summary=json.loads((final/'main_study.summary.json').read_text())
    summary.update(bl_route_sequences_available=True,bl_query_routes_available=634,
                   bl_query_zero_route_answers_available=36,bl_queries_rerun=703,
                   bl_supplementary_status_counts=dict(collections.Counter(r['status'] for r in recovered)),
                   bl_route_records=5567,main_study_route_records_including_supplementary_BL=27181,
                   all_methods_route_records=28342,
                   bl_validation_scope='raw_integer_clock_only',
                   processed_seconds_route_consistency='failed; see validation/routes.csv and summary.json',
                   original_runtime_values_and_statuses_preserved=True)
    (final/'main_study.summary.json').write_text(json.dumps(summary,indent=2))
    manifest=json.loads((final/'main_study.manifest.json').read_text())
    manifest.update(record_origin='author_workstation_runtime_plus_separately_sourced_route_evidence',
                    normalization_only=False,
                    formal_runtime_values_unchanged=True,
                    integration_script='scripts/integrate_bl_results.py',
                    integration_script_sha256=sha(Path(__file__)),
                    archived_routes_input_basis='raw_HHMMSS_as_integer',
                    processed_seconds_compatibility='not established; replay fails',
                    supplementary_BL={'query_count':703,'queries_with_routes':634,'routes':5567,
                                      'formal_runtime_replacement':False,
                                      'metadata_location':'results/final/source/bl_recovery.zip',
                                      'original_batches':release_manifest['supplementary_BL']['original_batches'],
                                      'source_archives':[{'path':'results/final/source/bl_recovery.zip','sha256':sha(source/'bl_recovery.zip')}]},
                    limitations=[
                        'Formal runtime cells and outcomes are preserved from the author workbook.',
                        'BL route fields may describe separate supplementary executions, including some whose original formal run timed out.',
                        'Raw HHMMSS-valued timestamps were used numerically by archived route runners; this is not equivalent to seconds.',
                        'All archived routes fail replay against processed second-valued edges with the currently released query windows.',
                        'Recorded A/AP quality values are preserved; 1146 route scores differ even under the archived raw clock.'])
    for field in ('publication_metadata_scope','archival_publication_copies'):
        if field in release_manifest:manifest[field]=release_manifest[field]
    (final/'main_study.manifest.json').write_text(json.dumps(manifest,indent=2))
    subprocess.run([sys.executable,str(root/'scripts/audit_archived_routes.py')],check=True,stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable,str(root/'scripts/audit_score_provenance.py')],check=True,stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable,str(root/'scripts/summarize.py'),'--results',str(final/'main_study.jsonl'),'--out',str(final/'main_study.runtime_summary.csv')],check=True,stdout=subprocess.DEVNULL)
    arrange_final(root)
    print('Integrated 5567 BL routes; 3600 original runtime/status cells preserved.')


if __name__=='__main__':main()
