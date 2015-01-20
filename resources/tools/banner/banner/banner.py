from PIL import Image, ImageDraw
import sys,os,platform

f=open('banner.png','rb')
image=Image.open(f)
if image.size[0] != 256 or image.size[1] != 128:
	f.close()
	print "ERROR: Image must be exactly 256 x 128. Abort."
	sys.exit(4)

icon=open('banner.cgfx','wb')
hdr=open("header.bin","rb")

posit=open('map256x128.bin','rb')
'''
chx=[0,1,4,5,16,17,20,21]
chy=[0,2,8,10,32,34,40,42]
w=image.size[0]
h=image.size[1]
'''
n=256*128
i=[0]*(n*2)
cbmdhdr="\x43\x42\x4D\x44\x00\x00\x00\x00\x88"+("\x00"*0x7B)

dump=list(image.getdata())
pos=0

for x in range(n):
	#xx=x%w
	#yy=x/w
	#print xx,yy
	#pos=(chx[x%8]+chy[(x>>3)%8])+((x>>6)<<6)  fail (i'm not a math major :p)
	#print pos

	p1=ord(posit.read(1))  
	p2=ord(posit.read(1))
	pos=p1+(p2<<8)
	#print p1,p2,pos
	r=dump[x][0]>>4
	g=dump[x][1]>>4
	b=dump[x][2]>>4
	a=dump[x][3]>>4

	i[pos<<1]= (b<<4) | a
	i[(pos<<1)+1]= (r<<4) | g


buf=hdr.read()
hdr.close()
	
for byte in buf:
	icon.write((byte))	
for byte in i:
    icon.write(chr(byte))

icon.close()
	
if platform.system() == "Windows":
	os.system("DSDecmp.exe -c lz11 banner.cgfx compressed.cgfx")
else:
	os.system("wine DSDecmp.exe -c lz11 banner.cgfx compressed.cgfx")

ccgfx=open("compressed.cgfx",'rb')
l=ccgfx.read()
len=len(l)+136

pad=16-(len%16)
l+=("\x00"*pad)
len+=pad

cbmdlen=['']*4
for c in range(4):
	cbmdlen[c]=chr(len&255)
	len=len>>8

for i in cbmdlen:
	cbmdhdr+=i

cbmd=open("banner.cbmd","wb")
cbmdfinal=l
cbmdhdr+=cbmdfinal
cbmd.write(cbmdhdr)
cbmd.close()

print "Done."
f.close()
posit.close()
cbmd.close()
ccgfx.close()
sys.exit(0)








