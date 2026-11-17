"""MakerWorld cover images from the two real photos in photos/. Needs pillow, numpy, rembg (pip install \"rembg[cpu]\") for the cut-outs
the first time (cut-outs are cached in photos/cutout_*.png). Writes the four LyricFlash_cover_*.jpg next to this file."""
from PIL import Image, ImageFilter, ImageChops
import numpy as np
import pathlib
HERE=pathlib.Path(__file__).resolve().parent
I=str(HERE/'photos')+'/'
OUT=str(HERE)+'/'

def crop_ratio(im, cx, cy, ratio, ):
    W,H=im.size
    # largest crop of given ratio (w/h) that fits, centred near (cx,cy)
    h=H; w=h*ratio
    if w>W: w=W; h=w/ratio
    x0=min(max(cx-w/2,0),W-w); y0=min(max(cy-h/2,0),H-h)
    return im.crop((round(x0),round(y0),round(x0+w),round(y0+h)))
def photo(n,cx,cy,ratio,size,name):
    im=Image.open(I+{'1':'device_landscape','2':'device_portrait'}[n]+'.jpg').convert('RGB')
    c=crop_ratio(im,cx,cy,ratio).resize(size,Image.LANCZOS)
    c.save(OUT+name,quality=95); print(name,c.size)
def white(n,ratio_size,frac_h,frac_w,name,dy=0.0):
    cut=Image.open(HERE/'photos'/f'cutout_{n}.png').convert('RGBA')
    bb=cut.getbbox(); cut=cut.crop(bb)
    a=cut.getchannel('A').filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.8))   # trim the 1 px halo, soften the edge
    cut.putalpha(a)
    W,H=ratio_size
    s=min(H*frac_h/cut.height, W*frac_w/cut.width)
    cut=cut.resize((round(cut.width*s),round(cut.height*s)),Image.LANCZOS)
    canvas=Image.new('RGBA',(W,H),(255,255,255,255))
    x=(W-cut.width)//2; y=(H-cut.height)//2+round(dy*H)
    # soft contact shadow under the device (offset down-right, blurred)
    sh=Image.new('L',(W,H),0); m=cut.getchannel('A').point(lambda v:int(v*0.35)); sh.paste(m,(x+round(W*0.012),y+round(H*0.02)))
    sh=sh.filter(ImageFilter.GaussianBlur(W*0.014))
    shadow=Image.new('RGBA',(W,H),(40,40,40,255)); shadow.putalpha(sh)
    canvas.alpha_composite(shadow); canvas.alpha_composite(cut,(x,y))
    canvas.convert('RGB').save(OUT+name,quality=95); print(name,canvas.size,'device',cut.size)
def ensure_cutouts():
    from rembg import remove, new_session
    sess = None
    for n, f in (('1', 'device_landscape'), ('2', 'device_portrait')):
        out = HERE / 'photos' / f'cutout_{n}.png'
        if not out.exists():
            sess = sess or new_session('u2net')
            remove(Image.open(HERE / 'photos' / f'{f}.jpg').convert('RGB'), session=sess).save(out)


ensure_cutouts()
photo('1',1206,560,4/3,(1600,1200),'LyricFlash_cover_4x3_photo.jpg')
photo('2',650,953,3/4,(1200,1600),'LyricFlash_cover_3x4_photo.jpg')
white('1',(1600,1200),0.82,0.8,'LyricFlash_cover_4x3_white.jpg')
white('2',(1200,1600),0.80,0.80,'LyricFlash_cover_3x4_white.jpg')
