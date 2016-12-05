main: # comment
mr    5, 3
li    6, 0
stw   6, -12(1)
stw   5, -16(1)
std   4, -24(1)
lwz   5, -16(1)
addi  5, 5, 42
extsw 3, 5
blr

b     main
