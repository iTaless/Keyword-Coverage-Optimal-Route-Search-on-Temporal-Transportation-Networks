#!/usr/bin/env python3
"""Plot formal runtime boxes and individual points without mixing BL recovery times."""
import argparse
import csv
from pathlib import Path


def panel(ax, rows, factor, city):
    import numpy as np
    from matplotlib.colors import to_rgb
    values=sorted({float(r['value']) for r in rows if r['city']==city and r['parameter']==factor})
    colors={'BL':'#df7e27','A':'#346ac3','AP':'#269c73'}
    rng=np.random.default_rng(20261016)
    for alg_index,alg in enumerate(('BL','A','AP')):
        for index,value in enumerate(values):
            group=[r for r in rows if r['city']==city and r['parameter']==factor and float(r['value'])==value and r['algorithm']==alg]
            times=[float(r['runtime_sec']) for r in group if r['status'] in ('completed','ok','no_route') and r['runtime_sec']]
            x=index+(alg_index-1)*.23
            if times:
                assert all(t>0 for t in times), 'Log plot requires positive measured runtimes'
                box=ax.boxplot([times],positions=[x],widths=.20,patch_artist=True,
                               showfliers=False,manage_ticks=False)
                pale=tuple(.25*c+.75 for c in to_rgb(colors[alg]))
                for patch in box['boxes']:patch.set(facecolor=pale)
                for part in ('whiskers','caps','medians'):
                    for line in box[part]:line.set(color=colors[alg],linewidth=.9)
                ax.scatter(x+rng.uniform(-.07,.07,len(times)),times,color=colors[alg],s=5)
            n=sum(r['status']=='timeout' for r in group)
            if n:ax.text(x,340,f'T{n}',color='#a52121',fontsize=6,ha='center')
        ax.plot([],[],color=colors[alg],label='A*' if alg=='A' else alg)
    ax.axhline(300,color='#777777',linestyle='--',linewidth=.8)
    ax.set_yscale('log');ax.set_ylim(.8,500)
    ax.set_xticks(range(len(values)),[f'{v:g}' for v in values])
    ax.set_title(city)
    ax.set_xlabel({'keywords':'Keyword-set size','budget':'Budget','k':'Requested k','window':'Time window (min)'}[factor])
    ax.set_ylabel('Formal runtime (s, log scale)')
    ax.legend(fontsize=7,frameon=False,ncol=3)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',required=True,help='historical runtime.csv; recovery_runtime_sec is ignored')
    p.add_argument('--out-dir',required=True)
    a=p.parse_args()
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    with open(a.runtime,newline='',encoding='utf-8-sig') as f:rows=list(csv.DictReader(f))
    out=Path(a.out_dir);out.mkdir(parents=True,exist_ok=True)
    for num,factor in enumerate(('keywords','budget','window','k'),2):
        for city in ('Austin','Houston'):
            fig,ax=plt.subplots(figsize=(5.4,3.5),layout='constrained')
            panel(ax,rows,factor,city)
            for extension in ('png','pdf','eps'):
                fig.savefig(out/f'figure{num}_{city.lower()}.{extension}',dpi=180)
            plt.close(fig)
        fig,axes=plt.subplots(1,2,figsize=(10.8,3.5),layout='constrained')
        for ax,city in zip(axes,('Austin','Houston')):panel(ax,rows,factor,city)
        fig.savefig(out/f'figure{num}_two_cities.pdf')
        plt.close(fig)
    print('Saved 8 city-specific boxplots with completed-query dots; timeout counts are separate.')


if __name__=='__main__':main()
