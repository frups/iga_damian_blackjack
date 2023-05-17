/*
 * Iga_Damian_Blackjack.c
 *
 * Created: 19.05.2022 11:02:00
 * Author : Damian Bakowski
 */ 
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <util/delay.h>
#include <unistd.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

const unsigned char deck[]={2,3,4,5,6,7,8,9,'J','Q','K','A'};//declaring card deck as global array
const unsigned int digitsHex[]={0x7e,0x30,0x6d,0x79,0x33,0x5b,0x5f,0x70,0x7f,0x7b,0x77,0x1f,0x4e,0x3d,0x4f,0x47};//declaring array of values in hex from 1 to 9 for display purposes
const unsigned int facesHex[]={0x3c,0x73,0x57,0x77};//declaring array of chars for face cards in hex J,Q,K,A for displaying letters
const unsigned int winMessageHex[]={0x4E,0x1D,0x15,0x5E,0x05,0x77,0x0F,0x3E,0x0E,0x77,0x0F,0x06,0x1D,0x15,0x5B};//CONGRATULATIONS - message for winner
const unsigned int looserMessageHex[]={0x3B, 0x1D,0x1C,0x00,0x77,0x05, 0x4F,0x00,0x77,0x00, 0x0E,0x7E,0x7E,0x5B,0x4F,0x05};//YOU ARE A LOOSER - message for looser

volatile unsigned char card;
volatile unsigned char playerHand[2];
volatile unsigned char dealerHand[2];
volatile unsigned int playerScore=0;
volatile unsigned int dealerScore=0;
volatile uint8_t state = 0;
volatile uint8_t lis = 0;
volatile uint8_t oldPINB = 0xFF;

//functions for cards drawing and managment
uint8_t trueRandom()//source:https://www.codeproject.com/Articles/5311070/A-True-Random-Number-Generator-in-Arduino-AVR-ATme
{
	int N = 1;
	uint8_t r;
	// 20 works really fine for uint8, can be increased to reduce patterns.
	// Each cycle takes around 80us on 16Mhz main clock
	// which give a random number almost every 1.5ms.
	for (int i = 0; i < N; i++)
	{
	  // Synthesize a voltage on the input
	  if (i % 2 == 0)
		ADMUX = 0b01001110; // [A] // High (1.1V). // See Datasheet Table 23-3 and 23-4
								   // for Atmega328p
	  else
	  ADMUX = 0b01001111; // [A] //  Low (0V)
	  ADCSRA |= 1 << ADSC;  // Start a conversion. See Datasheet for Atmega328p Page 207
	  uint8_t low, high;
	  // ADSC is cleared when the conversion finishes
	  while ((ADCSRA >> ADSC) % 2); // wait for the conversion to complete
	  low  = ADCL;          // do not swap this sequence. Low has to be fetched first.
	  high = ADCH;          // the value is always between 0-3
	  r ^= low;
	  r ^= high;

	  // Let's shift rotate the number;

	  uint8_t last = r % 2;
	  r >>= 1;
	  r |= last << 7;        // "7" will need to change to K-1 to use a K bits wide datatype
	  // Disable the ADC
	  ADCSRA = 0;

	  // Enable the ADC with a randomly selected clock Prescaler between 2 and 128.
	  // Since each conversion takes 13 ADC cycles, at 16Mhz system clock,
	  // the ADC will now take something in between 1.6us and 104us
	  // for each conversion (75us on the average).
	  ADCSRA = 0b10000000 | ((r % 4) << 1); // See Datasheet Table 23-5 for Atmega328p
	}
return r;
}

int cardTwoPoint(char card)
{	
	//int val;
	switch(card)
	{
		case 'J':
		case 'Q':
		case 'K':
			return 10;
			break;
		case 'A':
			return 11;
			break;
		default:
			//val = card-'0';//string to int conversion
			return card;
			break;
	}
}

char drawCard()
{
	volatile int r;
	uint8_t tr = trueRandom();
	r = srand(trueRandom())%13;
	return deck[r];
}

//functions for displaying content on 7seg
int getFirstDigit(int val)
{
	if (val>19)
	{
		return 2;
	}	
	else if (val>9)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
int getSecondDigit(int val)
{	
	if (val>19)
	{
		return (val-20);
	}
	else if (val>9)
	{
		return (val-10);
	}
	else
	{
		return val;
	}
}

void dispNum(int val, int period)//display two digit value on 7 segment display(only for numbers) - used for player score
{
	int firstDigit=getFirstDigit(val);
	int secondDigit=getSecondDigit(val);
	for(int i = 0;i<period;i++)
	{
		PORTE=~0x02;//7-segment disp - selecting display
		PORTD=~digitsHex[firstDigit];//7-segment disp - value to displa
		_delay_ms(10); // delay of one second
		PORTE=~0x01;//7-segment disp - selecting display
		PORTD=~digitsHex[secondDigit];//7-segment disp - value to display
		_delay_ms(10); // delay of one second
	}
}

void dispChars(char c, char a, char b, int period)//display two digit value on 7 segment display(for letters and numbers) - used for hand presentation
{
	for(int i = 0;i<period; i++)
	{
		PORTE=~0x08;//7-segment disp - selecting display
		switch(c){
			case 'J':
			PORTD=~facesHex[0];
			break;
			case 'Q':
			PORTD=~facesHex[1];
			break;
			case 'K':
			PORTD=~facesHex[2];
			break;
			case 'A':
			PORTD=~facesHex[3];
			break;
			default:
			PORTD=~digitsHex[c];
			break;
			}
		_delay_ms(2); // delay
		PORTE=~0x02;//7-segment disp - selecting display
		switch(a){
			case 'J':
				PORTD=~facesHex[0];
				break;
			case 'Q':
				PORTD=~facesHex[1];
				break;
			case 'K':
				PORTD=~facesHex[2];
				break;
			case 'A':
				PORTD=~facesHex[3];
				break;
			default:
				PORTD=~digitsHex[a];
				break;
		}
		_delay_ms(2); // delay
		PORTE=~0x01;//7-segment disp - selecting display
		switch(b){
			case 'J':
				PORTD=~facesHex[0];
				break;
			case 'Q':
				PORTD=~facesHex[1];
				break;
			case 'K':
				PORTD=~facesHex[2];
				break;
			case 'A':
				PORTD=~facesHex[3];
				break;
			default:
				PORTD=~digitsHex[b];
				break;
		}
		_delay_ms(2); // delay
	}
}

void dispMessage(int option)//display dynamically moving message based on defined global array
{
	while(1)
	{
		int d1=30;
		int d2=5;
		int h1=0;
		int h2=1;
		int h3=2;
		int h4=3;
		for (int j = 0;j<((round(sizeof(winMessageHex))/3)+3);j++)
		{
			for (int i = 0;i<d1;i++)
			{
				PORTE=~0x08;//7-segment disp - selecting display
				if(option == 1)
				{
					PORTD=~winMessageHex[h1];//7-segment disp - value to display
				}
				else
				{
					PORTD=~looserMessageHex[h1];//7-segment disp - value to display
				}
				_delay_ms(d2); // delay of one second
				PORTE=~0x04;//7-segment disp - selecting display
				
				if(option == 1)
				{
					PORTD=~winMessageHex[h2];//7-segment disp - value to display
				}
				else
				{
					PORTD=~looserMessageHex[h2];//7-segment disp - value to display
				}
				_delay_ms(d2); // delay of one second
				PORTE=~0x02;//7-segment disp - selecting display
				
				if(option == 1)
				{
					PORTD=~winMessageHex[h3];//7-segment disp - value to display
				}
				else
				{
					PORTD=~looserMessageHex[h3];//7-segment disp - value to display
				}
				_delay_ms(d2); // delay of one second
				PORTE=~0x01;//7-segment disp - selecting display
				
				if(option == 1)
				{
					PORTD=~winMessageHex[h4];//7-segment disp - value to display
				}
				else
				{
					PORTD=~looserMessageHex[h4];//7-segment disp - value to display
				}
				_delay_ms(d2); // delay of one second
			}
			h1++;
			h2++;
			h3++;
			h4++;
		}
	}
}

//main game logic and loops
void playerWin(void)
{
	dispMessage(1);
}

void playerLoose(void)
{
	dispMessage(0);
}

void playerLoop(void)
{
	while(1)
	{
		if(playerScore==21)
		{
			playerWin();
			break;
		}
		else if(playerScore>21)
		{
			playerLoose();
			break;
		}
		else
		{
			volatile int ustate = listen();
			if(ustate==1)//take next card
			{
				card= drawCard();
				playerScore += cardTwoPoint(card);
				dispNum(playerScore, 20);//update score on display
			}
			else//pass
			{
				break;
			}
		}
	}
}

void dealerLogic(void)
{
	while(dealerScore<16)
	{
		dispNum(dealerScore,50);
		if(dealerScore==21)
		{
			playerLoose();
			break;
		}
		else if(dealerScore>21)
		{
			playerWin();
			break;
		}
		else
		{
			unsigned char card= drawCard();
			dealerScore += cardTwoPoint(card);
		}
	}
	dispNum(dealerScore,50);
}

void finalCondition(void)//when no winner so far
{
	if (playerScore>dealerScore)
	{
		playerWin();
	}
	else
	{
		playerLoose();
	}
}

int blackjackCondition(void)// (at least one Ace and one face card)
{
	volatile int isAce =0;
	for(int i = 0;i<1;i++)
	{
		if(playerHand[i]=='A')
		{
			isAce=1;
		}
	}
	if(isAce)
	{
		for(int i = 0; i<1; i++)
		{
			if(playerHand[i]=='J' ||playerHand[i]=='Q'||playerHand[i]=='K'||playerHand[i]=='A')
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}
	}
	else
	{
		return 0;
	}
}


//interrupt for two buttons on PORTB
ISR(PCINT0_vect) {
	uint8_t changedbits = PINB ^ oldPINB;//for checking which edge type it is
	oldPINB = PINB;
  if (changedbits & (1 << PINB0)) {//detecting only rising edge
	  if (lis==0)//manually setting listener to avoid queued activations
	  {
		lis = 1;
		state = 1; //state for first button
	  }
  }

  if (changedbits & (1 << PINB1)) {//detecting only rising edge	  
	  if (lis==0)//manually setting listener to avoid queued activations
	  {
		lis = 1;
		state = 0; //state for second button
	  }
  }
}



//listener for interrupt
int listen(void)
{
	lis=0;
	while(lis==0)
	{
		PORTC = 0b01000000;//diode
		for (int i = 0; i<8; i++)
		{
			dispNum(playerScore,1);
			_delay_ms(5); // delay
			PORTC = PORTC >>1;//bitwise right shift operation for diode blinking effect
		}
	}
	return state;
}

void configuration(void)
{
	//port configuration
	DDRB=0x00;//buttons
	PORTB=0xFF;//buttons pullup resistor
	DDRC=0xFF;//LEDs
	DDRD=0xFF;//7seg disp - value to display
	DDRE=0xFF;//7seg disp - selecting display
		
	//interrupts configuration
	PCICR = 0b00000001;//activating interrupt
	PCMSK0 = 0b00000011;//selecting which pin should serve interrupt
	sei();//globally activating interrupts
}

void gameInit(void)
{
	//drawing two cards for player
	card = drawCard();
	playerHand[0]=card;
	playerScore += cardTwoPoint(card);
	card = drawCard();
	playerHand[1]=card;
	playerScore += cardTwoPoint(card);
	card = drawCard();
	//and one for a dealer
	dealerHand[0] = card;
	dealerScore += cardTwoPoint(card);
	//check if blackjack was hit
	if (blackjackCondition())
	{
		playerWin();
	}
	//displaying dealer and player starting hand for 0.03ms
	dispChars(dealerHand[0],playerHand[0],playerHand[1],300);
	//then only player score
	dispNum(playerScore,1);
}

int main(void)
{
	configuration();
	gameInit();
	playerLoop();
	dealerLogic();
	finalCondition();
	
	return 0;
}