#!/usr/bin/env python3
"""Diagnose archived scores under both old dwell rules. Raw integer clocks ONLY.
Not a physical-seconds validation or a new exact-search quality experiment.
"""
import csv,collections,json,zipfile,io
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
out=repo/'results/final/audit'
out.mkdir(parents=True,exist_ok=True)
def rows(p):return list(csv.DictReader(open(p,newline='',encoding='utf-8-sig')))
queries={q['query_id']:q for q in rows(repo/'queries/final_queries_1260.csv')}
routes=rows(repo/'results/final/route_level.routes.csv')
needed=collections.defaultdict(set)
for r in routes:needed[r['city']].update(map(int,r['edge_ids'].split(';')))
lookup={};labels={};dwell={}
for city in ['Austin','Houston']:
 lookup[city]={}
 for i,line in enumerate(open(repo/f'data/raw/{city}/edges.txt')):
  if i in needed[city]:
   u,v,d,a,c=line.split();lookup[city][i]=(int(u),int(v),int(d),int(a),float(c))
 labels[city]={}
 for line in open(repo/f'data/raw/{city}/labels.txt'):
  vv=list(map(int,line.split()));labels[city][vv[0]]=set(vv[1:])
 dwell[city]={int(r['node_id']):int(r['dwell_seconds']) for r in rows(repo/f'results/final/source/original_aap_package/{city.lower()}_dwell.csv')}
result=[]
for r in routes:
 q=queries[r['query_id']];city=r['city'];ee=[lookup[city][i] for i in map(int,r['edge_ids'].split(';'))]
 visits=[(ee[0][0],ee[0][2]-int(q['start_min']))]+[(e[1],(ee[i+1][2] if i+1<len(ee) else int(q['end_min']))-e[3]) for i,e in enumerate(ee)]
 scores={}
 for basis in ['node','query']:
  covered=set()
  for v,wait in visits:
   threshold=dwell[city][v] if basis=='node' else int(q['dwell_seconds'])
   if wait>=threshold:covered.update(labels[city][v])
  scores[basis]=sum(w for k,w in zip(map(int,q['keywords'].split('|')),map(float,q['weights'].split('|'))) if k in covered)
 result.append({'query_id':r['query_id'],'algorithm':r['algorithm'],'route_rank':r['route_rank'],'recorded_kc':r['kc'],'recomputed_node_dwell_raw_clock':scores['node'],'recomputed_query_dwell_raw_clock':scores['query'],'node_match':int(abs(float(r['kc'])-scores['node'])<1e-6),'query_match':int(abs(float(r['kc'])-scores['query'])<1e-6)})
with open(out/'score_rule_comparison.csv','w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(result[0]));w.writeheader();w.writerows(result)
summary={'time_basis':'raw_integer_clock_diagnostic_only','match_absolute_tolerance':1e-6,'routes':len(result),'node_dwell_matches':sum(r['node_match'] for r in result),'query_dwell_matches':sum(r['query_match'] for r in result),'node_only_matches':sum(r['node_match'] and not r['query_match'] for r in result),'node_dwell_mismatches':sum(not r['node_match'] for r in result),'query_dwell_mismatches':sum(not r['query_match'] for r in result),'algorithms':{a:{'routes':sum(r['algorithm']==a for r in result),'node_mismatches':sum(r['algorithm']==a and not r['node_match'] for r in result),'query_mismatches':sum(r['algorithm']==a and not r['query_match'] for r in result)} for a in ['A','AP']}}
print(json.dumps(summary,indent=2));(out/'score_provenance.json').write_text(json.dumps(summary,indent=2))

# Preserve the mixed-schema source; this mapping is a diagnostic hypothesis.
with zipfile.ZipFile(repo/'results/final/source/original_aap_package/original_aap_package.zip') as archive:
 name=next(n for n in archive.namelist() if n.endswith('routes_all_30.csv'))
 original=list(csv.DictReader(io.StringIO(archive.read(name).decode('utf-8-sig'))))
groups=collections.defaultdict(list)
for r in original:
 number=int(float(r['query_no'] or r['query_id']))
 groups[r['city'],r['sweep'],r['param_value'],number,r['algo'],r['edge_ids']].append(float(r['kc']))
schema={'source_rows':len(original),'blank_query_no_rows':sum(not r['query_no'] for r in original),
        'provisional_path_keys':len(groups),'duplicate_rows_after_provisional_mapping':sum(len(v)-1 for v in groups.values()),
        'score_differing_path_keys_any_precision':sum(max(v)!=min(v) for v in groups.values()),
        'score_differing_path_keys_above_1e_6':sum(max(v)-min(v)>1e-6 for v in groups.values()),
        'identity_mapping_certified':False,
        'note':'Numeric query_id as replicate is a diagnostic hypothesis, not authorization to merge mixed-schema rows.'}
(out/'original_source_schema.json').write_text(json.dumps(schema,indent=2))
