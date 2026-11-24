import importlib.util, sys
spec=importlib.util.spec_from_file_location('g',__import__('os').path.join(__import__('os').path.dirname(__file__),'screen_card.py')); g=importlib.util.module_from_spec(spec); spec.loader.exec_module(g)
day=g.screen(g.DAY, playing=True, uid='d'); night=g.screen(g.NIGHT, playing=True, uid='n')
CREAM, DARK, INDIGO, IVORY = '#F3E6C8', '#08070A', '#2E2B55', '#EBD9A8'
def title(col, cls):
    return (f'<text class="{cls} tL" x="0" y="118" text-anchor="end" font-family="Cinzel, serif" font-weight="800" font-size="92" fill="{col}">AL</text>'
            f'<text class="{cls} tT" x="512" y="118" text-anchor="middle" font-family="Cinzel, serif" font-weight="800" font-size="92" fill="{col}">T</text>'
            f'<text class="{cls} tR" x="0" y="118" text-anchor="start" font-family="Cinzel, serif" font-weight="800" font-size="92" fill="{col}">ER</text>'
            f'<text x="478" y="958" text-anchor="end" font-family="JetBrains Mono, monospace" font-size="28" letter-spacing="6" fill="{col}" fill-opacity="0.75">EUCLIDEAN CHAOS</text>'f'<text x="512" y="958" text-anchor="middle" font-family="JetBrains Mono, monospace" font-size="28" fill="{col}" fill-opacity="0.75">·</text>'f'<text x="546" y="958" text-anchor="start" font-family="JetBrains Mono, monospace" font-size="28" letter-spacing="6" fill="{col}" fill-opacity="0.75">M-VAVE FM-1</text>')
svg=(f'<svg id="s" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">'
 f'<defs><clipPath id="L"><rect x="0" y="0" width="512" height="1024"/></clipPath><clipPath id="R"><rect x="512" y="0" width="512" height="1024"/></clipPath>'
 f'<clipPath id="sl"><rect x="0" y="0" width="120" height="240"/></clipPath><clipPath id="sr"><rect x="120" y="0" width="120" height="240"/></clipPath></defs>'
 f'<rect width="512" height="1024" fill="{CREAM}"/><rect x="512" width="512" height="1024" fill="{DARK}"/>'
 f'<g clip-path="url(#L)">{title(INDIGO,"a")}<rect x="148" y="166" width="728" height="728" fill="none" stroke="#C8962E" stroke-opacity="0.5" stroke-width="2"/></g>'
 f'<g clip-path="url(#R)">{title(IVORY,"b")}<rect x="148" y="166" width="728" height="728" fill="none" stroke="#C9A24D" stroke-opacity="0.35" stroke-width="2"/></g>'
 f'<svg x="156" y="174" width="712" height="712" viewBox="0 0 240 240"><g clip-path="url(#sl)">{day}</g><g clip-path="url(#sr)">{night}</g></svg>'
 f'</svg>')
js='''<script>
const T=document.querySelector('.tT').getBBox(); const gap=3;
document.querySelectorAll('.tL').forEach(e=>e.setAttribute('x', T.x-gap));
document.querySelectorAll('.tR').forEach(e=>e.setAttribute('x', T.x+T.width+gap));
window.tmid=T.x+T.width/2;
</script>'''
open('split.html','w').write('<html><body style="margin:0">'+svg+js+'</body></html>')
