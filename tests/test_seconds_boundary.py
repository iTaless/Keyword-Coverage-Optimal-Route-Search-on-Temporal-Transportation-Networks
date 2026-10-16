#!/usr/bin/env python3
"""Regression: time-of-day encodings must not inflate elapsed dwell duration."""
import csv
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    root=Path(__file__).resolve().parents[1]
    route,bl=map(lambda p:str(Path(p).resolve()),sys.argv[1:3])
    with tempfile.TemporaryDirectory(prefix='seconds_boundary_') as directory:
        d=Path(directory)
        (d/'stations.csv').write_text('station_id,dwell_min,keyword,coverage\nS,1,k,1\nT,1,k,0\n')
        (d/'trips.csv').write_text('edge_id,from,to,departure_min,arrival_min,cost\ne1,S,T,36900,36960,1\n')
        fields=['query_id','city','parameter','value','query_no','source','target','start_min','end_min','budget','keywords','weights','k','dwell_seconds','seed']
        with (d/'queries.csv').open('w',newline='') as f:
            w=csv.writer(f);w.writerow(fields)
            for name,start in [('wait15',36000),('wait25',35400)]:
                w.writerow([name,'Demo','default','-',1,'S','T',start,37020,5,'k',1,1,1200,1])
        subprocess.run([sys.executable,str(root/'scripts/run_experiments.py'),
                        '--binary',route,'--bl-binary',bl,
                        '--dataset',f'Demo:{d}/stations.csv:{d}/trips.csv',
                        '--queries',str(d/'queries.csv'),'--out',str(d/'results.jsonl'),
                        '--algorithms','BL,A,AP','--timeout','10'],check=True,capture_output=True,text=True)
        records=[json.loads(line) for line in (d/'results.jsonl').read_text().splitlines()]
        assert len(records)==6
        for record in records:
            assert record['time_basis']=='processed_seconds'
            assert record['solutions'][0]['score']==(0 if record['query_id']=='wait15' else 1)
    spec=importlib.util.spec_from_file_location('converter',root/'scripts/convert_austin_raw.py')
    converter=importlib.util.module_from_spec(spec);spec.loader.exec_module(converter)
    assert converter.hms_to_seconds('100000')-converter.hms_to_seconds('95900')==60
    assert converter.hms_to_seconds('240000')==86400
    spec=importlib.util.spec_from_file_location('gtfs_converter',root/'scripts/gtfs_to_trips.py')
    gtfs=importlib.util.module_from_spec(spec);spec.loader.exec_module(gtfs)
    assert gtfs.elapsed_seconds('10:00:30')-gtfs.elapsed_seconds('09:59:30')==60
    assert gtfs.elapsed_seconds('24:00:05')==86405
    print('Seconds boundary checks passed for BL/A/AP, HHMMSS and GTFS conversion.')


if __name__=='__main__':main()
