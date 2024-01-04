; a test program
macro sub v1 v2
subleq v1 v2 next
next: endm

macro jmp label
subleq zero, zero, label
endm

macro mov to, from
sub mov_tmp, mov_tmp
sub mov_tmp, from
sub to, to
sub to, mov_tmp
endm

macro neg label
sub neg_tmp, neg_tmp
sub neg_tmp, label
mov label, neg_tmp
endm

subleq ax ax next
mov ax ax
next:
subleq begin zero zero 
subleq -100300 1 2
ax: dw 0
mov_tmp: dw 0
zero: dw 0
neg_tmp: dw zero
dw 'hello world',0

org 1920
begin: jmp begin ;endless loop
 
