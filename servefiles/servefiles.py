#!/usr/bin/env python
import os
import socket
import struct
import sys
import threading
import time
import urllib

try:
	from SimpleHTTPServer import SimpleHTTPRequestHandler
	from SocketServer import TCPServer
	from urlparse import urljoin
	from urllib import pathname2url, quote
except ImportError:
	from http.server import SimpleHTTPRequestHandler
	from socketserver import TCPServer
	from urllib.parse import urljoin, quote
	from urllib.request import pathname2url

if len(sys.argv) < 3:
	print("Usage: " + sys.argv[0] + " <ip> <file/directory> [host ip]")
	sys.exit(1)

ip = sys.argv[1]
directory = sys.argv[2]

if not os.path.exists(directory):
	print(directory + ": No such file or directory.")
	sys.exit(1)

if len(sys.argv) >= 4:
	hostIp = sys.argv[3]
else:
	hostIp = [(s.connect(('8.8.8.8', 53)), s.getsockname()[0], s.close()) for s in [socket.socket(socket.AF_INET, socket.SOCK_DGRAM)]][0][1]

print("Preparing data...")

baseUrl = hostIp + ":8080/"
payload = ""

if os.path.isfile(directory):
	payload += baseUrl + quote(os.path.basename(directory))
	directory = os.path.dirname(directory)
else:
	for file in [ file for file in next(os.walk(directory))[2] if file.endswith(('.cia', '.tik')) ]:
		payload += baseUrl + quote(file) + "\n"

if len(payload) == 0:
	print("No files to serve.")
	sys.exit(1)

payloadBytes = payload.encode("ascii")

if not directory == "":
	os.chdir(directory)

print("")
print("URLS:")
print(payload)
print("")

print("Opening HTTP server on port 8080...")

server = TCPServer(("", 8080), SimpleHTTPRequestHandler)
thread = threading.Thread(target=server.serve_forever)
thread.start()

try:
	print("Sending URL(s) to " + ip + ":5000...")

	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	sock.connect((ip, 5000))
	sock.sendall(struct.pack('!L', len(payloadBytes)) + payloadBytes)
	while len(sock.recv(1)) < 1:
		time.sleep(0.05)

	sock.close()
except Exception as e:
	print("Error: " + str(e))
	server.shutdown()
	sys.exit(1)

print("Shutting down HTTP server...")

server.shutdown()
