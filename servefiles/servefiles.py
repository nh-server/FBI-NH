import atexit
import os
import sys
import tempfile
import urllib
import webbrowser

import http.server
import socketserver

import netifaces
import qrcode

try:
	from urlparse import urljoin
	from urllib import pathname2url
except ImportError:
	from urllib.parse import urljoin
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
		qrData += baseUrl + urllib.parse.quote(os.path.basename(directory))

	directory = os.path.dirname(directory)
else:
	for file in [ file for file in next(os.walk(directory))[2] if file.endswith(('.cia', '.tik')) ]:
		qrData += baseUrl + urllib.parse.quote(file) + "\n"

if len(qrData) == 0:
	print("No files to serve.")
	sys.exit(1)

if not directory == "":
	os.chdir(directory)

print("")
print("URLS:")
print(qrData)
print("")

print("Creating QR code...")

qrFile = tempfile.mkstemp(suffix=".png")[1]
atexit.register(os.remove, qrFile)

qrImage = qrcode.make(qrData)
qrImage.save(qrFile)

webbrowser.open_new_tab(urljoin('file:', pathname2url(qrFile)))

print("Listening on port 8080...")

httpd = socketserver.TCPServer(("", 8080), http.server.SimpleHTTPRequestHandler)
httpd.serve_forever()
