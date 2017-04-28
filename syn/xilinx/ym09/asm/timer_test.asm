; IS THIS CODE RESETTING THE CPU????
;

MEM_START   EQU $F000
STACK_START EQU MEM_START+256
DP_START    EQU STACK_START+512
LED_ADDR	  EQU	$810
UART_DATA   EQU $800
UART_STATUS EQU $801

YMCTRL      EQU $A00
; cs_n, 0,0,0, // 0,0, rd_n, wr_n
YMSIGNALS   EQU $A01
YMDATA      EQU $A02
YMPM        EQU $A03
YMICN       EQU $A04
YMA0        EQU $A05
YMPM_TGL    EQU $A07  ; any write access will toggle ym_pm and increase the YM PM counter
YMCNT       EQU $A10
YMCNT_LSB   EQU $A12

; YM Registers
YM_CLKA1    EQU $10
YM_CLKA2    EQU $11
YM_CLKB     EQU $12
YM_CLKCTRL  EQU $14

			ORG  MEM_START
			FILL $A5,1024
RESET:ORCC #$50					; Disable interrupts
      LDS	 #STACK_START
			LDA  #$F2
			TFR  A,DP      
			CLR  LED_ADDR
			JSR  RESET_YM
			LDA	 #$FF
			JSR	 RUN_PM
			ANDCC #$EF				; Enable IRQ
			
			; WAIT FOR TIMER DATA FROM PC
MAIN:	LDA  #1
			STA  LED_ADDR			
WDATA:LDA  <0
			BEQ  WDATA			

			; SET TIMER B
			LDA  #$10
			STA  LED_ADDR			
			CLR  YMA0
			LDA  #YM_CLKB
			JSR  WRITE_YM
			INC  YMA0
			LDA  <0
			JSR  WRITE_YM
			CLR  <0

			; START TIMER
			LDA  #$20
			STA  LED_ADDR			
			CLR  YMA0
			LDA  #YM_CLKCTRL
			JSR  WRITE_YM
			INC  YMA0
			LDA  #$2A
			JSR  WRITE_YM

			; WAIT FOR TIMER INTERRUPT      
			LDA  #$40
			STA  LED_ADDR			
			JSR  CLR_PM_CNT			
			JSR  WAIT_YM_IRQ

			; SEND THE PM COUNTER TO THE PC
			LDA  #$80
			STA  LED_ADDR			
      JSR  SEND_YMCNT

			; RESET THE TIMER FLAG
			LDA  #$81
			STA  LED_ADDR			
			CLR  YMA0
			LDA  #YM_CLKCTRL
			JSR  WRITE_YM
			INC  YMA0
			LDA  #$24
			JSR  WRITE_YM
      CLR	 YMPM_TGL
      CLR	 YMPM_TGL      
      
LOOP:	CLR	 YMPM_TGL
			BRA  MAIN

; ********** send the counter data in intel order
SEND_YMCNT: 
      LDX  #YMCNT
      LDB  #3
SNDL: CLR  UART_STATUS
			LDA  B,X
      STA  UART_DATA
SNDW: LDA  UART_STATUS
      ANDA #2
      BEQ  SNDW
      DECB
      BPL  SNDL
      RTS
      

WAIT_YM_IRQ:
      LDA  YMSIGNALS
      ANDA #$80
      BEQ  IRQ_DONE
      CLR  YMPM_TGL
      BRA  WAIT_YM_IRQ
IRQ_DONE:		
      RTS

CLR_PM_CNT:
      CLR YMCNT
      CLR YMCNT+1
      CLR YMCNT+2
      CLR YMCNT+3
      RTS      

WRITE_YM:
			; PSHS A,B
			; WRITE ADDRESS
			STA  YMDATA
			LDA  #$FF
			STA  YMCTRL
			LDA  #2
LWYM1:DECA
			BNE  LWYM1
			LDA  #$0F	;CS=0
			STA  YMCTRL
			LDA	 #2
LWYM2:DECA
      BNE  LWYM2
      LDA  #$2  ; rd_n=1, other=0
      STA  YMCTRL
      LDA  #2
LWYM3:CLR  YMPM_TGL
			DECA
      BNE  LWYM3
      LDA  #$FF
      STA  YMCTRL
      NOP
      NOP
      RTS

RUN_PM:			
      CLR  YMPM_TGL
      DECA
      BNE  RUN_PM
      RTS

RESET_YM:
			LDA		#8
			STA		YMCTRL
			CLRA
			STA		YMICN
L30:	CLR 	YMPM_TGL
			DECA
			BNE 	L30
			LDA		#1
			STA		YMICN
			RTS
			
IRQ_SER:
			LDA  #$1f
			STA  LED_ADDR			
			LDA	 UART_DATA
			STA  <0
			CLR  UART_STATUS	
			RTI


	
TopMem	EQU	$FFF8
				FILL $FF,TopMem-*
				ORG TopMem
				FDB	IRQ_SER	; $FFF8
				FDB $FFFF, $FFFF
				FDB RESET
