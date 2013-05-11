.abicalls
.globl manyadds
.text
.align 2
.globl manyadds
.set nomips16
.ent manyadds
.type manyadds, @function
manyadds:
	.set noreorder
	.rept 2000000
	addiu	$a0, $a0, 2
	addiu	$a0, $a0, 3
	addiu	$a0, $a0, 5
	addiu	$a0, $a0, 7
	addiu	$a0, $a0, 11
	addiu	$a0, $a0, 13
	addiu	$a0, $a0, 17
	.endr
	jr		$ra
	move	$v0, $a0
.set reorder
.end manyadds
