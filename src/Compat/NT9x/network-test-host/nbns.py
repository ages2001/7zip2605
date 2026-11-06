"""Private NBNS responder for the isolated NT 3.1 SMB test host."""
import socket, struct
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
s.bind(('127.0.0.1',137))
while True:
 data,peer=s.recvfrom(1024)
 if len(data)<50 or data[12]!=32 or data[45]!=0 or data[46:50]!=b'\x00\x20\x00\x01':continue
 try:name=bytes(((data[13+i]-65)<<4)|(data[14+i]-65) for i in range(0,32,2))
 except ValueError:continue
 if name[:15].rstrip()!=b'Z7HOST':continue
 answer=data[:2]+struct.pack('!HHHHH',0x8500,0,1,0,0)+data[12:46]+struct.pack('!HHIH',0x20,1,300,6)+b'\x00\x00'+socket.inet_aton('10.0.2.2')
 s.sendto(answer,peer)
 print('Answered Z7HOST name query',flush=True)
