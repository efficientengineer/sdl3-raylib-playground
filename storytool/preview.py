"""story/out/<scene>.preview.html: the manga page, on the Mac.

page_layout composes the panels the way the game does, reveal_plan applies the reveal
rule, and the HTML reveals them line by line with a dialogue box. Missing panel images
become labelled placeholders, so a scene's pacing can be judged before any art exists.
This is the reference for the in-game player: a change here is a change in star_logic.cpp.

Must never do: drift from the game's layout without the engine side being changed too.
Public: page_layout, reveal_plan, cmd_preview, LAYOUTS. Imports: cast, paths, png, rules,
scenes, style.
"""
import json
import sys
from .cast import is_narrator, load_cast
from .paths import OUT, ROOT
from .png import png_size
from .rules import SHAPE_ASPECT
from .scenes import reveal_plan, backdrop_panel, panel_file, parse_scene, portrait_file, scene_paths, speaker_sides, validate
from .style import load_style



# ───────────────────────── Browser preview (panel reveal + dialogue) ─────────────────────────

# Two page layouts, as percentages of a stage. "land": 16:10 stage, dialogue box overlays the bottom.
# "port": 4:5 stage for an upright phone, dialogue box sits below the stage.
LAYOUTS = {
    "land": {"aspect": 1.6, "bottom": 74.0, "tuck": (7, 9), "slots": {
        "wide": (52, [(4, 5), (20, 40)]), "tall": (22, [(73, 3), (5, 8)]),
        "slit": (60, [(20, 52), (8, 6)]), "square": (17, None)}},
    "port": {"aspect": 0.8, "bottom": 99.0, "tuck": (14, 10), "slots": {
        "wide": (80, [(3, 2), (17, 52)]), "tall": (36, [(61, 10), (3, 30)]),
        "slit": (92, [(4, 40), (4, 8)]), "square": (30, None)}},
}


def page_layout(scene, style, mode="land"):
    """Where each panel sits on the composed page. Square panels tuck over the previous panel's corner."""
    lay, out, used, prev, page = LAYOUTS[mode], [], {}, None, 0
    for n, (sid, desc) in enumerate(scene["panels"], 1):
        if scene["pages"][n - 1] != page:                  # a new page starts from an empty screen
            page, used, prev = scene["pages"][n - 1], {}, None
        shape = style["shots"][sid]["shape"]
        img = panel_file(scene, n, sid)
        size = png_size(img) if img.exists() else None
        aspect = size[0] / size[1] if size else SHAPE_ASPECT[shape]
        w, spots = lay["slots"][shape]
        if spots:
            x, y = spots[min(used.get(shape, 0), len(spots) - 1)]
        elif prev:
            x, y = prev["x"] + prev["w"] - lay["tuck"][0], prev["y"] + prev["h"] - lay["tuck"][1]
        else:
            x, y = 6, 40
        h = w / aspect * lay["aspect"]
        x = max(1, min(x, 99 - w))
        y = max(1, min(y, lay["bottom"] - h))
        used[shape] = used.get(shape, 0) + 1
        prev = {"n": n, "page": page, "shot": sid, "x": round(x, 1), "y": round(y, 1), "w": w, "h": round(h, 1),
                "img": f"../panels/{img.name}" if size else None, "file": img.name, "content": desc}
        out.append(prev)
    return out


PREVIEW_HTML = r"""<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>__TITLE__</title>
<style>
html,body{margin:0;height:100%;background:#000;color:#fff;font-family:ui-monospace,Menlo,monospace}
#stage{position:relative;width:min(100vw,160vh);aspect-ratio:16/10;margin:0 auto;background:#000;overflow:hidden;
 cursor:pointer;user-select:none;container-type:inline-size}
.p{position:absolute;opacity:0;transform:scale(.97);transition:opacity .25s,transform .25s;box-sizing:border-box}
.p.on{opacity:1;transform:none}
.p img{width:100%;height:100%;display:block;image-rendering:pixelated}
.p.ph{background:#15131f;border:.35cqw solid #fff;outline:.15cqw solid #222;outline-offset:-.5cqw;
 display:flex;align-items:center;justify-content:center;text-align:center;font-size:1.5cqw;color:#889;padding:1cqw}
#bd{position:absolute;inset:0;width:100%;height:100%;object-fit:contain;image-rendering:pixelated;
 filter:brightness(.34) saturate(.75)}
#box{position:absolute;left:4%;right:4%;bottom:2%;height:22%;background:#0b2a7c;border:.45cqw solid #d8d8e0;
 outline:.25cqw solid #5a5a6a;border-radius:.6cqw;box-sizing:border-box;padding:1.4cqw 2cqw;z-index:99}
#who{color:#ffd84a;font-weight:700;font-size:2cqw;margin-bottom:.3cqw}
#txt{font-weight:700;font-size:2.4cqw;line-height:1.3;white-space:pre-wrap}
#face{position:absolute;top:7%;height:86%;width:auto;display:none;box-sizing:border-box;background:#05060f;
 border:.35cqw solid #d8d8e0;outline:.15cqw solid #5a5a6a;object-fit:cover;image-rendering:pixelated}
#arrow{position:absolute;right:1.6cqw;bottom:.6cqw;font-size:2cqw;animation:b 1s steps(2) infinite}
@keyframes b{50%{opacity:0}}
@keyframes fin{from{opacity:0;transform:translateX(var(--fx))}to{opacity:1;transform:none}}
</style>
<div id="stage"><div id="box"><img id="face" alt=""><div id="inner"><div id="who"></div><div id="txt"></div></div>
 <div id="arrow">&#9660;</div></div></div>
<script>
const D=__DATA__, stage=document.getElementById('stage'), who=document.getElementById('who'),
      txt=document.getElementById('txt'), arrow=document.getElementById('arrow'),
      box=document.getElementById('box'), face=document.getElementById('face'),
      inner=document.getElementById('inner');
const port=location.hash.includes('port')||(!location.hash.includes('land')&&innerWidth<innerHeight);
if(D.backdrop){const b=new Image();b.id='bd';b.src=D.backdrop;stage.insertBefore(b,box)}
// Speaker portrait at one end of the box, the text narrowed to make room (Phantasy Star IV field talk).
let faceRight=false,faceKey=null;
function fitFace(){const g=box.clientWidth*0.015,w=face.offsetWidth+g*2;
  inner.style.marginLeft=faceRight?0:w+'px';inner.style.marginRight=faceRight?w+'px':0}
face.onload=fitFace;
function setFace(l){
  if(!l.portrait){face.style.display='none';faceKey=null;inner.style.margin='0';return}
  faceRight=l.side===1;face.style.display='block';
  face.style.left=faceRight?'auto':'1.5%';face.style.right=faceRight?'1.5%':'auto';
  if(l.portrait+faceRight!==faceKey){faceKey=l.portrait+faceRight;face.src=l.portrait;
    face.style.setProperty('--fx',(faceRight?'':'-')+'3cqw');
    face.style.animation='none';void face.offsetWidth;face.style.animation='fin .22s ease-out'}
  fitFace()}
if(port){stage.style.width='min(100vw,62vh)';stage.style.aspectRatio='4/6.4';D.panels=D.port;
  const b=document.getElementById('box');b.style.height='20%';b.style.bottom='1%'}
const els=D.panels.map(p=>{const e=document.createElement('div');e.className='p'+(p.img?'':' ph');
  e.style.cssText=port?`left:${p.x}%;top:${p.y*0.78}%;width:${p.w}%;height:${p.h*0.78}%`
                      :`left:${p.x}%;top:${p.y}%;width:${p.w}%;height:${p.h}%`;
  if(p.img){const i=new Image();i.src=p.img;e.appendChild(i)}else e.textContent=`P${p.n} ${p.shot}\n${p.content}`;
  stage.insertBefore(e,document.getElementById('box'));return e});
let line=-1,typing=null,z=1;
let page=0;
function reveal(n){const e=els[n-1],p=D.panels[n-1];if(!e)return;if(p.page!==page){page=p.page;els.forEach(x=>x.classList.remove('on'))}
  if(!e.classList.contains('on')){e.style.zIndex=z++;e.classList.add('on')}}
function show(){const l=D.lines[line];reveal(l.reveal);setFace(l);who.textContent=l.speaker;txt.textContent='';arrow.style.display='none';
  let i=0;typing=setInterval(()=>{txt.textContent=l.text.slice(0,++i);if(i>=l.text.length)done()},28)}
function done(){clearInterval(typing);typing=null;txt.textContent=D.lines[line].text;arrow.style.display=''}
function next(){if(typing)return done();
  if(line>=D.lines.length-1){els.forEach(e=>e.classList.remove('on'));z=1;line=-1}
  line++;show()}
stage.onclick=next;document.onkeydown=e=>{if(e.key===' '||e.key==='Enter')next()};
const upto=(location.hash.match(/line(\d+)/)||[])[1];
if(upto){for(let i=0;i<Math.min(+upto,D.lines.length);i++)reveal(D.lines[i].reveal);line=Math.min(+upto,D.lines.length)-1;show();done()}
else if(location.hash.includes('all')){D.panels.forEach(p=>reveal(p.n));if(D.lines.length){line=D.lines.length-1;show();done()}}
else if(D.lines.length)next();else D.panels.forEach(p=>reveal(p.n));
</script>
"""


def cmd_preview(args):
    style, cast = load_style(), load_cast()
    for path in scene_paths(args):
        scene = parse_scene(path)
        err, _ = validate(scene, style, cast)
        if err:
            for e in err:
                print(f"{path.name}: ERROR: {e}", file=sys.stderr)
            sys.exit("story_prompt: scene rejected. Fix the scene file; do not bypass the rules.")
        panels = page_layout(scene, style, "land")
        sides = speaker_sides(scene, cast)
        lines = [{"speaker": "" if is_narrator(who) else who, "text": text, "reveal": n,
                  "side": sides[who.strip().lower()],
                  "portrait": (lambda p: f"../portraits/{p.name}" if p else None)(portrait_file(who, cast))}
                 for (who, text), n in zip(scene["dialogue"], reveal_plan(scene))]
        bd = backdrop_panel(scene) if scene["talk"] else None
        data = {"panels": panels, "port": page_layout(scene, style, "port"), "lines": lines,
                "backdrop": f"../panels/{bd[2].name}" if bd and bd[2] and bd[2].exists() else None}
        OUT.mkdir(exist_ok=True)
        html = OUT / f"{scene['stem']}.preview.html"
        html.write_text(PREVIEW_HTML.replace("__TITLE__", scene["title"]).replace("__DATA__", json.dumps(data)))
        have = sum(1 for p in panels if p["img"])
        print(f"wrote {html.relative_to(ROOT.parent)}  ({have}/{len(panels)} panel images found, "
              f"{len(lines)} dialogue lines). Open it in a browser; tap or press space to advance.")
