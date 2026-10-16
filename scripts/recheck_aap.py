#!/usr/bin/env python3
"""Rerun fixed A/AP queries for scoring verification, with checkpoints and independent replay.

These are new measurements on the current environment, never replacements for
the archived author-workstation runtime comparison.
"""
import argparse
import collections
import concurrent.futures
import csv
import hashlib
import json
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path
from verify_results import data, verify


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1<<20),b''):h.update(block)
    return h.hexdigest()


def write_csv(path, records, fields=None):
    with path.open('w',newline='',encoding='utf-8') as stream:
        w=csv.DictWriter(stream,fieldnames=fields or list(records[0]));w.writeheader();w.writerows(records)


def main():
    root=Path(__file__).resolve().parents[1]
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workers',type=int,default=4)
    p.add_argument('--timeout',type=float,default=300)
    p.add_argument('--memory-mib',type=int,default=1536,
                   help='per-subprocess virtual-address-space limit on Linux; 0 disables')
    p.add_argument('--limit',type=int,default=0)
    p.add_argument('--out-dir',default='results/rechecked_aap')
    a=p.parse_args()
    if a.workers<1 or a.timeout<=0:p.error('workers and timeout must be positive')
    output=root/a.out_dir;output.mkdir(parents=True,exist_ok=True)
    query_file=root/'queries/final_queries_1260.csv'
    with query_file.open(newline='') as f:queries=list(csv.DictReader(f))
    if a.limit:queries=queries[:a.limit]
    binary=root/'build/route_search'
    identity={'time_basis':'processed_seconds','query_sha256':sha(query_file),
              'binary_sha256':sha(binary),'source_sha256':sha(root/'src/main.cpp'),
              'timeout_seconds':a.timeout,'query_count':len(queries),'memory_limit_mib':a.memory_mib,
              'datasets':{city:{f:sha(root/f'data/processed/{city}/{f}') for f in ('stations.csv','trips.csv')}
                          for city in sorted({q['city'] for q in queries})}}
    identity_file=output/'input_identity.json'
    if identity_file.exists():
        if json.loads(identity_file.read_text())!=identity:raise RuntimeError('Inputs changed; use a new output directory.')
    else:identity_file.write_text(json.dumps(identity,indent=2))
    checkpoint=output/'aap.jsonl'
    records=[]
    if checkpoint.exists():
        records=[json.loads(line) for line in checkpoint.read_text().splitlines() if line.strip()]
    done={(r['query_id'],r['algorithm']) for r in records}
    if len(done)!=len(records):raise RuntimeError('duplicate checkpoint record')
    tasks=[(q,alg) for q in queries for alg in ('A','AP') if (q['query_id'],alg) not in done]
    started=time.time()
    def run(task):
        q,alg=task;city=q['city']
        command=[str(binary),'--stations',str(root/f'data/processed/{city}/stations.csv'),
                 '--trips',str(root/f'data/processed/{city}/trips.csv'),
                 '--queries',str(query_file),'--query-id',q['query_id'],'--algorithm',alg,
                 '--rho','8','--rounds','20','--runs',q['k']]
        if a.memory_mib:
            if not shutil.which('prlimit'):raise RuntimeError('prlimit required for the requested memory cap')
            command=['prlimit',f'--as={a.memory_mib*1024*1024}','--']+command
        begin=time.perf_counter()
        try:
            proc=subprocess.run(command,capture_output=True,text=True,timeout=a.timeout)
            if proc.returncode:
                limited=proc.returncode in (-9,137) or 'bad_alloc' in proc.stderr
                result={'query_id':q['query_id'],'algorithm':alg,'status':'resource_limit' if limited else 'error',
                        'exit_code':proc.returncode,'error':proc.stderr[-4000:],'solutions':[]}
            else:result=json.loads(proc.stdout)
        except subprocess.TimeoutExpired:
            result={'query_id':q['query_id'],'algorithm':alg,'status':'timeout','solutions':[]}
        result.update(city=city,parameter=q['parameter'],value=q['value'],query_no=int(q['query_no']),
                      requested_k=int(q['k']),dwell_seconds=int(q['dwell_seconds']),query_seed=int(q['seed']),
                      wall_ms=(time.perf_counter()-begin)*1000,timeout_threshold_sec=a.timeout,
                      time_basis='processed_seconds',record_origin='current_code_scoring_recheck')
        if result['status'] in ('ok','no_route'):
            result['query_runtime_ms']=result['preprocess_ms']+result['search_ms']
        return result
    with checkpoint.open('a',encoding='utf-8') as stream:
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as executor:
            pending={executor.submit(run,t):t for t in tasks}
            for future in concurrent.futures.as_completed(pending):
                record=future.result();records.append(record)
                stream.write(json.dumps(record,ensure_ascii=False)+'\n');stream.flush()
                if len(records)%25==0 or len(records)==2*len(queries):
                    print(json.dumps({'completed':len(records),'total':2*len(queries),
                                      'status':dict(collections.Counter(r['status'] for r in records)),
                                      'elapsed_seconds':round(time.time()-started,1)}),flush=True)
    records.sort(key=lambda r:(r['city'],r['query_id'],r['algorithm']))
    runtime=[];routes=[]
    for r in records:
        good=r['status'] in ('ok','no_route');solutions=r['solutions']
        runtime.append({k:r[k] for k in ('query_id','city','parameter','value','query_no','algorithm','status')})
        runtime[-1].update(runtime_sec=r['query_runtime_ms']/1000 if good else '',
                           wall_sec=r['wall_ms']/1000,timeout_threshold_sec=a.timeout,
                           requested_k=r['requested_k'],returned_distinct_routes=len(solutions) if good else '',
                           best_kc=max((s['score'] for s in solutions),default=''),
                           exact_optimal=int(r['algorithm']=='A' and good),time_basis='processed_seconds')
        for rank,s in enumerate(solutions):
            routes.append({'query_id':r['query_id'],'city':r['city'],'algorithm':r['algorithm'],'route_rank':rank,
                           'kc':s['score'],'budget':s['budget'],'arrival_time_sec':s['arrival_min'],
                           'edge_ids':';'.join(s['edge_ids']),'time_basis':'processed_seconds'})
    write_csv(output/'aap.runtime.csv',runtime)
    write_csv(output/'aap.routes.csv',routes,['query_id','city','algorithm','route_rank','kc','budget','arrival_time_sec','edge_ids','time_basis'])
    audits=[]
    for city in sorted({q['city'] for q in queries}):
        rr=[r for r in records if r['city']==city]
        needed={ident for r in rr for s in r['solutions'] for ident in s['edge_ids']}
        loaded=data(root/f'data/processed/{city}/stations.csv',root/f'data/processed/{city}/trips.csv',query_file,needed)
        for r in rr:
            try:
                audit=verify(r,*loaded)
                audit['audit_error']=''
            except (AssertionError,ValueError,KeyError) as error:
                audit={'query_id':r['query_id'],'algorithm':r['algorithm'],'execution_status':r['status'],
                       'validation_status':'failed','route_records_checked':len(r['solutions']),
                       'returned_routes':len(r['solutions']),'requested_k':r['requested_k'],
                       'all_returned_routes_feasible':0,'all_routes_distinct':'','scores_recomputed':0,
                       'exhaustive_oracle_checked':0,'audit_error':str(error)}
            audits.append(audit)
        del loaded
    write_csv(output/'aap.validation.csv',audits)
    by_id=collections.defaultdict(dict)
    for r in records:by_id[r['query_id']][r['algorithm']]=r
    audit_by_key={(r['query_id'],r['algorithm']):r for r in audits}
    quality=[]
    for q in queries:
        aa=by_id[q['query_id']]['A'];ap=by_id[q['query_id']]['AP']
        abest=max((s['score'] for s in aa['solutions']),default=0)
        pbest=max((s['score'] for s in ap['solutions']),default=0)
        valid=all(audit_by_key[q['query_id'],alg]['validation_status']=='passed' for alg in ('A','AP'))
        eligible=aa['status']=='ok' and ap['status'] in ('ok','no_route') and abest>0 and valid
        reason='' if eligible else ('A_not_completed' if aa['status'] in ('timeout','resource_limit','error') else 'A_no_route' if aa['status']=='no_route' else
                                   'A_zero_score' if abest==0 else 'validation_or_AP_execution_not_passed')
        quality.append({'query_id':q['query_id'],'city':q['city'],'astar_status':aa['status'],
                        'astar_optimal':int(aa['status'] in ('ok','no_route')),
                        'astar_best_kc':abest if aa['solutions'] else '',
                        'ap_status':ap['status'],'ap_best_kc':pbest if ap['solutions'] else '',
                        'quality_eligible':int(eligible),'ap_astar_ratio':pbest/abest if eligible else '',
                        'exclusion_reason':reason,'time_basis':'processed_seconds'})
    write_csv(output/'coverage_quality.all_queries.csv',quality)
    ratios=[float(r['ap_astar_ratio']) for r in quality if r['quality_eligible']]
    summary={'query_count':len(queries),'executions':len(records),'status_counts':{alg:dict(collections.Counter(r['status'] for r in records if r['algorithm']==alg)) for alg in ('A','AP')},
             'returned_routes':len(routes),'validation_failures':sum(r['validation_status']=='failed' for r in audits),
             'quality_eligible_queries':len(ratios),'mean_ratio':sum(ratios)/len(ratios) if ratios else None,
             'ratios_above_one':sum(x>1+1e-10 for x in ratios),
             'time_basis':'processed_seconds','author_workstation_runtime_replaced':False,
             'scope':'new scoring recheck on unchanged published query values; not restoration of historical query intent'}
    (output/'summary.json').write_text(json.dumps(summary,indent=2))
    meta={**identity,'workers':a.workers,
          'aggregation_script_sha256':sha(Path(__file__)),
          'python':platform.python_version(),'generated_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
          'runtime_comparison_use':False,'command':sys.argv}
    (output/'run_metadata.json').write_text(json.dumps(meta,indent=2))
    print(json.dumps(summary,indent=2),flush=True)
    if summary['validation_failures'] or summary['ratios_above_one']:raise SystemExit(1)


if __name__=='__main__':main()
