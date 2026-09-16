import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_TCP)
sock.connect(("pinkysmile.fr", 5254))
sock.send(b'\x01\x00\x00')
while True:
	data = sock.recv(6)
	if data[0] == 0 and data[1] == 0:
		break
	print(f'{data[2]}.{data[3]}.{data[4]}.{data[5]}:{data[0] | (data[1] << 8)}')
