#include <avr/io.h>
#include <util/delay.h>
#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1

#define Buzz PD5
#define LED PD7
#define ADD PC0
#define DEC PC1            
#define Show PC2
#define LOW PC3
#define MED PC4
#define HIGH PC5



#define set_bit(Y,bit_x) (Y |= (1 << bit_x))    //ativa o bit x da variável Y (coloca em 1)
#define clr_bit(Y,bit_x) (Y &=~(1 << bit_x))   //limpa o bit x da variável Y (coloca em 0) 
#define tst_bit(Y,bit_x) (Y & (1 << bit_x))     //testa o bit x da variável Y (retorna 0 ou 1)
#define cpl_bit(Y,bit_x) (Y ^= (1<<bit_x))    //troca o estado do bit x da variável Y (complementa)

volatile bool alarm_tocando=false;

void UART_Init(void){
  UBRR0H = (uint8_t) (MYUBRR >> 8); // ajusta a taxa de transmissão
  UBRR0L = (uint8_t) (MYUBRR);
  UCSR0A = 0; // desabilita velocidade dupla
  UCSR0B = (1 << RXEN0) | (1 << TXEN0);// habilita o transmissor e o receptor
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // ajusta o formato do frame 
 // 8 bits de dados 1 bit de parada  
}


//verifica se novo dado pode ser enviado pela UART
//Retorna valor 32 se novo dado pode ser enviado ou zero caso não
uint8_t uartTxOk(void){
 return (UCSR0A & (1 << UDRE0)); 
}

//Envia um byte pela pota UART
void uart_Transmit(uint8_t data){
  UDR0 = data; // coloca o dad no registrador de transmissão e o envia
}

//Envia uma string pela porta UART
void uartString (char *c){
  for(;*c!=0; c++){
    while(!uartTxOk());
    uart_Transmit(*c);
  }
}

//Verifica se UART possui novo dado
//Retorna valor 128 se existir novo dado recebido. Zerpo se não
uint8_t uartRx0k(void){
 return (UCSR0A & (1<< RXC0));
}

// ler byte recebido na porta UART
uint8_t uartRx(void){
 return UDR0;
}

//habilitar ou desabilitar a interrupção de recepção da USART
// x = 0, desabilita, qualquer outro valor, habilita a interrupçao

void uartIntRx(uint8_t _hab){
  if(_hab){
   UCSR0B |= ( 1 << RXCIE0); // habilitar a interrupção de recep 
  }else{
    UCSR0B &=~ ( 1 << RXCIE0); // desabilita a interrupção de recep 
  }
}

//habilitar ou desabilitar a interrupção de transmissão da USART
// x = 0, desabilita, qualquer outro valor, habilita a interrupçao
void uartIntTx(uint8_t _hab){
  if(_hab){
   UCSR0B |= ( 1 << TXCIE0); // habilitar a interrupção de recep 
  }else{
    UCSR0B &=~ ( 1 << TXCIE0); // desabilita a interrupção de recep 
  }
}


int main(){
  DDRD = 0xFF; // configurando como saida
  PORTD = 0b10011100;
  UART_Init(); //Inicalização UART

  DDRC = 0x00; // configurando a PORTC como entradas 
  PORTC = 0b0111111;// mudando o estados inicial das PORTC
  PCICR = 1 << PCIE1; // Habilitando interrupções por qaulquer mudança de sinal no PORTC
  PCMSK1 = (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10) | 
    	   (1 << PCINT11) |  (1 << PCINT12) | (1 << PCINT13);
  			// Habilitandos os PINOS para gerar interrupções 
  
  //PWM TC0
  TCCR0A = (1 << COM0A0) | (1 << WGM01); // troca de estados
  TCCR0B = (1 << CS02) | (1 << CS00); // TC0 prescaler de 1024
  OCR0A = 255 ;
  TIMSK0 = 1 << TOIE0;

  sei(); // habilitando as interrupções
 
  while(1){
    
  }
 return 0; 
}
ISR(TIMER0_OVF_vect){
 
    PORTD ^= (1<<Buzz);
  _delay_ms(1);
    PORTD ^= (1<<Buzz);
}




ISR(PCINT1_vect) {
  
   if (!tst_bit(PINC, ADD)) { //  hora
     
      uart_Transmit('1'); //
      uartString("\r\n");
  }
   else if (!tst_bit(PINC, DEC)) { // horas e minutos
     
     uart_Transmit('2'); //
      uartString("\r\n");
     
  }
  else if (!tst_bit(PINC, Show)) { // inserir alarme / soneca
     
    uart_Transmit('3'); //
      uartString("\r\n");
  }else if (!tst_bit(PINC, HIGH)) { // inserir alarme / soneca
     
    OCR0A=1;
  }
  else if (!tst_bit(PINC,MED )) { // inserir alarme / soneca
     
    OCR0A=50;
  }
  else if (!tst_bit(PINC, LOW)) { // inserir alarme / soneca
     
    OCR0A=255;
  }
   
}

