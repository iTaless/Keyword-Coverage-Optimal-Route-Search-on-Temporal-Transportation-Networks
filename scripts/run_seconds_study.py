#!/usr/bin/env python3
"""Run the three methods using one fixed query row and processed-second inputs."""
import argparse
import subprocess
import sys
from pathlib import Path


def main():
    root=Path(__file__).resolve().parents[1]
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--timeout',type=float,default=300)
    p.add_argument('--limit',type=int,default=0)
    p.add_argument('--city',choices=('Austin','Houston'))
    p.add_argument('--out',default='results/seconds_unified/main_study.jsonl')
    a=p.parse_args()
    command=[sys.executable,str(root/'scripts/run_experiments.py'),
             '--binary',str(root/'build/route_search'),
             '--bl-binary',str(root/'build/kcrn_bl'),
             '--queries',str(root/'queries/final_queries_1260.csv'),
             '--out',str(root/a.out),'--algorithms','BL,A,AP','--main-study-only',
             '--timeout',str(a.timeout),'--rho','8','--rounds','20','--runs-policy','k',
             '--time-unit','seconds','--dwell-description','query_level_dwell_seconds_from_fixed_csv']
    for city in ('Austin','Houston'):
        command+=['--dataset',f'{city}:{root}/data/processed/{city}/stations.csv:{root}/data/processed/{city}/trips.csv']
    if a.limit:command+=['--limit',str(a.limit)]
    if a.city:command+=['--city',a.city]
    raise SystemExit(subprocess.run(command).returncode)


if __name__=='__main__':main()
