#!/usr/bin/env python3
# The PLAY screen of ALTER redrawn in vectors (240x240 space, from the emulator capture) -> crisp 1024x1024 Hub cards.
import math
NIGHT = dict(bg='#0E0C10', ink='#EBD9A8', gold='#C9A24D', dim='#A89A86', crim='#C9504A', verd='#4FA88C', dot='#E2B85A',
             rib='#4A1A1F', ribtx='#EBD9A8', moon='#2A2418', pf=('#3A1E22', '#163830', '#7A5C22'), grain=0.04)
DAY = dict(bg='#F3E6C8', ink='#2E2B55', gold='#C8962E', dim='#6F604B', crim='#E0664A', verd='#2A9D8F', dot='#D99A22',
           rib='#2E2B55', ribtx='#F3E6C8', moon='#E4D2AC', pf=('#E8B0A0', '#9ED0C6', '#E6C479'), grain=0.05)
CX, CY = 120.0, 116.0

def P(r, deg):
    a = math.radians(deg)
    return CX + r * math.cos(a), CY + r * math.sin(a)

def screen(c, playing=False, uid='a'):
    s = [f'<rect width="240" height="240" fill="{c["bg"]}"/>',
         f'<filter id="g{uid}"><feTurbulence type="fractalNoise" baseFrequency="1.6" numOctaves="2" stitchTiles="stitch"/><feColorMatrix type="saturate" values="0"/></filter>',
         f'<rect width="240" height="240" filter="url(#g{uid})" opacity="{c["grain"]}"/>',
         f'<filter id="b{uid}" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="2.2"/></filter>']
    for i in range(48):                                    # the rays (kept off the knobs)
        x1, y1 = P(14, i * 7.5); x2, y2 = P(104, i * 7.5)
        s.append(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{c["gold"]}" stroke-opacity="{0.07 if i % 2 else 0.13}" stroke-width="0.4"/>')
    g = c['gold']
    s += [f'<circle cx="{CX}" cy="{CY}" r="{r}" fill="none" stroke="{g}" stroke-opacity="{o}" stroke-width="{w}"/>'
          for r, o, w in ((60, 0.75, 0.7), (52, 0.35, 0.4), (40, 0.6, 0.6), (30, 0.3, 0.4))]
    q = [P(80, d) for d in (-90, 0, 90, 180)]              # the square through the diamonds
    s.append('<polygon points="' + ' '.join(f'{x:.2f},{y:.2f}' for x, y in q) + f'" fill="none" stroke="{g}" stroke-opacity="0.5" stroke-width="0.5"/>')
    st = [P(40, -90 + k * 30) for k in range(12)]           # the sigils: a 12/5 star, a 8/3 star
    s.append('<polygon points="' + ' '.join(f'{st[(k * 5) % 12][0]:.2f},{st[(k * 5) % 12][1]:.2f}' for k in range(12)) + f'" fill="none" stroke="{g}" stroke-opacity="0.45" stroke-width="0.5"/>')
    st8 = [P(52, -90 + k * 45) for k in range(8)]
    s.append('<polygon points="' + ' '.join(f'{st8[(k * 3) % 8][0]:.2f},{st8[(k * 3) % 8][1]:.2f}' for k in range(8)) + f'" fill="none" stroke="{g}" stroke-opacity="0.3" stroke-width="0.45"/>')
    eyes = [P(60, d) for d in (-135.5, -21, 112.5)]
    s.append('<polygon points="' + ' '.join(f'{x:.2f},{y:.2f}' for x, y in eyes) + f'" fill="none" stroke="{c["verd"]}" stroke-opacity="0.85" stroke-width="0.7"/>')
    # the crimson ring (chosen circle), with its glow
    s.append(f'<circle cx="{CX}" cy="{CY}" r="80" fill="none" stroke="{c["crim"]}" stroke-opacity="0.45" stroke-width="5" filter="url(#b{uid})"/>')
    s.append(f'<circle cx="{CX}" cy="{CY}" r="80" fill="none" stroke="{c["crim"]}" stroke-width="3"/>')
    if playing:                                             # the hand
        x, y = P(80, -128)
        s.append(f'<line x1="{CX}" y1="{CY}" x2="{x:.2f}" y2="{y:.2f}" stroke="{c["crim"]}" stroke-width="1.2" stroke-linecap="round"/>')
    for k in range(12):                                     # the gold dots
        x, y = P(40, -90 + k * 30)
        s.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.9" fill="{c["bg"]}"/><circle cx="{x:.2f}" cy="{y:.2f}" r="2.3" fill="{c["dot"]}"/>')
    for x, y in eyes:                                       # the verdigris hits
        s.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="5" fill="{c["bg"]}"/><circle cx="{x:.2f}" cy="{y:.2f}" r="3.6" fill="none" stroke="{c["verd"]}" stroke-width="1.6"/><circle cx="{x:.2f}" cy="{y:.2f}" r="0.9" fill="{c["ink"]}"/>')
    for x, y in q:                                          # the diamonds
        s.append(f'<path d="M{x:.2f} {y-5:.2f} L{x+3.2:.2f} {y:.2f} L{x:.2f} {y+5:.2f} L{x-3.2:.2f} {y:.2f} Z" fill="{c["crim"]}" stroke="{g}" stroke-width="0.45"/>')
    # the prism
    t, bl, br, m = (120, 96), (103, 128), (138, 128), (121, 118)
    f = lambda a, b, d, col: f'<path d="M{a[0]} {a[1]} L{b[0]} {b[1]} L{d[0]} {d[1]} Z" fill="{col}"/>'
    s += [f(t, bl, m, c['pf'][0]), f(t, br, m, c['pf'][1]), f(bl, br, m, c['pf'][2]),
          f'<path d="M{t[0]} {t[1]} L{bl[0]} {bl[1]} L{br[0]} {br[1]} Z" fill="none" stroke="{g}" stroke-width="1" stroke-linejoin="round"/>',
          f'<path d="M{t[0]} {t[1]} L{m[0]} {m[1]} M{bl[0]} {bl[1]} L{m[0]} {m[1]} M{br[0]} {br[1]} L{m[0]} {m[1]}" stroke="{g}" stroke-opacity="0.7" stroke-width="0.5"/>',
          f'<circle cx="113.5" cy="112" r="2" fill="{c["crim"]}"/>']
    # the head
    s += [f'<text x="9" y="17" font-family="Space Grotesk, sans-serif" font-size="11" fill="{c["dim"]}">XIV</text>',
          f'<path d="M84 3 L157 3 L152 11 L157 19 L84 19 L89 11 Z" fill="{c["rib"]}" stroke="{g}" stroke-width="0.6"/>',
          f'<text x="120.5" y="15.6" text-anchor="middle" font-family="Cinzel, serif" font-weight="800" font-size="13" letter-spacing="0.6" fill="{c["ribtx"]}">KICK</text>',
          f'<text x="230" y="17" text-anchor="end" font-family="Space Grotesk, sans-serif" font-size="11.5" fill="{c["ink"]}">132</text>',
          f'<text x="230" y="31" text-anchor="end" font-family="Space Grotesk, sans-serif" font-size="10" letter-spacing="0.2" fill="{c["dim"]}">ALTER  PERC</text>']
    if playing:
        s.append(f'<path d="M181 8 L187 11.5 L181 15 Z" fill="{g}"/>')
    else:
        s.append(f'<rect x="181" y="8.5" width="5.5" height="5.5" fill="{g}" fill-opacity="0.8"/>')
    # the knobs
    s.append(f'<line x1="8" y1="196.5" x2="232" y2="196.5" stroke="{g}" stroke-opacity="0.6" stroke-width="0.5"/>')
    x, y = q[2]
    s.append(f'<path d="M{x:.2f} {y-5:.2f} L{x+3.2:.2f} {y:.2f} L{x:.2f} {y+5:.2f} L{x-3.2:.2f} {y:.2f} Z" fill="{c["crim"]}" stroke="{g}" stroke-width="0.45"/>')
    for (lx, name), mx in zip(((8, 'MRPH'), (65, 'SPRD'), (122, 'HARM'), (180, 'RUMBLE')), (19, 75, 132, 189)):
        s.append(f'<text x="{lx}" y="212" font-family="Space Grotesk, sans-serif" font-size="10.5" fill="{c["dim"]}">{name}</text>')
        s.append(f'<circle cx="{mx}" cy="224.5" r="7.6" fill="{c["moon"]}" stroke="{g}" stroke-opacity="0.55" stroke-width="0.6"/>')
    s.append(f'<path d="M189 216.9 A7.6 7.6 0 0 1 189 232.1 A3 7.6 0 0 0 189 216.9 Z" fill="{g}"/>')
    # the frame and its corners
    s.append(f'<rect x="3" y="3" width="234" height="234" fill="none" stroke="{g}" stroke-opacity="0.85" stroke-width="0.8"/>')
    for cx, cy, sx, sy in ((3, 3, 1, 1), (237, 3, -1, 1), (3, 237, 1, -1), (237, 237, -1, -1)):
        s.append(f'<path d="M{cx+7*sx} {cy} A7 7 0 0 {1 if sx*sy>0 else 0} {cx} {cy+7*sy}" fill="none" stroke="{g}" stroke-width="0.8"/><circle cx="{cx+2.6*sx}" cy="{cy+2.6*sy}" r="1" fill="{g}"/>')
    return ''.join(s)

def card(inner, size=1024):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 240" width="{size}" height="{size}">{inner}</svg>'

cards = {}
cards['1-ecran'] = card(screen(NIGHT, playing=False, uid='a'))
# 2 · the screen, playing, in a dark field with the name
s2 = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">'
      f'<rect width="1024" height="1024" fill="#08070A"/>'
      f'<text x="512" y="118" text-anchor="middle" font-family="Cinzel, serif" font-weight="800" font-size="92" letter-spacing="14" fill="#EBD9A8">ALTER</text>'
      f'<rect x="148" y="166" width="728" height="728" fill="none" stroke="#C9A24D" stroke-opacity="0.35" stroke-width="2"/>'
      f'<svg x="156" y="174" width="712" height="712" viewBox="0 0 240 240">{screen(NIGHT, playing=True, uid="b")}</svg>'
      f'<text x="512" y="958" text-anchor="middle" font-family="JetBrains Mono, monospace" font-size="28" letter-spacing="6" fill="#A89A86">EUCLIDEAN CHAOS · M-VAVE FM-1</text>'
      f'</svg>')
cards['2-ecran-titre'] = s2
cards['3-jour'] = card(screen(DAY, playing=True, uid='c'))
import os
for k, v in cards.items():
    open(f'/home/claude/hubcard/{k}.svg', 'w').write(v)
print('ok')
