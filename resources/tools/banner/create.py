import os,sys
execfile("../../AppData.txt")

icn=open("icon.icn","wb")
bnr=open("banner.bnr","wb")

cptk1=open("icon24/icon24.ctpk","rb")
cptk2=open("icon48/icon48.ctpk","rb")
bcwav1=open("audio/audio.bcwav","rb")
cbmd1=open("banner/banner.cbmd","rb")

ctp1=cptk1.read()
ctp2=cptk2.read()
bcwav=bcwav1.read()
cbmd=cbmd1.read()

header=list("\x53\x4D\x44\x48\x00\x00\x00\x00")
header+=("\x00"*0x1FF8)
header+="\x00\x00\x00\x00\x00\x00\x00\x00\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x00\x00\x00\x00\xFF\xFF\xFF\x7F\x31\x48\x62\x64\x99\x99\x99\x19\x91\x18\x62\x64\xA5\x01\x00\x00\x00\x01\x00\x00\x00\x00\x80\x3F\x32\x41\x79\x24\x00\x00\x00\x00\x00\x00\x00\x00"

header[0x2028]=chr(visibility | autoBoot<<1 | use3D<<2 | requireEULA<<3 | autoSaveOnExit<<4 | extendedBanner<<5 | gameRatings<<6 | useSaveData<<7)
header[0x2029]=chr(recordAppUsage | disableSaveBU<<2)

offset=8
pos=0

for x in range(11):
	for c in longtitle:
		header[offset+pos*2]=longtitle[pos]
		pos+=1
	pos=0
	offset+=0x80
	for c in shortitle:
		header[offset+pos*2]=shortitle[pos]
		pos+=1
	pos=0
	offset+=0x100
	
	for c in publisher:
		header[offset+pos*2]=publisher[pos]
		pos+=1
	pos=0
	offset+=0x80

header=''.join(header)
header+=(ctp1+ctp2)

icn.write(header)
bnr.write(cbmd+bcwav)

print "banner.bnr built."
print "icon.icn built."
print "Done."
bnr.close()
icn.close()
cptk1.close()
cptk2.close()
bcwav1.close()
cbmd1.close()