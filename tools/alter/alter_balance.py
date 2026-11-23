# ALTER v1.14 (Romain 23/11): the factory sounds' LEVEL by families (FRONT -25, BACK -31, the rest -28 LUFS).
# 1. node tools/alter/alter_render.mjs build/emu/felucca.wasm /tmp/r ; 2. ffmpeg ebur128 on /tmp/r/wav/sound_*.wav -> 'sound_NN_ENG_NAME LUFS' lines
# 3. python3 tools/alter/alter_balance.py lufs.txt (edits alter_sounds.c; run again until no change)
import re,sys
lufs={}
for l in open(sys.argv[1]):
    n,v=l.split(); i=int(n.split('_')[1])-1; lufs[i]=(n,float(v))
FRONT={'KICK','THUD','TOMS','GROWL','WARM'}
BACK={'HAT','OPEN HAT','RAIN','WIND','CRUSH','METAL','ARCADE'}
p='firmware/src/alter_sounds.c';L=open(p).read().split('\n')
idx=0;start=None
for i,l in enumerate(L):
    if 'ALT_PRE[' in l and '=' in l: start=i
    if start is None: continue
    m=re.match(r'(\s*\{"([A-Z ]+)", (\d), \{[^}]*\}, \{[^}]*\}, \{[^}]*\}, )(\d+)(.*)',l)
    if not m:
        if idx>0 and l.strip().startswith('};'): break
        continue
    name=m.group(2); n,v=lufs[idx]
    t=-25 if name in FRONT else -31 if name in BACK else -28
    old=int(m.group(4)); d=round(2*(t-v)); new=old+(d if abs(d)>1 else 0); cl=min(127,max(0,new))
    L[i]=m.group(1)+str(cl)+m.group(5)
    if cl!=old: print(name,v,t,old,'->',cl,'(short %.1f)'%((new-cl)/2) if cl!=new else '')
    idx+=1
open(p,'w').write('\n'.join(L))
