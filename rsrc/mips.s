main: # comment
addiu $sp, $sp, -16
sw    $fp, 12($sp)
move  $fp, $sp
sw    $zero, 8($fp)
sw    $4, 4($fp)
sw    $5, 0($fp)
lw    $4, 4($fp)
addiu $2, $4, 42
move  $sp, $fp
lw    $fp, 12($sp)
addiu $sp, $sp, 16
jr    $ra
nop

j   main
