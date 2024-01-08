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

macro add to, from
sub mov_tmp, mov_tmp
sub mov_tmp, from
sub to, mov_tmp
endm

macro neg label
sub neg_tmp, neg_tmp
sub neg_tmp, label
mov label, neg_tmp
endm

macro ptrjmp ptr
mov code_ptr+26 ptr
jmp code_ptr
endm

macro movpm ptr val
add code_movpm1, ptr
add code_movpm1+26, ptr
add code_movpm2, ptr
sub mov_tmp, mov_tmp
sub mov_tmp val
code_movpm1: subleq 0 0 code_movpm2
code_movpm2: subleq 0 mov_tmp next1
next1: sub code_movpm1, ptr
sub code_movpm1+26, ptr
sub code_movpm2, ptr
endm

macro addpm ptr val
add code_movpm2, ptr
sub mov_tmp, mov_tmp
sub mov_tmp val
code_movpm2: subleq 0 mov_tmp next1
next1: 
sub code_movpm2, ptr
endm

;-------------------
subleq zero zero code_entrypoint
;-------------------
code_ptr: subleq 0 0 0
dw 0
;--
zero: dw 0
mov_tmp: dw 0
neg_tmp: dw zero
;-------------------
code_entrypoint:

mov screen_1 p
mov ptr ptr_begin
mov y max_y
yloop: 

mov x max_x
xloop:

addpm ptr p
;movpm ptr p
sub ptr m52

subleq x p1 xendl
jmp xloop
xendl:

subleq y p1 yendl
jmp yloop
yendl:

jmp code_entrypoint

x: dw 0
y: dw 0
;p: dw 1501199875790165
p: dw 1
m1: dw -1
m52: dw -52
p1: dw 1
ptr_begin: dw screen_1
ptr: dw screen_1
max_y: dw 936
max_x: dw 18

org 876096
screen_1: dw 0
org 1752192
screen_2: dw 0
