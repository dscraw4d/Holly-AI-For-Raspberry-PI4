"""Actual Holly TCP carries fragmented guest protocols on ports 23 and 80."""
import ctypes as C, struct
from pathlib import Path
lib=C.CDLL(str(Path(__file__).resolve().parents[1]/'build/guest_tcp_adapter.so'))
MAC=bytes([2,1,2,3,4,5]);PEER=bytes([2,7,8,9,10,11]);IP=bytes([169,254,77,1]);REMOTE=bytes([169,254,77,2])
def checksum(p):
 if len(p)%2:p+=b'\0'
 v=sum(struct.unpack('>'+str(len(p)//2)+'H',p))
 while v>>16:v=(v&65535)+(v>>16)
 return ~v&65535
def packet(port,seq,ack,flags,data=b''):
 tcp=bytearray(struct.pack('>HHIIBBHHH',50000,port,seq,ack,80,flags,32768,0,0)+data)
 tcp[16:18]=struct.pack('>H',checksum(REMOTE+IP+b'\0\6'+struct.pack('>H',len(tcp))+tcp))
 ip=bytearray(struct.pack('>BBHHHBBH4s4s',69,0,20+len(tcp),0,16384,64,6,0,REMOTE,IP));ip[10:12]=struct.pack('>H',checksum(ip))
 return MAC+PEER+b'\x08\x00'+ip+tcp
def feed(p):assert lib.test_tcp_feed(C.create_string_buffer(p),len(p))==0
def pop():
 b=(C.c_ubyte*1514)();n=lib.test_tcp_pop(b);return bytes(b[:n])
for port,path in [(23,"/"),(80,"/"),(80,"/holly-face.jpg")]:
 lib.test_guest_start(port);seq=100;ack=0
 feed(packet(port,seq,ack,2));syn=pop();assert struct.unpack_from('>H',syn,34)[0]==port
 seq+=1;ack=struct.unpack_from('>I',syn,38)[0]+1;feed(packet(port,seq,ack,16))
 output=bytearray();closed=False
 def drain():
  global ack,closed
  for _ in range(1000):
   f=pop()
   if not f:return
   t=f[34:];assert struct.unpack_from('>H',t)[0]==port
   assert checksum(IP+REMOTE+b'\0\6'+struct.pack('>H',len(t))+t)==0
   data=t[(t[12]>>4)*4:];flags=t[13]
   if data:
    assert struct.unpack_from('>I',t,4)[0]==ack
    output.extend(data);ack+=len(data);feed(packet(port,seq,ack,16))
   if flags&1:
    ack+=1;feed(packet(port,seq,ack,17));closed=True
  raise AssertionError('queue did not drain')
 drain()
 payload=b'dwarf Lister\r\ntrain start 8\r\nexit\r\n' if port==23 else ('GET '+path+' HTTP/1.1\r\nHost: 169.254.77.1\r\n\r\n').encode()
 for start in range(0,len(payload),7):
  data=payload[start:start+7];p=packet(port,seq,ack,24,data);feed(p);feed(p);seq+=len(data);drain()
 assert closed
 if port==23:assert b'stasis' in output and b'SSH' in output and output.count(b'stasis')==1
 elif path=='/':assert b'HTTP/1.1 200' in output and b'</html>' in output
 else:
  header,body=bytes(output).split(b'\r\n\r\n',1)
  assert b'Content-Type: image/jpeg' in header
  expected=(Path(__file__).resolve().parents[1]/'web/holly-face.jpg').read_bytes()
  assert body==expected and b'Content-Length: '+str(len(body)).encode() in header
print('Ports 23/80: TCP handshake, checksums, fragmented requests, duplicate suppression, response drain, complete seven-frame portrait atlas and FIN passed')
