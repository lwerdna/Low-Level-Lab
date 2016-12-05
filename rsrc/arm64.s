main: // comment
sub sp, sp, #16
str wzr, [sp, #12]
str w0, [sp, #8]
str  x1, [sp]
ldr w0, [sp, #8]
add w0, w0, #42
add sp, sp, #16
ret

b   main
