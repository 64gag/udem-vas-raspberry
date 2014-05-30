import socket
import sys
import struct
import select

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Bind the socket to the port
server_address = ('192.168.1.1', 4444)
sock.bind(server_address)

# Listen for incoming connections

sock.listen(1)
while True:
	
	connection, client_address = sock.accept()
	try:
		while True:
			data = connection.recv(8)
			data = struct.unpack("!q", data)
			dataValue = data[0]
			print(dataValue)
	finally:
		connection.close()
