#!/usr/bin/env python3
"""Independently validate BL routes against common processed second-valued tables."""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
from verify_results import data, evaluate


def rows(path):
    with open(path,newline='',encoding='utf-8-sig') as f:return list(csv.DictReader(f))


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('queries','routes','out'):p.add_argument('--'+name,required=True)
    p.add_argument('--project-root',default='.')
    a=p.parse_args()
    root=Path(a.project_root).resolve()
    queries={q['query_id']:q for q in rows(a.queries)}
    routes=rows(a.routes)
    by_city=defaultdict(list)
    for r in routes:by_city[queries[r['query_id']]['city']].append(r)
    result=[]
    for city,rr in sorted(by_city.items()):
        base=root/'data/processed'/city
        needed={edge for r in rr for edge in r['edge_ids'].split(';') if edge}
        stations,trips,q=data(base/'stations.csv',base/'trips.csv',a.queries,needed)
        seen=defaultdict(set)
        for r in rr:
            ids=tuple(x for x in r['edge_ids'].split(';') if x)
            duplicate=ids in seen[r['query_id']]
            seen[r['query_id']].add(ids)
            score=cost=''
            try:
                score,cost,arrival=evaluate(ids,q[r['query_id']],stations,trips)
                if duplicate:raise ValueError('duplicate route')
                if not math.isclose(float(r['kc']),score,abs_tol=1e-6):raise ValueError('recorded score mismatch')
                if not math.isclose(float(r['budget']),cost,abs_tol=1e-6):raise ValueError('recorded budget mismatch')
                if int(r['arrival_time_sec'])!=arrival:raise ValueError('arrival time mismatch')
                ok,reason=True,'OK'
            except (KeyError,ValueError) as error:
                ok,reason=False,str(error)
            result.append({'query_id':r['query_id'],'algorithm':'BL','route_rank':r['route_rank'],
                           'feasible':int(ok),'reason':reason,'recomputed_budget':cost,
                           'recomputed_kc':score,'duplicate_route':int(duplicate),
                           'validation_time_basis':'processed_seconds'})
    fields=['query_id','algorithm','route_rank','feasible','reason','recomputed_budget',
            'recomputed_kc','duplicate_route','validation_time_basis']
    with open(a.out,'w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader();writer.writerows(result)
    failures=sum(not r['feasible'] for r in result)
    print(f'validated {len(result)} processed-second routes; failures={failures}')
    if failures:raise SystemExit(1)


if __name__=='__main__':main()
