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
			FILL $00,1024
RESET:ORCC #$50					; Disable interrupts
      LDS	 #STACK_START
			LDA  #$F2
			TFR  A,DP      
			ANDCC #$EF				; Enable IRQ
						
			CLR  LED_ADDR
WAIT: 
			LDA  <0
			STA  LED_ADDR
			BRA  WAIT

			
			
IRQ_SER:
			LDA UART_DATA
			STA <0
			CLR UART_STATUS
			RTI


	
TopMem	EQU	$FFF8
				FILL $FF,TopMem-*
				ORG TopMem
				FDB	IRQ_SER	; $FFF8
				FDB $FFFF, $FFFF
				FDB RESET
