.section .data
# load the delay counter 
L1: .word 3500000

.section .text

.global main

main:
lui x4, 0x10012
la x3, L1
lw x11, 0(x3)	
addi x5, x4, 4
addi x6, x4, 8 

# LED address as 0x1001200C
addi x7, x4, 12

addi x10, x0, 0x20
sw x0, 0(x5)
sw x10, 0(x6)

#counter
addi x1, x0, 1

#the infinite loop for blinking

WHILE: bne x1, x0, GLOW
	sw x0, 0(x7)
	jal x21, LAG
	addi x1, x0, 1
	beq x0, x0, WHILE
	
	# the glow logic
	GLOW:
	sw x10, 0(x7)
	jal x21, LAG
	addi x1, x0, 0
	beq x0, x0, WHILE
		
#delay logic

LAG: beq x11, x0, EXIT
	addi x11, x11, -1
	beq x0, x0, LAG

#exit delay to run again
EXIT:	lw x11, 0(x3)
	jalr x0, x21, 0

Lwhile1: j Lwhile1
