import json, sys
d=json.load(open('out/ladder.json'))
K=dict(mean=0.010,p95=0.04,edge=0.9)
def ok(m): return m['mean']<K['mean'] and m['p95']<K['p95'] and m['edge']>K['edge']
for a in d:
    print(f"\n=== {a['cls']}/{a['name']}  src {a['src'][0]}x{a['src'][1]}  pitch~{a['pitch']}  "
          f"disp phone {a['disp']['phone']} big {a['disp']['big']}")
    keys=list(a['rows'][0]['dev'])
    print(f"  {'label':7s} {'stored':11s} {'rgba':>8s} {'idx':>7s} " + " ".join(f"{k:>34s}" for k in keys))
    knee=None
    for r in a['rows']:
        cells=[]
        for k in keys:
            m=r['dev'][k]
            cells.append(f"m{m['mean']:.4f} p{m['p95']:.3f} o{m['over']*100:4.1f}% e{m['edge']:.2f}")
        mark=''
        pk=[k for k in keys if k.startswith('phone')][0]
        if ok(r['dev'][pk]): mark=' OK'
        print(f"  {r['label']:7s} {r['stored'][0]:4d}x{r['stored'][1]:<5d} {r['rgba']:8d} {r['idx']:7d} "+" ".join(f"{c:>34s}" for c in cells)+mark)
