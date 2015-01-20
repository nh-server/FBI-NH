from PIL import Image, ImageDraw
import sys,os

icon=open('icon24.ctpk','wb')

f=open('icon24.png','rb')
image=Image.open(f)
posit=open('map24x24.bin','rb')

chx=[0,1,4,5,16,17,20,21]
chy=[0,2,8,10,32,34,40,42]
w=image.size[0]
h=image.size[1]
n=w*w
i=[0]*(n*2)

if w != h:
    print "width, height unequal"
    f.close()
    icon.close()
    sys.exit(1)
if w != 24:
    print "sides need to be 24 pixels"
    f.close()
    icon.close()
    sys.exit(1)

dump=list(image.getdata())


pos=0

for x in xrange(n):
    #xx=x%w
    #yy=x/w
    #print xx,yy
    #pos=(chx[x%8]+chy[(x>>3)%8])+((x>>6)<<6) 
    #print pos

    p1=ord(posit.read(1)) 
    p2=ord(posit.read(1))
    pos=p1+(p2<<8)
    #print p1,p2,pos
    r=dump[x][0]>>3
    g=dump[x][1]>>2
    b=dump[x][2]>>3

    i[pos<<1]=(g&7)<<5| b
    i[(pos<<1)+1]=(r)<<3 | g>>3

for byte in i:
    icon.write(chr(byte))
print "Done."
icon.close()
f.close()
posit.close()

exit()







