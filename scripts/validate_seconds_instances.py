#!/usr/bin/env python3
"""Build reproducible second-valued city subgraphs and compare complete top-k lists."""
import collections
import csv
import json
import random
import subprocess
import sys
from pathlib import Path
import numpy as np
from verify_results import data, evaluate, verify


def write(path,fields,rr):
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.writer(f);w.writerow(fields);w.writerows(rr)


def enumerate_all(q,stations,trips,cap=10000):
    outgoing=collections.defaultdict(list)
    for e in trips.values():outgoing[e['from']].append(e)
    result=[];expanded=0
    def dfs(v,clock,cost,path,used):
        nonlocal expanded
        expanded+=1
        if expanded>200000:raise OverflowError('enumeration work cap')
        if v==q['target']:
            score,budget,arrival=evaluate(path,q,stations,trips)
            result.append((score,tuple(path),budget,arrival))
            if len(result)>cap:raise OverflowError('enumeration route cap')
            return
        for e in outgoing[v]:
            if e['edge_id'] in used or e['departure_min']<clock or e['arrival_min']>q['end_min'] or cost+e['cost']>q['budget']+1e-9:continue
            path.append(e['edge_id']);used.add(e['edge_id'])
            dfs(e['to'],e['arrival_min'],cost+e['cost'],path,used)
            used.remove(e['edge_id']);path.pop()
    dfs(q['source'],q['start_min'],0,[],set())
    result.sort(key=lambda x:(-x[0],x[1]))
    return result


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/'validation/seconds_instances';out.mkdir(parents=True,exist_ok=True)
    metadata=[];comparisons=[];rejections={}
    query_fields=['query_id','city','parameter','value','query_no','source','target','start_min','end_min','budget','keywords','weights','k','dwell_seconds','seed']
    for city,seed in [('Austin',20261928),('Houston',20262928)]:
        raw=np.loadtxt(root/f'data/raw/{city}/edges.txt')
        integer=raw[:,:4].astype(np.int64)
        for col in (2,3):
            values=integer[:,col].copy()
            integer[:,col]=values//10000*3600+(values//100%100)*60+values%100
        order=np.argsort(integer[:,0],kind='stable')
        counts=np.bincount(integer[:,0]);offset=np.concatenate(([0],np.cumsum(counts)))
        profiles=collections.defaultdict(dict)
        for line in (root/f'data/raw/{city}/labels.txt').open():
            values=line.split()
            for key in values[1:]:profiles[int(values[0])][key]=1.0
        rng=random.Random(seed);accepted=0;attempt=0;rejected=collections.Counter()
        while accepted<10:
            attempt+=1
            if attempt>20000:raise RuntimeError('unable to obtain tractable instances')
            first=rng.randrange(len(raw));begin=max(0,int(integer[first,2])-rng.randrange(301));end=begin+5400
            path=[first];vertices={int(integer[first,0]),int(integer[first,1])}
            now=int(integer[first,1]);clock=int(integer[first,3]);spent=float(raw[first,4])
            for _ in range(rng.randint(1,4)):
                if now>=len(counts):break
                ids=order[offset[now]:offset[now+1]]
                options=[int(i) for i in ids if int(integer[i,2])>=clock and int(integer[i,3])<=end and int(integer[i,1]) not in vertices and spent+float(raw[i,4])<=70]
                if not options:break
                edge=rng.choice(options);path.append(edge)
                now=int(integer[edge,1]);vertices.add(now);clock=int(integer[edge,3]);spent+=float(raw[edge,4])
            if len(vertices)<3 or spent>70:
                rejected['seed_path']+=1;continue
            mask=np.isin(integer[:,0],list(vertices))&np.isin(integer[:,1],list(vertices))&(integer[:,2]>=begin)&(integer[:,3]<=end)
            kept=np.flatnonzero(mask)
            if len(kept)>100:
                rejected['edge_count']+=1;continue
            idx=accepted+1;instance=f'{city.lower()}_seconds_{idx:02d}';folder=out/instance;folder.mkdir(exist_ok=True)
            keys=sorted({k for v in vertices for k in profiles[v]},key=int)
            if not keys:rejected['no_keywords']+=1;continue
            keys=rng.sample(keys,min(5,len(keys)));weights=[1/len(keys)]*len(keys)
            query=dict(zip(query_fields,[instance,city,'small_seconds',str(idx),str(idx),str(integer[first,0]),str(now),str(begin),str(end),'70','|'.join(keys),'|'.join(format(w,'.17g') for w in weights),'10',str(rng.randint(600,1800)),str(seed)]))
            write(folder/'stations.csv',['station_id','dwell_min','keyword','coverage'],[[v,1,k,value] for v in sorted(vertices) for k,value in profiles[v].items()])
            write(folder/'trips.csv',['edge_id','from','to','departure_min','arrival_min','cost'],[[f'{city.lower()}_e{i+1:06d}',int(integer[i,0]),int(integer[i,1]),int(integer[i,2]),int(integer[i,3]),float(raw[i,4])] for i in kept])
            write(folder/'query.csv',query_fields,[[query[k] for k in query_fields]])
            loaded=data(folder/'stations.csv',folder/'trips.csv',folder/'query.csv')
            q=loaded[2][instance]
            try:truth=enumerate_all(q,*loaded[:2])
            except OverflowError:
                rejected['enumeration_cap']+=1;continue
            if not truth:raise RuntimeError('generated seed path lost')
            expected=truth[:10];outputs={};rankrows=[]
            for alg in ('A','BL','AP'):
                binary=root/('build/kcrn_bl' if alg=='BL' else 'build/route_search')
                command=[str(binary),'--stations',str(folder/'stations.csv'),'--trips',str(folder/'trips.csv'),
                         '--queries',str(folder/'query.csv'),'--query-id',instance,'--algorithm',alg]
                if alg!='BL':command+=['--runs','10','--rho','8','--rounds','20']
                run=subprocess.run(command,check=True,capture_output=True,text=True,timeout=30)
                record=json.loads(run.stdout);verify(record,*loaded)
                outputs[alg]=record
                if alg in ('A','BL'):
                    got=record['solutions']
                    assert len(got)==len(expected),(instance,alg,'count')
                    assert [tuple(s['edge_ids']) for s in got]==[x[1] for x in expected],(instance,alg,'ordered sequences')
                    assert all(abs(s['score']-x[0])<=1e-8 for s,x in zip(got,expected))
            for rank,x in enumerate(expected):
                rankrows.append([instance,rank,x[0],';'.join(x[1]),outputs['A']['solutions'][rank]['score'],outputs['BL']['solutions'][rank]['score'],1,1])
            write(folder/'topk_comparison.csv',['instance_id','rank','exhaustive_kc','edge_ids','A_kc','BL_kc','A_matches','BL_matches'],rankrows)
            (folder/'executions.json').write_text(json.dumps(outputs,indent=2))
            metadata.append({'instance_id':instance,'city':city,'seed':seed,'accepted_attempt':attempt,'vertices':len(vertices),'edges':len(kept),'window_seconds':5400,'dwell_seconds':q['dwell_seconds'],'feasible_routes':len(truth),'requested_k':10,'compared_ranks':len(expected),'A_ordered_topk_match':1,'BL_ordered_topk_match':1,'AP_routes_feasible':1,'time_basis':'processed_seconds'})
            comparisons.extend(rankrows);accepted+=1
            print(f'{instance}: vertices={len(vertices)}, edges={len(kept)}, feasible={len(truth)}, ranks={len(expected)}',flush=True)
        rejections[city]=dict(rejected)
        print(city,'rejected by tractability/seed criteria',dict(rejected),flush=True)
        del raw,integer,order
    with (out/'metadata.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(metadata[0]));w.writeheader();w.writerows(metadata)
    write(out/'topk_comparison.csv',['instance_id','rank','exhaustive_kc','edge_ids','A_kc','BL_kc','A_matches','BL_matches'],comparisons)
    summary={'instances':len(metadata),'A_matches':20,'BL_matches':20,'AP_feasibility_pass':20,'compared_ranks':len(comparisons),'time_basis':'processed_seconds','tractability_rejections':rejections,
             'vertices_range':[min(x['vertices'] for x in metadata),max(x['vertices'] for x in metadata)],
             'edges_range':[min(x['edges'] for x in metadata),max(x['edges'] for x in metadata)],
             'feasible_routes_range':[min(x['feasible_routes'] for x in metadata),max(x['feasible_routes'] for x in metadata)],
             'dwell_range':[min(x['dwell_seconds'] for x in metadata),max(x['dwell_seconds'] for x in metadata)]}
    (out/'summary.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary,indent=2),flush=True)


if __name__=='__main__':main()
