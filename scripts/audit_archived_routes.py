#!/usr/bin/env python3
"""Audit archived routes under raw numeric clocks and declared seconds separately."""
import argparse
import collections
import csv
import json
import math
from pathlib import Path
from results_layout import merge


def rows(path):
    with path.open(newline='', encoding='utf-8-sig') as stream:
        return list(csv.DictReader(stream))


def write(path, records):
    with path.open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)


def hms(value):
    assert value % 100 < 60 and value // 100 % 100 < 60
    return value // 10000 * 3600 + value // 100 % 100 * 60 + value % 100


def evaluate(ids, q, edges, labels, converted=False):
    station, clock = int(q['source']), int(q['start_min'])
    end = int(q['end_min'])
    dwell = int(q['dwell_seconds'])
    visited, spent = set(), 0.0
    if not ids or len(ids) != len(set(ids)):
        return False, 'empty_or_repeated_edge', None, None
    for index, ident in enumerate(ids):
        u, v, dep, arr, cost = edges[ident]
        if converted:
            dep, arr = hms(dep), hms(arr)
        if u != station:
            return False, 'disconnected', None, None
        if dep < clock:
            return False, 'departure_before_current_time', None, None
        if arr > end:
            return False, 'arrival_after_window', None, None
        if dep - clock >= dwell:
            visited.add(station)
        spent += cost
        station, clock = v, arr
        if station == int(q['target']) and index + 1 < len(ids):
            return False, 'continues_after_target', None, None
    if station != int(q['target']):
        return False, 'wrong_target', None, None
    if spent > float(q['budget']) + 1e-9:
        return False, 'budget_exceeded', None, None
    if end - clock >= dwell:
        visited.add(station)
    covered = set().union(*(labels.get(v, set()) for v in visited))
    score = sum(w for k, w in zip(q['keywords'].split('|'), map(float, q['weights'].split('|'))) if int(k) in covered)
    return True, 'OK', score, clock


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', default=str(Path(__file__).resolve().parents[1]))
    args = parser.parse_args()
    root = Path(args.root).resolve()
    final = root / 'results/final'
    legacy = (final/'route_level.routes.csv').exists()
    out = final / 'audit'
    if legacy:out.mkdir(exist_ok=True)
    queries = {r['query_id']:r for r in rows(root/'queries/final_queries_1260.csv')}
    source = rows(final/('all_methods.routes.csv' if legacy else 'routes.csv'))
    needed = collections.defaultdict(set)
    for r in source:
        needed[r['city']].update(int(x) for x in r['edge_ids'].split(';'))
    networks, labels, network_info = {}, {}, {}
    for city in ('Austin', 'Houston'):
        networks[city], labels[city] = {}, {}
        count, invalid_hms, clock_mismatches = 0, 0, 0
        raw_path = root/f'data/raw/{city}/edges.txt'
        processed_path = root/f'data/processed/{city}/trips.csv'
        with raw_path.open() as raw, processed_path.open(newline='') as processed:
            csv_rows = csv.DictReader(processed)
            for index, line in enumerate(raw):
                u,v,dep,arr,cost = line.split()
                u,v,dep,arr,cost = int(u),int(v),int(dep),int(arr),float(cost)
                p = next(csv_rows)
                assert (int(p['from']), int(p['to'])) == (u,v)
                assert abs(float(p['cost'])-cost)<1e-9
                assert int(p['departure_min']) == hms(dep) and int(p['arrival_min']) == hms(arr)
                assert p['edge_id'] == f'{city.lower()}_e{index+1:06d}'
                clock_mismatches += dep != int(p['departure_min']) or arr != int(p['arrival_min'])
                count += 1
                if index in needed[city]:networks[city][index]=(u,v,dep,arr,cost)
            assert next(csv_rows,None) is None
        for line in (root/f'data/raw/{city}/labels.txt').open():
            values = list(map(int,line.split()))
            if values: labels[city][values[0]]=set(values[1:])
        network_info[city]={'stations':len(labels[city]),'scheduled_edges':count,
                            'mean_scheduled_out_degree':count/len(labels[city]),
                            'rows_with_raw_and_seconds_clock_difference':clock_mismatches,
                            'processed_conversion_and_id_mapping_verified':True}
    details, summary = [], {}
    for r in source:
        q=queries[r['query_id']]
        ids=[int(x) for x in r['edge_ids'].split(';')]
        raw_ok,raw_reason,raw_score,raw_arr = evaluate(ids,q,networks[r['city']],labels[r['city']])
        sec_ok,sec_reason,sec_score,sec_arr = evaluate(ids,q,networks[r['city']],labels[r['city']],True)
        detail={'query_id':r['query_id'],'algorithm':r['algorithm'],'route_rank':r['route_rank'],
                'raw_integer_clock_structural_pass':int(raw_ok),'raw_integer_clock_reason':raw_reason,
                'recorded_kc':r['kc'],'raw_integer_clock_recomputed_kc':raw_score if raw_ok else '',
                'raw_integer_clock_score_match':int(raw_ok and abs(raw_score-float(r['kc']))<1e-6),
                'processed_seconds_structural_pass':int(sec_ok),'processed_seconds_reason':sec_reason,
                'processed_seconds_recomputed_kc':sec_score if sec_ok else '',
                'processed_seconds_score_match':int(sec_ok and abs(sec_score-float(r['kc']))<1e-6),
                'processed_edge_ids':';'.join(f'{r["city"].lower()}_e{x+1:06d}' for x in ids)}
        details.append(detail)
    if legacy:
        write(out/'all_routes.time_and_score_audit.csv',details)
    else:
        existing=rows(final/'validation/routes.csv')
        write(final/'validation/routes.csv',merge(existing,details,require_all=True))
    for alg in ('A','AP','BL'):
        rr=[r for r in details if r['algorithm']==alg]
        summary[alg]={'routes':len(rr),
                     'raw_integer_clock_structural_pass':sum(r['raw_integer_clock_structural_pass'] for r in rr),
                     'raw_integer_clock_score_match':sum(r['raw_integer_clock_score_match'] for r in rr),
                     'processed_seconds_structural_pass':sum(r['processed_seconds_structural_pass'] for r in rr),
                     'processed_seconds_score_match':sum(r['processed_seconds_score_match'] for r in rr)}
    by_execution=collections.defaultdict(list)
    for r in details:by_execution[r['query_id'],r['algorithm']].append(r)
    quality=[]
    for r in rows(final/('coverage_quality.csv' if legacy else 'quality.csv')):
        a=by_execution[r['query_id'],'A']; ap=by_execution[r['query_id'],'AP']
        abest=max(float(x['raw_integer_clock_recomputed_kc']) for x in a)
        pbest=max((float(x['raw_integer_clock_recomputed_kc']) for x in ap),default=0)
        quality.append({'query_id':r['query_id'],'recorded_astar_best_kc':r['astar_best_kc'],
                        'recorded_ap_best_kc':r['ap_best_kc'],'recorded_ratio':r['ap_astar_ratio'],
                        'any_raw_clock_route_score_mismatch':int(any(not x['raw_integer_clock_score_match'] for x in a+ap)),
                        'raw_clock_astar_best_recomputed':abest,'raw_clock_ap_best_recomputed':pbest,
                        'raw_clock_astar_best_matches_recorded':int(abs(abest-float(r['astar_best_kc']))<1e-6),
                        'raw_clock_ap_best_matches_recorded':int(abs(pbest-float(r['ap_best_kc']))<1e-6),
                        'all_reference_routes_pass_processed_seconds':int(all(x['processed_seconds_structural_pass'] for x in a)),
                        'eligibility_under_declared_seconds_verified':0})
    if legacy:
        write(out/'coverage_quality.recorded_score_audit.csv',quality)
    else:
        existing=rows(final/'quality.csv')
        write(final/'quality.csv',merge(existing,quality,('query_id',),prefix='audit_',require_all=True))
    report={'network_inputs':network_info,'route_audit_by_algorithm':summary,
            'recorded_quality_queries':len(quality),
            'recorded_quality_queries_with_any_raw_clock_score_mismatch':sum(r['any_raw_clock_route_score_mismatch'] for r in quality),
            'recorded_quality_queries_with_changed_best_score':sum(not r['raw_clock_astar_best_matches_recorded'] or not r['raw_clock_ap_best_matches_recorded'] for r in quality),
            'declared_seconds_and_raw_integer_clock_are_not_equivalent':True,
            'recorded_quality_values_overwritten':False}
    if legacy:
        (out/'time_and_score_audit.summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    else:
        summary=json.loads((final/'summary.json').read_text())
        summary['audits']['time_and_score']=report
        (final/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
