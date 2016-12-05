main: // comment
sub sp, sp, #20
mov r2, r1
mov r3, r0
mov r12, #0
str r12, [sp, #16]
str r0, [sp, #12]
str r1, [sp, #8]
ldr r0, [sp, #12]
add r0, r0, #42
str r2, [sp, #4]
str r3, [sp]
add sp, sp, #20
bx      lr

loop:
b   loop
