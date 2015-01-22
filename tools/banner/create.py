from PIL import Image, ImageDraw
from io import BytesIO
from compress import compress_nlz11
from struct import pack
import sys

longtitle=sys.argv[1]
shortitle=sys.argv[2]
publisher=sys.argv[3]

# fixed variables; not likely these will be used in this context, but here just in case.
visibility    =1
autoBoot      =0
use3D         =1
requireEULA   =0
autoSaveOnExit=0
extendedBanner=0
gameRatings   =0
useSaveData   =1
recordAppUsage=0
disableSaveBU =0

def make_icon(file, size):
    f=open(file,'rb')
    image=Image.open(f)
    posit=open('map' + str(size) + 'x' + str(size) + '.bin','rb')

    w=image.size[0]
    h=image.size[1]
    n=w*w
    i=[0]*(n*2)

    if w != h:
        print("width, height unequal")
        f.close()
        sys.exit(1)
    if w != size:
        print("sides need to be " + str(size) + " pixels")
        f.close()
        sys.exit(1)

    dump=list(image.getdata())


    pos=0

    for x in range(n):
        p1=ord(posit.read(1))  
        p2=ord(posit.read(1))
        pos=p1+(p2<<8)

        r=dump[x][0]>>3
        g=dump[x][1]>>2
        b=dump[x][2]>>3

        i[pos<<1]=(g&7)<<5| b
        i[(pos<<1)+1]=(r)<<3 | g>>3

    f.close()
    posit.close()
    return bytearray(i)

def make_banner(file):
    f=open(file,'rb')
    image=Image.open(f)
    if image.size[0] != 256 or image.size[1] != 128:
	    f.close()
	    print("ERROR: Image must be exactly 256 x 128. Abort.")
	    sys.exit(4)

    hdr=open("header.bin","rb")

    posit=open('map256x128.bin','rb')
    n=256*128
    i=[0]*(n*2)
    cbmdhdr=bytearray(b"\x43\x42\x4D\x44\x00\x00\x00\x00\x88"+(b"\x00"*0x7B))

    dump=list(image.getdata())
    pos=0

    for x in range(n):
        p1=ord(posit.read(1))  
        p2=ord(posit.read(1))
        pos=p1+(p2<<8)

        r=dump[x][0]>>4
        g=dump[x][1]>>4
        b=dump[x][2]>>4
        a=dump[x][3]>>4

        i[pos<<1]= (b<<4) | a
        i[(pos<<1)+1]= (r<<4) | g


    buf=hdr.read()
    hdr.close()

    out = BytesIO()
    compress_nlz11(buf+bytearray(i), out)
    l=bytearray(out.getvalue())
    length=len(l)+136

    pad=16-(length%16)
    l+=bytearray([0]*pad)
    length+=pad

    for c in range(4):
	    cbmdhdr+=pack("B", length & 255)
	    length=length>>8

    cbmdhdr+=l

    f.close()
    posit.close()
    return bytearray(cbmdhdr)
  
def make_audio():
    # TODO: Convert WAV file.
    bcwav1=open("audio.bcwav","rb")
    bcwav=bcwav1.read()
    bcwav1.close()
    return bytearray(bcwav)

ctp1=make_icon(sys.argv[4], 24)
ctp2=make_icon(sys.argv[5], 48)
bcwav=make_audio()
cbmd=make_banner(sys.argv[6])

icn=open(sys.argv[7],"wb")
bnr=open(sys.argv[8],"wb")

header=bytearray(b"\x53\x4D\x44\x48\x00\x00\x00\x00")+bytearray(b"\x00"*0x1FF8)+bytearray(b"\x00\x00\x00\x00\x00\x00\x00\x00\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x00\x00\x00\x00\xFF\xFF\xFF\x7F\x31\x48\x62\x64\x99\x99\x99\x19\x91\x18\x62\x64\xA5\x01\x00\x00\x00\x01\x00\x00\x00\x00\x80\x3F\x32\x41\x79\x24\x00\x00\x00\x00\x00\x00\x00\x00")

header[0x2028]=(visibility | autoBoot<<1 | use3D<<2 | requireEULA<<3 | autoSaveOnExit<<4 | extendedBanner<<5 | gameRatings<<6 | useSaveData<<7)
header[0x2029]=(recordAppUsage | disableSaveBU<<2)

offset=8
pos=0

for x in range(11):
        for c in longtitle:
            header[offset+pos*2]=ord(longtitle[pos])
            pos+=1
        pos=0
        offset+=0x80
        for c in shortitle:
            header[offset+pos*2]=ord(shortitle[pos])
            pos+=1
        pos=0
        offset+=0x100
        for c in publisher:
            header[offset+pos*2]=ord(publisher[pos])
            pos+=1
        pos=0
        offset+=0x80

header+=(ctp1+ctp2)

icn.write(header)
bnr.write(cbmd+bcwav)

bnr.close()
icn.close()