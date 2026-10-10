#!/usr/bin/env python3
"""Five optional 3-D pickups for the clean-room ZIP. All geometry authored.

AXBM v1: header `AXBM`, u16 version=1, u16 count=5; repeated:
u16 vertex_count, u16 face_count; vertices eight LE floats (x/y/z, normal,
U/V), faces u8 size + size*u16 indices. Loadable on armhf and arm64.
"""
import math, struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

TYPES=['SHIELD','SNOWFLAKE','LIVES_X2','SCORE_X2','SAFE_TAIL','MAGNET']
H=384; SLOT=64

def model_outline(kind):
    if kind==0:
        return [(-.4,.42),(.4,.42),(.5,.25),(.44,-.20),(0,-.58),(-.44,-.20),(-.5,.25)]
    if kind==1:
        # central hexagon and branches are separate quads and spokes
        return None
    if kind==2:
        # The x/y contour is transcribed from the existing original-heart
        # constructor at 0x421D39; only face materials and X2 are new.
        pts=[(0,3),(2,5),(3,5.3),(4,5),(4.7,4),(5,3),(4.8,2),(4,0),(.1,-5),(-.1,-5),(-4,0),(-4.8,2),(-5,3),(-4.7,4),(-4,5),(-3,5.3),(-2,5)]
        return [(x*.102,y*.102) for x,y in pts]
    if kind==3:
        return [(math.cos(math.pi/2+i*math.pi/5)*(.55 if i%2==0 else .26),math.sin(math.pi/2+i*math.pi/5)*(.55 if i%2==0 else .26)) for i in range(10)]
    if kind==4:return [(-.53,-.25),(-.35,.14),(-.1,.55),(.05,.2),(.35,.5),(.28,.08),(.54,-.30)]
    # Horseshoe/U-shaped magnet with open top, procedurally extruded.
    return [(-.52,.49),(-.22,.49),(-.22,-.04),(-.17,-.24),(.17,-.24),(.22,-.04),(.22,.49),(.52,.49),(.52,-.09),(.4,-.42),(.12,-.57),(-.12,-.57),(-.4,-.42),(-.52,-.09)]

def add_extruded(v,faces,outline,idx):
    if not outline:return
    n=len(outline);uMid=.5;vMid=(idx*SLOT+SLOT*.5)/H
    # Face fan. All coordinates small like original procedural pickups.
    start=len(v)
    def vertex(x,y,z,nz):
        u=.5+x*.70; texv=(idx*SLOT+SLOT*.5-y*SLOT*.68)/H
        v.append((x*.009,y*.009,z,0.,0.,nz,u,texv));return len(v)-1
    front=[vertex(x,y,.00085,1.) for x,y in outline]
    back=[vertex(x,y,-.00085,-1.) for x,y in outline]
    center=vertex(sum(x for x,y in outline)/n,sum(y for x,y in outline)/n,.00085,1.)
    for j in range(n):
        k=(j+1)%n
        faces.append((center,front[j],front[k]))
        faces.append((back[k],back[j],center))
        faces.append((front[j],back[j],back[k],front[k]))

def generate(root:Path):
    root=Path(root);out=root/'models';out.mkdir(parents=True,exist_ok=True)
    meshes=[]
    for k in range(6):
        verts=[];faces=[]
        if k==1:
            # Snowflake: six extruded spokes + two side branches each.
            for i in range(6):
                a=i*math.pi/3
                def rect(r1,r2,w):
                    ux,uy=math.cos(a),math.sin(a)
                    nx,ny=-uy,ux
                    pts=[(ux*r1+nx*w,uy*r1+ny*w),(ux*r2+nx*w,uy*r2+ny*w),
                         (ux*r2-nx*w,uy*r2-ny*w),(ux*r1-nx*w,uy*r1-ny*w)]
                    add_extruded(verts,faces,pts,k)
                rect(0,.55,.065)
                for d in [-1,1]:
                    t=.35
                    ux,uy=math.cos(a),math.sin(a)
                    vx,vy=math.cos(a+d*.65),math.sin(a+d*.65)
                    pts=[(ux*t+vx*.15,uy*t+vy*.15),(ux*t+vx*.11,uy*t+vy*.11),
                         (ux*t-vx*.03,uy*t-vy*.03),(ux*t+vx*.01,uy*t+vy*.01)]
                    add_extruded(verts,faces,pts,k)
        else:add_extruded(verts,faces,model_outline(k),k)
        meshes.append((verts,faces))
    data=bytearray(b'AXBM'+struct.pack('<HH',1,len(meshes)))
    for verts,faces in meshes:
        data+=struct.pack('<HH',len(verts),len(faces))
        for v in verts:data+=struct.pack('<8f',*v)
        for face in faces:data+=struct.pack('<B',len(face))+struct.pack('<'+'H'*len(face),*face)
    (out/'bonus_models.axbm').write_bytes(data)
    # A matching 256x384 UV atlas with original-style flat bright materials.
    im=Image.new('RGBA',(256,H),(0,0,0,0));d=ImageDraw.Draw(im)
    pal=[('#148fce','#e0faff'),('#379cde','#eaffff'),('#c81930','#ffc52f'),('#cc7f0c','#ffdb3d'),('#e74113','#ffb431'),('#d52c3d','#80aaff')]
    for k,(c1,c2) in enumerate(pal):
        off=k*SLOT;cx=128;cy=off+25
        if k==0:
            d.polygon([(70,off+5),(185,off+5),(187,off+29),(128,off+46),(69,off+29)],fill=c1,outline=c2,width=4)
            d.ellipse((113,off+14,143,off+33),fill='#f5df4f',outline='#b92e1b',width=4)
        elif k==1:
            for i in range(6):
                a=i*math.pi/3
                p=(cx+math.cos(a)*19,cy+math.sin(a)*19)
                d.line((cx,cy,*p),fill=c2,width=5)
                for sign in [-1,1]:
                    ap=a+sign*.75
                    mid=(cx+math.cos(a)*14,cy+math.sin(a)*14)
                    d.line((*mid,mid[0]-math.cos(ap)*8,mid[1]-math.sin(ap)*8),fill=c2,width=3)
        elif k==2:
            pts=[(cx+int(x*10),cy-int(y*3.3)) for x,y in [(0,3),(2,5),(3,5.3),(4,5),(4.7,4),(5,3),(4.8,2),(4,0),(.1,-5),(-.1,-5),(-4,0),(-4.8,2),(-5,3),(-4.7,4),(-4,5),(-3,5.3),(-2,5)]]
            d.polygon(pts,fill=c1,outline='#fff0dd',width=2)
            d.text((119,off+15),'x2',fill=c2,stroke_width=1,stroke_fill='#5a1800')
        elif k==3:
            pts=[(cx+int(math.cos(math.pi/2+i*math.pi/5)*(22 if i%2==0 else 11)),cy-int(math.sin(math.pi/2+i*math.pi/5)*(21 if i%2==0 else 10))) for i in range(10)]
            d.polygon(pts,fill=c1,outline=c2,width=2);d.text((117,off+15),'x2',fill='#ffe14d')
        elif k==4:
            d.polygon([(75,cy+8),(120,cy-16),(160,cy-12),(184,cy+3),(145,cy+6),(125,cy+19)],fill=c1,outline=c2,width=3)
            d.ellipse((95,cy-9,120,cy+13),fill='#e4db52',outline='#da2418',width=3)
        else:
            # Retro arcade low-poly horseshoe: red and blue magnetic poles.
            d.arc((cx-29,cy-21,cx+29,cy+30),0,180,fill='#faf4a0',width=12)
            d.line((cx-29,cy-21,cx-29,cy+4),fill='#df3333',width=12)
            d.line((cx+29,cy-21,cx+29,cy+4),fill='#2d73e4',width=12)
            d.rectangle((cx-35,cy-27,cx-24,cy-12),fill='#ff9393')
            d.rectangle((cx+24,cy-27,cx+35,cy-12),fill='#b1d7ff')
    # TGA RLE-free, loaded by the existing GLES texture loader.
    (root/'textures').mkdir(exist_ok=True)
    im.save(root/'textures'/'BONUSX.tga',format='TGA')
    (out/'README.txt').write_text('AXBM v1: six original-inspired low-poly bonus meshes, local x/y frontal plane.\nTexture: textures/BONUSX.tga 256x384 RGBA.\nNo commercial model data. +2 lives uses traced original heart outline.\n',encoding='utf8')
    print('bonus mesh generated:',len(data),'bytes')
if __name__=='__main__':
    import sys
    generate(Path(sys.argv[1]))
