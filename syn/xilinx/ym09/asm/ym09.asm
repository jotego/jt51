; YM2151 controlador remoto
; Jose Tejada
; 5 junio 2016
; Comandos


CMD_SIGNALS EQU $01	; enviar señales
CMD_WR_LED  EQU $03 ; Escribe en el LED - 1 dato
CMD_RD_SO   EQU $04 ; Lee X muestras del canal izq. y X del derecho - 1 dato
CMD_RST_CNT EQU $05 ; Borra la cuenta del PM
CMD_WR_ICN  EQU $06 ; Controla la línea de reset del YM - 1 dato
CMD_WAIT_ST EQU $07 ; Lee el estado del YM hasta que esté libre
CMD_WR_REG  EQU $08 ; Escribe con A0=0 - 1 dato
CMD_WR_DATA EQU $09 ; Escribe con A0=1 - 1 dato
CMD_RD_DATA EQU $0A ; Lee el bus de datos del YM con A0=0
CMD_RD_CNT  EQU $0B ; Envía la cuenta del PM
CMD_WAITIRQ EQU $0D ; Conmuta PM hasta que YM_IRQ baje.  volver a
										;      enviar para interrumpir la espera
CMD_RX_TST  EQU $0E ; Recibe del 0 al FF por la UART. Si es
										;      correcto contesta 0 si no, 1

MEM_START   EQU $F000
SO_BUFFER   EQU $8000
;SO_BUFSIZE  EQU $7800 ; 30kB
SO_BUFSIZE  EQU 1024*16
STACK_START EQU MEM_START+2048+256 ; 2kB para pila, DP y código
DP_START    EQU STACK_START
DP_SIZE     EQU 256
LEDS	 		  EQU	$810
LEDS_ALT    EQU	$811
UART_DATA   EQU $800
RX_STATUS   EQU $801
TX_STATUS   EQU $802
VERSION     EQU $830

YMCTRL      EQU $A00
; cs_n, 0,0,0, // 0,0, rd_n, wr_n
YMSIGNALS   EQU $A01 ; { ym_irq_n, ym_ct2, ym_ct1, ym_pm, ym_p1, ym_sh2, ym_sh1, ym_so };
YMDATA      EQU $A02
YMPM        EQU $A03	; solo lectura
YMICN       EQU $A04
YMA0        EQU $A05
YMSPEED			EQU $A06  ; un 1 pone el YM a velocidad real, un 0 la baja a la mitad
YMDATA_SYNC	EQU $A08
YMLEFT      EQU $A0A
YMRIGHT     EQU $A0C
YMCNT       EQU $A10	; solo lectura
YMCNT_CLR		EQU $A20	; cualquier escritura borra YMCNT completo
YMLEFT_EXP  EQU $A1A
YMRIGHT_EXP EQU $A1C

; YM Registers
YM_CLKA1    EQU $10
YM_CLKA2    EQU $11
YM_CLKB     EQU $12
YM_CLKCTRL  EQU $14

; variables globales en el la pagina de DP
YMREG_SEL   EQU $0		; registro seleccionado para escritura
SO_CONTINUO EQU $1		; a 1 si hay que enviar SO todo el rato
BUFSIZE     EQU $2    ; kB * 4 del SO
BUFEND      EQU $4    ; direccion final del bufer de SO
CROSS_PERIOD EQU $6		; usado para contar el periodo de la señal de salida
LAST_LEFT   EQU $8
SENT_COUNT  EQU $10   ; 3 bytes, numero de datos a enviar
SEND_LIMIT  EQU $14
ZERO        EQU $16   ; 2 bytes a cero
SO_ZEROCNT  EQU $18   ; numero de ceros que hemos leido de SO
WAITSTA_CNT EQU $1A   ; numero de veces que se leyó el registro de estado. 1 bytes
AUX			EQU $1B	  ; variable comodin
; A partir de DP + $80 variables con direccion completa
YMLEFT_PTR	EQU STACK_START+$80	; contiene un puntero a YMLEFT o YMLEFT_EXP
											; de aqui es de donde leen las rutinas de SO


			ORG  MEM_START
			FILL $00,DP_START+DP_SIZE-MEM_START
			ORG  DP_START+DP_SIZE
RESET:ORCC #$50					; Disable interrupts
      LDS	 #STACK_START
			LDA  #$F2
			TFR  A,DP
			LDA  #$83
			STA  YMCTRL				; ni lectura ni escritura, la FPGA controla el bus, CS alto
			CLR  <SO_CONTINUO
			LDA  VERSION      ; Signal that we are waiting the first data
			STA  LEDS
			; Por defecto el bufer son 4kB
			LDA  #$20
			STA  <BUFSIZE
			LDD  #SO_BUFFER
			ADDA <BUFSIZE
			STD  <BUFEND
			CLRA
			CLRB
			STD  <ZERO
			LDD  #YMLEFT
			STD  YMLEFT_PTR

			JSR  RESETEA_YM
;		  Muestra la version
			LDA  VERSION      ; Signal that we are waiting the first data
			STA  LEDS

WAIT: ; bucle principal
			; no hay que escribir a LEDS en este bucle o no
			; se veria lo que escriban los programas
			LDA  <SO_CONTINUO
			BEQ  ESPERA_SYNC
ESPERA_SOCONT:	; espera a que entre un dato sin dejar de mandar SO
			LBSR  RDLEFT
			LBSR  ENVIA_D
			LDA  RX_STATUS
			BEQ  ESPERA_SOCONT
			BRA  LEE_CMD	; ejecuta el comando
ESPERA_SYNC
			SYNC  ; espera a que entre un dato
LEE_CMD:
			CLR  RX_STATUS
			LDA  UART_DATA
CMD_CASE:
			LDX  #CMD_TABLE
			ANDA #$1F
			STA  LEDS_ALT
			LSLA  ; A *= 2
			LDX  A,X
			JSR  ,X
			CLR  LEDS_ALT
			BRA  WAIT

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RESETEA_YM:
			LDA  #0
			STA  YMICN
			LDA  #255
YMRST_L0:
			DECA
			BNE  YMRST_L0
			LDA  #1
			STA  YMICN
			RTS

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;
;;   COMANDOS
;

RUN_CMD_SIGNALS:
			LDA  YMSIGNALS
			STA  UART_DATA
			JSR  WAIT_UART
			RTS


; Carga el siguiente dato de la UART en A
WAIT_EXTRA_DATA:
			LDA  RX_STATUS
			BITA #1
			BNE  DATA_READY
			SYNC
DATA_READY:
			CLR  RX_STATUS
			LDA  UART_DATA
			RTS

RUN_CMD_WR_LED:
			BSR  WAIT_EXTRA_DATA
			STA  LEDS
			RTS

;*****************************************************************
;********** LECTURA DE SO --- SOUND OUTPUT --
*****************************************************************
RUN_CMD_FASTKONSO:
			CLR  YMA0
			LDA  #8
			LBSR RUN_CMD_WRITE
			LDA  #1
			STA  YMA0
			LBSR WAIT_EXTRA_DATA
			PSHS A
			LBSR WAIT_EXTRA_DATA
			CLRB
			LSLA
			LSLA
			TFR  D,X
			PULS A
			LBSR RUN_CMD_WRITE	; hace el key on
			CLR  YMSPEED			; baja la velocidad del YM para que de tiempo

FASTKOS_LOOP:
			LBSR  RDLEFT
			LBSR  ENVIA_D
			LEAX -1,X
			CMPX #0
			BNE  FASTKOS_LOOP
			LDA  #1
			STA  YMSPEED		; restaura la velocidad normal
			RTS

RUN_CMD_KEYONSO:
			CLR  YMA0
			LDA  #8
			LBSR RUN_CMD_WRITE
			LDA  #1
			STA  YMA0
			LBSR WAIT_EXTRA_DATA
			PSHS A
			LBSR WAIT_EXTRA_DATA
			STA  <SEND_LIMIT
			CLR  <SENT_COUNT
			CLR  <SENT_COUNT+1
			CLR  <SENT_COUNT+2
			LDX  #0
			PULS A
			LBSR RUN_CMD_WRITE	; hace el key on
			CLR  YMSPEED			; baja la velocidad del YM para que de tiempo

CMDKOS_LOOP:
			LBSR  RDLEFT
			CMPD <ZERO
			BNE  CMDKOS_NOZERO
			;BRA  CMDKOS_NOZERO ; Cambio para medir el LFO
			LEAX  1,X
			CMPX #$8000
			BEQ  CMDKOS_FIN
			BRA  CMDKOS_SIGUE
CMDKOS_NOZERO:
			LDX  #0
CMDKOS_SIGUE:
			LBSR  ENVIA_D
			LDD  <SENT_COUNT+1
			ADDD #1
			BCC  CMDKOS_CC
			INC  <SENT_COUNT
CMDKOS_CC:
			STD  <SENT_COUNT+1
			; estamos en el limite?
			LDA  <SEND_LIMIT
			CMPA <SENT_COUNT
			BNE  CMDKOS_LOOP
CMDKOS_FIN:
			LDA  #1
			STA  YMSPEED		; restaura la velocidad normal
			RTS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
RUN_CMD_KEYOFFSO:
			CLR  YMA0
			LDA  #8
			LBSR RUN_CMD_WRITE
			LDA  #1
			STA  YMA0
			LBSR WAIT_EXTRA_DATA
			PSHS A
			LBSR WAIT_EXTRA_DATA
			STA  <SEND_LIMIT
			CLR  <SENT_COUNT
			CLR  <SENT_COUNT+1
			CLR  <SENT_COUNT+2
			LDX  #0
			LDA  ,S
			LBSR RUN_CMD_WRITE	; hace el key on
			CLR  YMSPEED			; baja la velocidad del YM para que de tiempo
			CLR  <AUX

CMDKOS2_LOOP:
			LBSR  RDLEFT
			CMPD <ZERO
			BNE  CMDKOS2_NOZERO
			LEAX  1,X
			CMPX #$8000
			BEQ  CMDKOS2_FIN
			BRA  CMDKOS2_SIGUE
CMDKOS2_NOZERO:
			LDX  #0
CMDKOS2_SIGUE:
			LBSR  ENVIA_D
			LDD  <SENT_COUNT+1
			ADDD #1
			BCC  CMDKOS2_CC
			INC  <SENT_COUNT
			STD  <SENT_COUNT+1
			; hacemos el keyoff?
			LDA  <AUX
			CMPA #2
			BEQ  CMDKOS2_NOKEYOFF
			CMPA #1
			BEQ  CMDKOS2_MANDAKEYOFF
			; si que hago el key off
			CLR  YMA0
			LDA  #8
			LBSR RUN_CMD_WRITE
			LDA  #1
			STA  <AUX
			BRA  CMDKOS2_NOKEYOFF
CMDKOS2_MANDAKEYOFF:
			LDA  #1
			STA  YMA0
			PULS A
			ANDA #7
			CLRA
			LBSR RUN_CMD_WRITE
			LDA  #2
			STA  <AUX
			BRA  CMDKOS2_NOKEYOFF
CMDKOS2_CC:
			STD  <SENT_COUNT+1
CMDKOS2_NOKEYOFF:
			; estamos en el limite?
			LDA  <SEND_LIMIT
			CMPA <SENT_COUNT
			BNE  CMDKOS2_LOOP
CMDKOS2_FIN:
			LDA  #1
			STA  YMSPEED		; restaura la velocidad normal
			RTS

RUN_CMD_DIRECT:
			LBSR WAIT_EXTRA_DATA
			STA  <SEND_LIMIT
			;CLR  YMSPEED			; baja la velocidad del YM para que de tiempo

			CLR  <SENT_COUNT
			CLR  <SENT_COUNT+1
			CLR  <SENT_COUNT+2
CMDDIR_LOOP:
			BSR  RDLEFT
			BSR  ENVIA_D
			LDD  <SENT_COUNT+1
			ADDA #1
			ADCB #0
			BCC  CMDDIR_CC
			INC  <SENT_COUNT
CMDDIR_CC:
			STD  <SENT_COUNT+1
			LDA  <SEND_LIMIT
			CMPA <SENT_COUNT
			BNE  CMDDIR_LOOP
			LDA  #1
			STA  YMSPEED		; restaura la velocidad normal
			RTS


RDLEFT:
			LDA  YMSIGNALS
			BITA #2
			BEQ  RDLEFT	; espera el 1 primero
RDLEFT_SH_A_CERO:
			LDA  YMSIGNALS
			BITA #2
			BNE  RDLEFT_SH_A_CERO
			LDD  [YMLEFT_PTR]
			RTS

ENVIA_D:
			PSHS A
			STB  UART_DATA
			LBSR WAIT_UART
			PULS A
			STA  UART_DATA
			LBSR WAIT_UART
			RTS

RUN_CMD_RD_SO:
			LDX  #SO_BUFFER
RDSO_CAPTURA:			; solo da tiempo a mandar un canal
			BSR  RDLEFT
			STD  ,X++			; este bucle se engancha cuando intento ocupar
			; toda la memoria. A lo mejor los limites estan mal puestos.
			; de momento funciona con 8kB
			; a ver cuantos llevamos
			;CMPX #SO_BUFFER+SO_BUFSIZE
			CMPX BUFEND
			BNE  RDSO_CAPTURA
			LDX  #SO_BUFFER
RDSO_ENVIA:
			LDD  ,X++
			BSR  ENVIA_D
			; a ver cuantos llevamos
			; CMPX #SO_BUFFER+SO_BUFSIZE
			CMPX BUFEND
			BEQ  RDSO_FIN
			BRA  RDSO_ENVIA
RDSO_FIN:
			RTS

LOOP_0CROSS:
			LDD  <CROSS_PERIOD
			ADDD #1
			STD  <CROSS_PERIOD
			CMPD #$FFFF
			BEQ  FIN_0CROSS

			LDD  [YMLEFT_PTR]
			STD  <LAST_LEFT
			BSR  RDLEFT

			CMPD #0
			BMI  LOOP_0CROSS
			LDD  <LAST_LEFT
			CMPD #0
			BGE  LOOP_0CROSS
FIN_0CROSS:
			RTS

RUN_CMD_0CROSS:
			LDY  #8
			LDX  #0
OTRO0CROSS:
			BSR  RDLEFT
			BSR  LOOP_0CROSS
			; Primer cruce hecho, borra el contador
			CLRA
			CLRB
			STD  <CROSS_PERIOD
			BSR  LOOP_0CROSS	; busca el segundo cruce

			LDD  <CROSS_PERIOD
			PSHS D
			LEAY -1,Y
			CMPY #0
			BNE  OTRO0CROSS
			; SUMA LOS 8 ULTIMOS
			PULS D
			ADDD ,S
			ADDD 2,S
			ADDD 4,S
			ADDD 6,S
			ADDD 8,S
			ADDD 10,S
			ADDD 12,S
			LBSR  ENVIA_D
			LEAS 14,S
			RTS

;*****************************************************************
RUN_CMD_RST_CNT:
			CLR  YMCNT_CLR
			RTS

RUN_CMD_TEST_D:
			LBSR WAIT_EXTRA_DATA
			STA  YMDATA
			STA  LEDS			
			RTS

RUN_CMD_TEST_CTRL:
			LBSR WAIT_EXTRA_DATA
			STA  YMCTRL
			STA  LEDS
			RTS

RUN_CMD_WR_ICN:
			LBSR WAIT_EXTRA_DATA
			STA  YMICN
			RTS

RUN_CMD_RD_ICN:
			LDA  YMICN
      STA  UART_DATA
			LBSR WAIT_UART
			RTS
;*******************************************************************
RUN_CMD_CONT_SO:
			LBSR WAIT_EXTRA_DATA
			STA  <SO_CONTINUO
			RTS

RUN_CMD_RDINT: ; lee datos internos
			CLR  YMA0
			LDA  #7
			STA  YMCTRL ; baja CS, activa level shifters para leer
			NOP
			NOP
			LDA  #5	; baja RD, FPGA lee
			CLRA
			STA  YMSPEED		; despacito y buena letra

			LDX  #0
RDINT_SIG:
			LBSR RDLEFT
			LDA  YMDATA_SYNC
			STA  UART_DATA
			LBSR WAIT_UART
			LEAX 1,X
			CMPX #4096
			BNE  RDINT_SIG
			; Restaura el bus
			LDA  #$83  ; sube WR, FPGA escribe
			STA  YMCTRL
			LDA  #1
			STA  YMSPEED		; restaura la velocidad normal
			RTS

RUN_CMD_WR_PAIR:
			LBSR WAIT_EXTRA_DATA
			PSHS A
			LBSR WAIT_EXTRA_DATA
			PSHS A
			; DIRECCION
			CLR  YMA0
			LDA  1,S
			BSR  RUN_CMD_WRITE
			BSR  RUN_CMD_WAIT_ST
			; DATO
			LDA  #1
			STA  YMA0
			LDA  ,S
			BSR  RUN_CMD_WRITE
			LEAS 2,S ; restaura la pila
			;LDA  #$FF
			;STA UART_DATA
			;JSR WAIT_UART

			BSR  RUN_CMD_WAIT_ST
			RTS

RUN_CMD_WAIT_ST:
			CLR  YMA0
			LDY  #$FF
YMWAITL:
			LDA  #7
			STA  YMCTRL ; baja CS, activa level shifters para leer
			NOP
			NOP
			LDA  #5	; baja RD, FPGA lee
			STA  YMCTRL
			LDB  YMDATA		; cargamos los datos
			LDA  #$83  ; sube WR, FPGA escribe
			STA  YMCTRL
			STB  LEDS
			NOP
			NOP
			BITB #$80	; bit "ocupado" a 1 ?
			BEQ  YMNOTBUSY
			LDX  #$FF
YMWAITWAIT:
			LEAX ,-X
			BNE  YMWAITWAIT
			LEAY ,-Y
			BNE  YMWAITL
YMNOTBUSY:
			TFR  Y,D
			NEGB
			STB  WAITSTA_CNT
			STB	 UART_DATA ; envia la cuenta para señalar el exito
			BRA  WAIT_UART
			;BRA YMNOTBUSY ; provoca un atasco real

RUN_CMD_WR_REG:
			CLR  YMA0
			LBSR WAIT_EXTRA_DATA
			STA  <YMREG_SEL
			BRA  RUN_CMD_WRITE

RUN_CMD_WR_DATA:
			LDA  #1
			STA  YMA0
			LBSR WAIT_EXTRA_DATA
			; si se cargan los temporizadores, borro el contador del PM
			LDB  <YMREG_SEL
			CMPB #$14
			BNE  RUN_CMD_WRITE
			BITA #3
			BEQ  RUN_CMD_WRITE
			CLR  YMCNT_CLR
			BRA  RUN_CMD_WRITE

RUN_CMD_WRITE:
; La escritura en registros es asincrona, YM PM no necesita conmutar
			STA  YMDATA
			LDA  #3
			STA  YMCTRL ; baja CS
			NOP
			NOP
			LDA  #2	; baja WR, FPGA escribe
			STA  YMCTRL
			NOP
			NOP
			LDA  #$83  ; sube WR, FPGA escribe
			STA  YMCTRL
			NOP
			NOP
			LDA  #$83
			STA  YMCTRL ; FPGA escribe, el bus de datos queda en Z
			RTS

RUN_CMD_RD_DATA:
			; POR HACER
			RTS

RUN_CMD_RD_CNT:
      LDX  #YMCNT
      LDB  #3
		  CLR  TX_STATUS
SNDL:
			LDA  B,X
      STA  UART_DATA
			BSR  WAIT_UART
      DECB
      BPL  SNDL
      RTS

RUN_CMD_UNKNOWN:
			LDA  #$AA
			STA  LEDS
			RTS

RUN_CMD_UART_TST:
			CLRB
UTST1:STB  UART_DATA
			JSR  WAIT_UART
			INCB
			BNE  UTST1
			RTS


WAIT_UART:
			LDA  TX_STATUS
			STA  LEDS_ALT
			BEQ  WAIT_UART
			CLR  TX_STATUS
			RTS

RUN_CMD_WAITIRQ:
			CLR  YMCNT_CLR
WIRQ1:
			LDA  YMSIGNALS
			STA  LEDS
			BITA #$80
			BNE  WIRQ1	; sin rastro de la IRQ
      BRA  RUN_CMD_RD_CNT		; send PM counter and end


RUN_CMD_RX_TST:
			CLRA
RX_TST_WAIT:
      SYNC
      CLR  RX_STATUS
			CMPA UART_DATA
			BEQ  RX_TST_SIGUE
			LDA  #1
      BRA  RX_TST_FIN
RX_TST_SIGUE:
			INCA
			BNE  RX_TST_WAIT
RX_TST_FIN:
			STA  UART_DATA ; A sera cero si todo ha ido bien
			BRA  WAIT_UART

RUN_CMD_VERSION:
			LDA  VERSION
			STA  UART_DATA
			LBRA WAIT_UART
			RTS

RUN_CMD_BUFSIZE:
			LBSR WAIT_EXTRA_DATA
			LSLA
			LSLA
			STA  <BUFSIZE
			LDD  #SO_BUFFER
			ADDA <BUFSIZE
			STD  BUFEND
			RTS

RUN_CMD_SOMODE:
			LBSR WAIT_EXTRA_DATA
			CMPA #0
			BEQ  SOMODE_LIN
			LDD  #YMLEFT_EXP
			STD  YMLEFT_PTR
			RTS
SOMODE_LIN:
			LDD  #YMLEFT
			STD  YMLEFT_PTR
			RTS

RUN_CMD_NOP:
			RTS


CMD_TABLE EQU $FF00
			FILL $FF, CMD_TABLE-*
			ORG  CMD_TABLE	; vectores de rutina

			FDB 	RUN_CMD_NOP     ; $00
			FDB		RUN_CMD_SIGNALS ; $01	 enviar señales
			FDB 	RUN_CMD_VERSION ; $02  Lee la version del verilog
			FDB 	RUN_CMD_WR_LED  ; $03  Escribe en el LED - 1 dato
			FDB 	RUN_CMD_RD_SO   ; $04  Lee 256x2 muestras
			FDB 	RUN_CMD_RST_CNT ; $05  Borra la cuenta del PM
			FDB 	RUN_CMD_WR_ICN  ; $06  Controla la línea de reset del YM - 1 dato
			FDB 	RUN_CMD_WAIT_ST ; $07  Lee el estado del YM hasta que esté libre
			FDB 	RUN_CMD_WR_REG  ; $08  Escribe con A0=0 - 1 dato
			FDB 	RUN_CMD_WR_DATA ; $09  Escribe con A0=1 - 1 dato
			FDB 	RUN_CMD_RD_DATA ; $0A  Lee el bus de datos del YM con A0=0
			FDB 	RUN_CMD_RD_CNT  ; $0B  Envía la cuenta del PM
			FDB 	RUN_CMD_RD_ICN  ; $0C  Envia el estado del reset del YM
			FDB 	RUN_CMD_WAITIRQ ; $0D	 Conmuta PM hasta que YM_IRQ baje.  volver a
														;      enviar para interrumpir la espera
			FDB 	RUN_CMD_RX_TST  ; $0E  Recibe del 0 al FF por la UART. Si es
														;      correcto contesta 0 si no, 1
			FDB 	RUN_CMD_UART_TST; $0F	 Envia del 0 al FF por la UART
			FDB 	RUN_CMD_CONT_SO ; $10	 Habilita o inhabilita el envio continuo SO
			FDB 	RUN_CMD_BUFSIZE ; $11	 El siguiente byte dice el numero de
														;      kilobytes que se usaran para el bufer de SO
			FDB 	RUN_CMD_0CROSS  ; $12	 Mide el numero de muestras entre dos
														;      cruces por cero
			FDB 	RUN_CMD_DIRECT  ; $13	 Manda SO sin pasar por el bufer hasta
														;      alcanzar 2^16*numero enviado
			FDB 	RUN_CMD_KEYONSO ; $14	 Hace un KEY ON con el segundo byte recibido
			                      ;      y luego envia hasta n bloques, donde n
			                      ;      es el tercer byte. Si se detectan
			                      ;      x ceros seguidos deja de enviar
			FDB 	RUN_CMD_SOMODE  ; $15	 Si se manda un 0 se lee SO lineal
														;			 si no, comprimido en mantisa+exp
			FDB 	RUN_CMD_FASTKONSO;$16	 Hace un KEY ON con el segundo byte recibido
			                      ;      y luego envia hasta n palabras, donde n
			                      ;      es el tercer byte.
			FDB 	RUN_CMD_RDINT   ; $17	 Envía 4096 valores de M1. Hay que programar
														;      el registro de TEST primero
			FDB  	RUN_CMD_WR_PAIR ; $18  Escribe en la direccion del primer valor adicional
								  ;      recibido, el segundo valor. Cuenta las esperas
								  ;      despues de escribir la direccion y despues de
								  ;      escribir el valor
			FDB		RUN_CMD_KEYOFFSO ; $19 como RUN_CMD_KEYONSO pero tras transmitir un
									; bloque apaga el canal. Sirve para medir el
									; release rate
			FDB		RUN_CMD_TEST_D		; $1A Escribe el valor en D
			FDB		RUN_CMD_TEST_CTRL	; $1B Escribe el valor en YMCTRL


TopMem	EQU	$FFF8
				FILL $FF,TopMem-*
				ORG TopMem
				FDB	$FFFF	; $FFF8
				FDB $FFFF, $FFFF
				FDB RESET
