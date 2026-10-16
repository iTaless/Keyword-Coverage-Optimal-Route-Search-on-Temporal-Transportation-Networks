#!/usr/bin/env python3
"""Regression tests for fractional budget boundaries and positive dwell input."""
import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    a_binary,bl_binary=[str(Path(x).resolve()) for x in sys.argv[1:3]]
    with tempfile.TemporaryDirectory(prefix='numeric_budget_') as directory:
        d=Path(directory)
        (d/'stations.csv').write_text('station_id,dwell_min,keyword,coverage\nS,1,x,1\nM,1,x,0\nT,1,x,0\n')
        (d/'trips.csv').write_text('edge_id,from,to,departure_min,arrival_min,cost\ne1,S,M,1,2,0.1\ne2,M,T,3,4,0.2\n')
        with (d/'queries.csv').open('w',newline='') as f:
            w=csv.writer(f);w.writerow(['query_id','city','parameter','value','source','target','start_min','end_min','budget','keywords','weights','k','dwell_seconds','seed'])
            w.writerow(['fraction','Demo','test',1,'S','T',0,10,.3,'x',1,1,1,1])
            w.writerow(['zero_dwell','Demo','test',1,'S','T',0,10,.3,'x',1,1,0,1])
        for alg in ('A','AP','BL'):
            cmd=[bl_binary if alg=='BL' else a_binary,'--stations',str(d/'stations.csv'),
                 '--trips',str(d/'trips.csv'),'--queries',str(d/'queries.csv'),
                 '--algorithm',alg,'--query-id','fraction']
            process=subprocess.run(cmd,capture_output=True,text=True,check=True)
            r=json.loads(process.stdout)
            assert r['status']=='ok' and len(r['solutions'])==1
            assert abs(r['solutions'][0]['budget']-.3)<1e-9
            cmd[-1]='zero_dwell'
            process=subprocess.run(cmd,capture_output=True,text=True)
            assert process.returncode!=0, 'dQ=0 must be rejected by the paper interface'
    print('Fractional budget and positive query-dwell regressions passed for BL/A/AP.')


if __name__=='__main__':main()
