; a test program
subleq ax ax next
sub ax ax
next:
subleq begin zero zero 
subleq -100300 1 2
ax: dw 0
bx: dw 0
zero: dw 0
pzero: dw zero
dw 'hello world',0

org 1920
begin: subleq zero, zero, begin ;endless loop
 
