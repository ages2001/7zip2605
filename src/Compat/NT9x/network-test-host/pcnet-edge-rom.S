.code16
.global _start
_start:
.byte 0x55,0xaa,1
pushal
pushfl
movw $0xcf8,%dx
movl $0x80001804,%eax
outl %eax,%dx
movw $0xcfc,%dx
inw %dx,%ax
orw $4,%ax
outw %ax,%dx
movw $0x4d1,%dx
inb %dx,%al
andb $0xf7,%al
outb %al,%dx
popfl
popal
lret
.org 511
.byte 0
