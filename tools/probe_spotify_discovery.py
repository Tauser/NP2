"""Bounded read-only LAN probe; prints only NovaPanel service availability."""
import argparse
import ipaddress
import json
import socket
import struct
import time
import urllib.request

SERVICE='_spotify-connect._tcp.local'

def name_at(packet,offset):
    labels=[];end=None;seen=set()
    for _ in range(64):
        if offset in seen or offset>=len(packet):raise ValueError()
        seen.add(offset);size=packet[offset];offset+=1
        if size&0xc0==0xc0:
            if offset>=len(packet):raise ValueError()
            if end is None:end=offset+1
            offset=((size&63)<<8)|packet[offset];continue
        if size&0xc0 or size>63 or offset+size>len(packet):raise ValueError()
        if not size:return '.'.join(labels).lower(),end or offset
        labels.append(packet[offset:offset+size].decode('utf-8'));offset+=size
    raise ValueError()

def records(packet):
    if len(packet)<12:raise ValueError()
    _,_,questions,answers,authorities,additional=struct.unpack('!6H',packet[:12])
    if questions+answers+authorities+additional>256:raise ValueError()
    offset=12
    for _ in range(questions):_,offset=name_at(packet,offset);offset+=4
    result=[]
    for _ in range(answers+authorities+additional):
        name,offset=name_at(packet,offset)
        if offset+10>len(packet):raise ValueError()
        kind,_,ttl,size=struct.unpack('!HHIH',packet[offset:offset+10]);offset+=10
        if offset+size>len(packet):raise ValueError()
        value=None
        if kind==12:value=name_at(packet,offset)[0]
        elif kind==33 and size>=7:value=(struct.unpack('!H',packet[offset+4:offset+6])[0],name_at(packet,offset+6)[0])
        elif kind==1 and size==4:value=socket.inet_ntoa(packet[offset:offset+4])
        if value is not None and ttl:result.append((name,kind,value))
        offset+=size
    return result

def query(connection,name,kind):
    wire=b''.join(bytes([len(part)])+part.encode() for part in name.split('.'))+b'\0'
    connection.sendto(struct.pack('!6H',0,0,1,0,0,0)+wire+struct.pack('!HH',kind,0x8001),('224.0.0.251',5353))

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--seconds',type=int,default=300,choices=range(1,601))
    args=parser.parse_args();services={};addresses={};instances=set();next_query=0
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as connection:
        connection.bind(('',0));connection.settimeout(.25);connection.setsockopt(socket.IPPROTO_IP,socket.IP_MULTICAST_TTL,1)
        deadline=time.monotonic()+args.seconds
        print('Waiting for NovaPanel Spotify LAN advertisement (read-only).',flush=True)
        while time.monotonic()<deadline:
            if time.monotonic()>=next_query:
                query(connection,SERVICE,12)
                for instance in instances:query(connection,instance,33)
                for _,host in services.values():query(connection,host,1)
                next_query=time.monotonic()+2
            try:
                packet,_=connection.recvfrom(16384)
                for name,kind,value in records(packet):
                    if kind==12 and name==SERVICE and value.startswith('novapanel teste '):instances.add(value)
                    elif kind==33 and name.startswith('novapanel teste ') and name.endswith('.'+SERVICE):
                        instances.add(name);services[name]=value
                    elif kind==1:addresses[name]=value
            except (socket.timeout,ValueError,UnicodeError,struct.error):continue
            for instance in instances:
                port,host=services.get(instance,(0,''));address=addresses.get(host)
                if port!=8080 or not address or not host.endswith('.local'):continue
                ip=ipaddress.ip_address(address)
                if not ip.is_private or ip.is_loopback or ip.is_multicast:continue
                print('NovaPanel mDNS PTR/SRV/A resolved; port=8080.',flush=True)
                try:
                    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
                    with opener.open(f'http://{address}:8080/spotify_info',timeout=3) as response:
                        body=response.read(4097)
                        valid=len(body)<=4096 and response.status==200
                    info=json.loads(body) if valid else {}
                    valid=valid and str(info.get('remoteName','')).startswith('NovaPanel teste ')
                    print('NovaPanel discovery HTTP info: '+('PASS' if valid else 'FAIL')+' (private fields omitted).',flush=True)
                    return 0 if valid else 2
                except (OSError,ValueError):
                    print('NovaPanel discovery HTTP info unavailable; details omitted.',flush=True);return 2
    print('No NovaPanel Spotify advertisement observed within probe window.',flush=True);return 2

if __name__=='__main__':raise SystemExit(main())
