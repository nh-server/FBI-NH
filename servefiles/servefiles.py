import atexit
import os
import sys
import tempfile
import threading
import urllib

import netifaces
import qrcode

from PIL import ImageTk

try:
	from SimpleHTTPServer import SimpleHTTPRequestHandler
	from SocketServer import TCPServer
	from Tkinter import Tk, Frame, Label, BitmapImage
	from urlparse import urljoin
	from urllib import pathname2url, quote
except ImportError:
	from http.server import SimpleHTTPRequestHandler
	from socketserver import TCPServer
	from tkinter import Tk, Frame, Label, BitmapImage
	from urllib.parse import urljoin, quote
	from urllib.request import pathname2url

if len(sys.argv) < 2:
	print("Please specify a file/directory.")
	sys.exit(1)

directory = sys.argv[1]

if not os.path.exists(directory):
	print(directory + ": No such file or directory.")
	sys.exit(1)

print("Preparing data...")

baseUrl = netifaces.ifaddresses(netifaces.gateways()['default'][netifaces.AF_INET][1])[2][0]['addr'] + ":8080/"
qrData = ""

if os.path.isfile(directory):
	if directory.endswith(('.cia', '.tik')):
		qrData += baseUrl + quote(os.path.basename(directory))

	directory = os.path.dirname(directory)
else:
	for file in [ file for file in next(os.walk(directory))[2] if file.endswith(('.cia', '.tik')) ]:
		qrData += baseUrl + quote(file) + "\n"

if len(qrData) == 0:
	print("No files to serve.")
	sys.exit(1)

if not directory == "":
	os.chdir(directory)

print("")
print("URLS:")
print(qrData)
print("")

print("Opening HTTP server on port 8080...")

server = TCPServer(("", 8080), SimpleHTTPRequestHandler)
thread = threading.Thread(target=server.serve_forever)
thread.start()
atexit.register(server.shutdown)

print("Displaying QR code...")

qrImage = qrcode.make(qrData)

root = Tk()
root.title("QR Code")

frame = Frame(root)
frame.pack()

qrBitmap = ImageTk.PhotoImage(qrImage)
qrLabel = Label(frame, image=qrBitmap)
qrLabel.pack()

root.mainloop()

server.shutdown()
