import socket

from md5_generator import generate_md5

from subprocess import Popen, PIPE, STDOUT


with open('binary.raw', 'rb') as inp:
    firmware = inp.read()
ADDR = ("flagrom.ctfcompetition.com", 1337)
REMOTE = True# False
FIRMWARE_LENGTH = len(firmware)

class FakePipe:
    def __init__(self, addr):
        self.sock = socket.socket()
        self.sock.connect(addr)

    def read(self, count):
        return self.sock.recv(count)

    def write(self, data):
        self.sock.send(data)

    @property
    def stdout(self):
        return self

    @property
    def stdin(self):
        return self

def read_until(fd, data=b'\n'):
    more_data = []
    read_byte = None
    while read_byte != data:
        if read_byte:
            print(read_byte)
        read_byte = fd.read(1)
        more_data.append(read_byte)
    return b''.join(more_data)

print("[*] Initiating interaction...")
if not REMOTE:
    target = Popen('./flagrom', stdout=PIPE, stdin=PIPE, stderr=STDOUT, bufsize=0)
else:
    target = FakePipe(ADDR)
data = read_until(target.stdout)
print(data)
target_hash = data.split()[-1].split(b'?')[0].strip().decode()
print("[*] Generating requested hash({})...".format(target_hash))
answer = generate_md5(target_hash)
print("[+] Done!")
target.stdin.write((answer + '\n').encode())
print('[*] Written answer...')

data = read_until(target.stdout)
if b'?' in data:
    print("[+] Hash accepted!")
print(data)
target.stdin.write((str(FIRMWARE_LENGTH) + '\n').encode())
target.stdin.write(firmware)

for _ in range(7):
    data = read_until(target.stdout)
    print(data)

flag = []
for idx in range(len(data) // 8): 
    flag.append(chr(sum(1 << (7 - jdx)
                        if each == 1 else 0
                        for (jdx, each) in enumerate(data[idx*8:(idx+1)*8]))))

print("found flag : {}".format(''.join(flag)))


