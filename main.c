/*
 * main.c
 *
 *  Construido com a IDE CCSv6 e compilador GNU v4.9.1 (Red Hat)
 */


#include "setup.h"
#include "uart/uart_tx.h"
#include "i2c/hmc5883l/hmc5883l.h"
#include "i2c/twi_master.h"

volatile int counter = 0;
volatile int counterRX = 0;
volatile int counterTX = 0;

void initPorts();

// Para sair do modo de baixo consumo no modo I2C
static volatile uint8_t FLAG_wakeUpI2C = 0;

/* Metodos externos para o I2C */
//extern void i2c_nack(void);
extern void i2c_rx(void);
extern void i2c_tx(void);
//extern void i2c_state_isr(void);
//extern void i2c_txrx_isr(void);

/* Metodo externo para o UART RX */
extern void uart_rx(void);

/* Variaveis WDT */
volatile unsigned long wdt_overflow_count = 0;
volatile unsigned long wdt_millis = 0;
volatile unsigned int wdt_fract = 0;

/* Para o controle do sleep */
volatile uint8_t sleeping = 0;

// Incrementa quando sleeping.
uint16_t SMILLIS_INC = 0;
uint16_t SFRACT_INC = 0;

// Parametros do magnometro
volatile uint8_t readMag = 0;	// Informa se deve ser realizado a leitura do magnometro
uint8_t magEnabled = 0;			// Informa que o magnometro estah habilitado
int mx = 0, my = 0, mz = 0;		// Dados do magnometro


/*
 * main.c
 */
int main(void) {

	initPorts();
    disableWatchDog();
    initClocks();
    enableWatchDog();
    saveUsbPower();
    setupUart();
    setupI2C();


    while (1) {

    	uint8_t enable = hmc5883l_detect();
    	if (enable) {
    		if (magEnabled == 0) {
    			hmc5883l_config();
    			magEnabled = 1;
    		}
    		hmc5883l_read_data(&mx,&my,&mz);
    		uart_printf("HMC5883L Habilitado\r\n");
    		uart_printf("x = %i; y = %i; z = %i\r\n", mx, my, mz);
    	}
    	else {
    		uart_printf("HMC5883L Desabilitado\r\n");
    		if (magEnabled)
    			magEnabled = 0;
    	}

    	delay(500);

    	counter++;

    }


	return 0;
}

void initPorts() {
	P1DIR |= BIT0;	// P1.0 output
	P1OUT &= ~BIT0;	// Disable P1.0
}

/*
 * Configuração para o TIMER_0 (disparado a cada 20ms)
 */
/*void setupTimer0(void) {
	TA0CTL = TASSEL_2 +		// Fonte do clock: SMCLK (25MHz)
			 MC_1 +			// Modo de contagem: progressiva
			 ID_3 +			// Fator de divisão: 8 ( 3125kHz = 0,32ms )
			 TACLR;         // Limpa contador

	TA0CCTL0 = CCIE;        // Habilita interrupção do Timer A Bloco CCR0
	TA0CCR0 = 62500;		// Valor a ser comparado: 62500 --> 20ms
}*/

/*
 * Disparado por UART RX
 * Para sair do modo de baixo consumo em determinado caracter recebido
 */
void uart_rx_auxiliar(uint8_t c) {}

void wakeUpI2C() {
	FLAG_wakeUpI2C = 1;
}

/*
 * Interrupção UART (USCI_A1)
 */
__attribute__ ((interrupt(USCI_A1_VECTOR)))
void USCI_A1_ISR (void)
{
  switch(UCA1IV)
  {
  	  case USCI_UCRXIFG: uart_rx(); break;
  	  case USCI_UCTXIFG: break;
  }

  __bic_SR_register_on_exit(LPM4_bits);
}

/*
 * Interrupção I2C (USCI_B0)
 */
__attribute__ ((interrupt(USCI_B0_VECTOR)))
void USCI_B0_ISR (void)
{
	switch(__even_in_range(UCB0IV,12))
	{
		case  0: break;			// Vector  0: No interrupts
		case  2: break;	        // Vector  2: ALIFG
		case  4: break;         // Vector  4: NACKIFG
		case  6: break;         // Vector  6: STTIFG
		case  8: break;         // Vector  8: STPIFG
		case 10:                // Vector 10: RXIFG
			counterRX++;
			i2c_rx();
			break;
		case 12:                // Vector 12: TXIFG
			counterTX++;
			i2c_tx();
			break;
		default: break;
	}

	// Sair do modo de baixo consumo caso solicitado
	if (FLAG_wakeUpI2C) {
		FLAG_wakeUpI2C = 0;
		__bic_SR_register_on_exit(LPM0_bits); // Sair do modo LPM0
	}

}

/*
 * Interrupção do WatchDog
 */
__attribute__((interrupt(WDT_VECTOR)))
void WDT_ISR (void)
{
	// Copia para variaveis locais para que possam ser armazenadas em registros
	// (variaveis volateis devem ser lidas da memoria em cada acesso)
	unsigned long m = wdt_millis;
	unsigned int f = wdt_fract;

	m += sleeping ? SMILLIS_INC : MILLIS_INC;
	f += sleeping ? SFRACT_INC : FRACT_INC;
	if (f >= FRACT_MAX) {
		f -= FRACT_MAX;
		m += 1;
	}

	wdt_fract = f;
	wdt_millis = m;
	wdt_overflow_count++;

	/* Sair do modo de baixo consumo */
	__bic_SR_register_on_exit(LPM0_bits);
}


// Timer0 A0 interrupt service routine
/*__attribute__((interrupt(TIMER_A0_VECTOR)))
void TIMER0_A0_ISR (void)
{
  P1OUT ^= 0x01;                            // Toggle P1.0
}
*/
