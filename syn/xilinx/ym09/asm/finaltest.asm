; JT51 Final Test
; Jose Tejada
; 9 octubre 2016

; Use TAB > 4 spaces

MEM_START   EQU $8000
STACK_START EQU MEM_START
RAM_START	EQU	$6000
DP_SIZE     EQU 256
VERSION     EQU $01

YM_A0    	EQU $2000
YM_A1    	EQU $2001

		ORG  MEM_START
RESET:	ORCC #$50					; Disable interrupts
      	LDS	 #STACK_START
		LDA  #$F2
		TFR  A,60
	
		; Key off general
		LDD	#$800
L00:		JSR  YM_WRITE
		INCB
		CMPB #8
		BNE  L00
		; Noise disable
		LDD  #$0F00
		JSR	YM_WRITE
		; Timer disable
		LDD	#$1400
		JSR	YM_WRITE
				
		; Borra los registros superiores	
		LDD	#$2000
L01:		JSR	YM_WRITE
		INCA
		CMPA	#0
		BNE	L01

		; un segundo de gracia
		LDX	#20
		JSR	TIME_WAIT
		
		; Primero prueba de envolventes
		LDD	#$20C7
		JSR	YM_WRITE
		LDD	#$8001	; AR = 1
		JSR	YM_WRITE
		LDD	#$E00F	; RR = F
		JSR	YM_WRITE
		LDD	#$0808	; Key on OP 0
		JSR	YM_WRITE
		LDX	#20
		JSR	TIME_WAIT
		LDD	#$0800	; Key off
		JSR	YM_WRITE
		
		LBRA RESET


TIME_WAIT:; Espera el numero de 50ms que diga X
		PSHS	A,B
		LDD	#$1251
		JSR	YM_WRITE
TWL0:	
		LDD	#$142A
		JSR	YM_WRITE
TWL1:	LDA	YM_A1
		TSTA	#2
		BEQ	TWL1
		LEAX	-1,X
		BNE	TWL0
		POPS	A,B
		RTS

YM_WRITE:	; Escribe A en A0 y B en A1
		PSHS	B
L8003:
		ldb		YM_A1
		andb	#$80
		bne	L8003
		sta		YM_A0
L800D:
		ldb		YM_A1
		andb	#$80
		bne	L800D
		POPS	B
		stb		YM_A1
		rts


TopMem	EQU	$FFF8
		FILL $FF,TopMem-*
		ORG TopMem
		FDB	$FFFF	; $FFF8
		FDB $FFFF, $FFFF
		FDB RESET
