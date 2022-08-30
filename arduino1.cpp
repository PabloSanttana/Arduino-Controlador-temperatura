
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1


#define clr_bit(Y,bit_x) (Y &=~(1 << bit_x)) 
#define set_bit(Y,bit_x) (Y |= (1 << bit_x)) 
#define cpl_bit(Y,bit_x) (Y ^= (1<<bit_x))

#define AREF 0 // Tensão de Referencia = Aref
#define AVCC 1 // Tensão de Referencia = Avcc
#define VR11 2 // Tensão de Referencia = 1,1 V
#define ADC0 0 // Selecionar a entrada ADC0
#define ADC1 1 // Selecionar a entrada ADC1
#define ADC2 2 // Selecionar a entrada ADC2
#define ADC3 3 // Selecionar a entrada ADC3
#define ADC4 4 // Selecionar a entrada ADC4
#define ADC5 5 // Selecionar a entrada ADC5
#define Vtemp 6// Selecionar o Sensor de Temperatura interna
#define V11 7 // Selecionar a tensão(1,1 v)
#define Vgnd 8 // Selecionar a tensão GND (0v)


#define DG1 PD2
#define DG2 PD3
#define HIST PD7
#define BIP PD5
#define LOW PC0
#define MED PC1
#define HIGH PC3

//Estados do display
volatile int temp_digits[2]={0};


//Estados do histerese
volatile int histerese_digits[2]={2,5};
uint16_t value_histerese = 25;
volatile bool histerese_toview=false;


volatile unsigned int TIMER = 0;
volatile unsigned int SETDISPLAY = 0;
volatile bool histerese_add=false;
volatile unsigned int show_add = 0;





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

//----------------------------------------------------------------------------------
//Configura o conversor ADC 
// ref = 0. Para usar a tensão de referencoa Aref
//ref = 1. Para usar a tensão de referencia Avcc - lembre-se do capacitor 100nf
//ref = 2. Para usar a tensão de referencia interna de 1,1 V
// did: valor para o registrador DIDR0

void adcBegin(uint8_t ref,uint8_t did){
  ADCSRA = 0; // configuração inicial
  ADCSRB = 0; // configurção inicial
  DIDR0 = did; // valor do did
  
  if(ref ==0){
  	ADMUX &=~((1<<REFS1) | (1<<REFS0)); //Aref
  }
  if((ref ==1) || (ref > 2)){
   	 ADMUX &=~(1<<REFS1); //Avcc
     ADMUX |=(1<<REFS0);//Avcc
  }
  if(ref ==2){
   	ADMUX |=((1<<REFS1) | (1<<REFS0)); //tensão interna 1.1v
  }
  
  ADMUX &=~(1<<ADLAR);// alinhamento a direita
  ADCSRA |= (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);
  // habilitar AD. Prescaler de 128 (clk_AD = F_cpu/128)
}

//-----------------------------------------------------------------------------
// Selecionar canal do ADC
// 0 <= channel <= 5 - ler dos pinos AD0 a AD5
// channel = 6 - leitura do sensir de temperatura 
// channel = 7 - 1,1v
// channel > 7 - GND
void adcChannel(uint8_t canal){
  if(canal <= 5){ // selescionar o canal no multiplex
   ADMUX = (ADMUX & 0xF0) | canal;
  }
  else if(canal == 6){// sensor de temperatura interno
     ADMUX = (ADMUX & 0xF0) | 0x08 ; 
  }
  else if(canal == 7){ // seleciona 1.1v
     ADMUX = (ADMUX & 0xF0) | 0x0E ; 
  }
  else if(canal > 7){// seleciona GND
      ADMUX = (ADMUX & 0xF0) | 0x0F ; 
  }

} 
 
//----------------------------------------------------------------------------
// iniciar a conversão
void adcSample(void){
  ADCSRA |= (1<<ADSC); // iniciar a conversão
}

//------------------------------------------------------
//verificar se a conversão foi concluida
//Retorna valor 0 se conversão concluida. 64 se não.

uint8_t adcOk(void){
 return (ADCSRA & (1<<ADSC)); 
}

//----------------------------------------------------------
//ler o ADC e retorna o Valor lido do ADC
uint16_t adcReadOnly(){
  return ((ADCH<<8) | ADCL); // retorna o valor do ADC
}

//------------------------------------------------------------
//converte, aguarda, ler e retorna valor lido do ADC
uint16_t adcRead(){
  adcSample(); // iniciar a conversão
  while(adcOk()); // aguardando a finalização da conversão ADSC =0
  return adcReadOnly(); // retorna o valor do ADC
}

//-------------------------------------------------------------------------
//habilitar ou desabilitar interrupções do ADC
// se X =0, desabilita interuupçao
// Caso contrario, habilitada
void adcIntEn(uint8_t x){
  if(x){
    ADCSRA |= (1<<ADIE); //habilitar interrupçao do ADC
  }
  else{
    ADCSRA &=~(1<<ADIE);
  }
}




//---------------------------------------------------------------------------
//Envia pela uart variavel de 2 bytes (16 bits) com digitos em decimal
void uartDec2B(uint16_t valor)
{ int8_t disp;
	char digitos[5];
	int8_t conta = 0;

     if(valor <104){
       valor = 00;
     }else if (valor >= 305){
        valor= 99;

     }else{
       valor = (valor - 103) / 2;
     }
 
     if(valor >= value_histerese){
       	set_bit(PORTC,HIGH);
       	set_bit(PORTC,MED);
        clr_bit(PORTC,LOW);
     }
     if(valor >= value_histerese + 2){
       	set_bit(PORTC,HIGH);
       	set_bit(PORTC,LOW);
		clr_bit(PORTC,MED);
     }
     if(valor >= value_histerese + 4){
       set_bit(PORTC,MED);
       set_bit(PORTC,LOW);
       clr_bit(PORTC,HIGH);
     }
 
 
 do //converte o valor armazenando os algarismos no vetor digitos
	{ disp = (valor%10);//armazena o resto da divisao por 10 e soma com 48
		valor /= 10;
		digitos[conta]=disp;
		conta++;
	} while (valor!=0);
	
 
   temp_digits[0] = digitos[1];
   temp_digits[1] = digitos[0];
 
 
 	
}


void hanldeSetDisplay(){
  
  if(SETDISPLAY == 1){
   set_bit(PORTD,DG2); 
    if(histerese_toview ){
    	PORTB = histerese_digits[0];
    }else{
     PORTB =temp_digits[0];
    }
  
   clr_bit(PORTD,DG1);   
  }
  else if(SETDISPLAY == 15){
   set_bit(PORTD,DG1);   // desligar  o digitos display anterior
   if(histerese_toview  ){
    	PORTB = histerese_digits[1];
    }else{
     PORTB =temp_digits[1];
    }
   clr_bit(PORTD,DG2);
  }
   else if(SETDISPLAY==29){
   SETDISPLAY=0;
  } 
  
}

void temperature(){
  
  // habilitar a leitura do podenciomentro
  	  uint16_t valorADC;
      valorADC = adcRead(); //ler o valor analogico
      uartDec2B(valorADC);
   	_delay_ms(200); // aguarda
    
}


void add_histerese(bool add_or_DCM = true){
  histerese_add=true;
  histerese_toview = true;
  set_bit(PORTD,BIP);
  if(add_or_DCM && value_histerese < 90){
   value_histerese++;
  	histerese_digits[1]++;
       if(histerese_digits[1]>=10){
         histerese_digits[0]++;
         histerese_digits[1] = 0;
     }
  }else if(value_histerese > 5){
    
    value_histerese--;
    if(histerese_digits[1]==0){
    	histerese_digits[0]--;
         histerese_digits[1] = 9;
    }else{
    	histerese_digits[1]--;
      
    }
  }
  
  _delay_ms(100);
  clr_bit(PORTD,BIP);
  

}



int main(){
  DDRD = 0xFF; // configurando como saida
  DDRB = 0b00111111;
  PORTD = 0b00001100;
  PORTB = 0b00110000;
  DDRC =  0b00001011;
  PORTC = 0b00001011;

 UART_Init(); //Inicalização UART
 uartIntRx(1); // habilitar recebimento UART interr.
  
  adcBegin(AVCC, 0x01); //Inicialização A/D
  adcChannel(ADC2);// porta A/d
  
  
  TCCR0A=(1<<COM0A0) | (1<<WGM01); // troca de estados
  TCCR0B = (1<<CS00) | (1<<CS01); // TC0 prescaler de 64
 OCR0A = 249; // maximo valor de contagem do registrador TCNT0
 TIMSK0 = 1<<OCIE0A; // interrupção no OCR0A
   

  sei(); 
  
  while(1){
    temperature();
 
  }
 return 0; 
}



ISR(TIMER0_COMPA_vect){ // interrupção acada 1ms
  TIMER += 1;
  SETDISPLAY +=1;
  hanldeSetDisplay();  
  
  if(TIMER == 1000){
  	TIMER=0;
    //Mostra valor do Histeres
    if(histerese_add){
      
    	show_add++;
      if(show_add == 2){
       PORTD = (0<< HIST);
      	histerese_toview = false;
        histerese_add=false;
        show_add=0;
      }
  	}
  }
  
  
}




ISR(USART_RX_vect){
  
  uint8_t dado_rx; 
  dado_rx = uartRx();
  while(!uartTxOk());
    switch(dado_rx){//testa o valor Lido
            //+
            case '1':
               add_histerese();
                break;
            //-
            case '2':
                add_histerese(false);
                break;
            //Ver histerese
            case '3':
          		histerese_toview = !histerese_toview;
      			PORTD ^= (1<< HIST);
                break;
        }
    
}
















