/******************************************************************************
 * ANALOG SIGNAL DETECTION AND RAPID RESPONSE SYSTEM
 *
 * Description:
 *
 * A twin platform system based out of QNX purple box and Freescale board. The QNX
 *
 * board detects an analog voltage supplied to it, which is quantized, range checked and
 *
 * then communicated to the Freescale board. The Freescale board receives the data from
 *
 * the QNX, interprets and diagnoses the signal before providing visual indication of the
 *
 * received data on a PWM controlled servo motor.
 *
 * Author:
 *  		Siddharth Ramkrishnan (sxr4316@rit.edu)	: 05.10.2016
 *
 *****************************************************************************/

// system includes
#include <hidef.h>      /* common defines and macros */
#include <stdio.h>      /* Standard I/O Library */

// project includes
#include "types.h"
#include "derivative.h" /* derivative-specific definitions */

// Definitions

// Change this value to change the frequency of the output compare signal.
#define OC_FREQ_HZ    ((UINT16)10)

// Macro definitions for determining the TC1 value for the desired frequency in Hz (OC_FREQ_HZ).
#define BUS_CLK_FREQ  ((UINT32) 2000000)   
#define PRESCALE      ((UINT16)  2)         
#define TC1_VAL       ((UINT16)  (((BUS_CLK_FREQ / PRESCALE) / 2) / OC_FREQ_HZ))

static UINT8  FunctionEnable = 0;

static UINT16 CurrentTime = 0;

// Initializes SCI0 for 8N1, 9600 baud, polled I/O
void InitializeSerialPort(void)
{
    // Set baud rate to ~9600
    SCI0BD = 13;          
    
    // Enable the transmitter and receiver.
    SCI0CR2_TE = 1;	SCI0CR2_RE = 1;
}


// Initializes I/O and timer settings for the demo.
//--------------------------------------------------------------       
void InitializeTimer(void)
{
  // Set the timer prescaler to %2, since the bus clock is at 2 MHz,
  TSCR2_PR0 = 1;
  TSCR2_PR1 = 0;
  TSCR2_PR2 = 0;      
    
  // Enable input capture on Channel 1 
  TIOS_IOS1 = 0;
  
  
  // Set up input capture edge control to capture on a rising edge.
  TCTL4_EDG1A = 1;
  TCTL4_EDG1B = 1;
   
  // Clear the input capture Interrupt Flag (Channel 1)
  TFLG1 = TFLG1_C1F_MASK;
  
  // Enable the input capture interrupt on Channel 1;
  TIE_C1I = 1; 
  
  // Enable the timer
  TSCR1_TEN = 1;
   
  // Enable interrupts via macro provided by hidef.h
  EnableInterrupts;
}

void InitializePWM0()
{
	PWMCLK_PCLK0	=	1;
   
	PWMPOL_PPOL0	=	1;
   
	PWMSCLA			=	0x50;
   
	PWMPER0			=	0xFF;
   
	PWME_PWME0		=	1;
}

void InitializePWM1()
{
	PWMCLK_PCLK1	=	1;
	
	PWMPOL_PPOL1	=	1;
	
	PWMSCLA			=	0x50;
	
	PWMPER1			=	0xFF;
	
	PWME_PWME1		=	1;
}

void PortDeclaration()
{
	DDRA	=	0x00	;
	
	DDRB	=	0xFF	;
}

// Output Compare Channel 1 Interrupt Service Routine
// Refreshes TC1 and clears the interrupt flag.
//          
// The first CODE_SEG pragma is needed to ensure that the ISR
// is placed in non-banked memory. The following CODE_SEG
// pragma returns to the default scheme. This is neccessary
// when non-ISR code follows. 
//
// The TRAP_PROC tells the compiler to implement an
// interrupt funcion. Alternitively, one could use
// the __interrupt keyword instead.
// 
// The following line must be added to the Project.prm
// file in order for this ISR to be placed in the correct
// location:
//		VECTOR ADDRESS 0xFFEC OC1_isr 
#pragma push
#pragma CODE_SEG __SHORT_SEG NON_BANKED
//--------------------------------------------------------------       
void interrupt 9 OC1_isr( void )
{

	FunctionEnable	=	1		;
	
	CurrentTime		=	TC1		;
	
	PORTB			    =	0x00	;	
	
	PWMDTY0			=	4+((PORTA&0x1F)*20)/32 ;
	
	PWMDTY1			=	4+((PORTA&0x1F)*20)/32	;
	
  (void)printf(" \n \r Received Data : %d " , PORTA) ;
	
   // set the interrupt enable flag for that port because it is cleared every everytime an interrupt fires.
   
   TFLG1   =   TFLG1_C1F_MASK;
}
#pragma pop


// This function is called by printf in order to
// output data. Our implementation will use polled
// serial I/O on SCI0 to output the character.
//
// Remember to call InitializeSerialPort() before using printf!
//
// Parameters: character to output
//--------------------------------------------------------------       
void TERMIO_PutChar(INT8 ch)
{
    // Poll for the last transmit to be complete
    do
    {
      // Nothing  
    } while (SCI0SR1_TC == 0);
    
    // write the data to the output shift register
    SCI0DRL = ch;
}


// Polls for a character on the serial port.
//
// Returns: Received character
//--------------------------------------------------------------       
UINT8 GetChar(void)
{ 
  // Poll for data
  if(SCI0SR1_RDRF == 0)
   
    // Fetch and return data from SCI0
    return SCI0DRL;
    
}

//Initialize the Pulse Period Capture data
void initPulseCapture()
{
    TFLG1   =   TFLG1_C1F_MASK;
}

// Function to call all the functions used to Initialize the System
//-----------------------------------------------------------------       
void hardwareinit(void)
{
  InitializeSerialPort();
  
  initPulseCapture()	;
  
  PortDeclaration()		;
  
  InitializeTimer()		;
  
  InitializePWM0()		;
  
  InitializePWM1()		;
  
  PWMDTY0	=	0x0D	;
  
  PWMDTY1	=	0x0D	;
}

// Entry point of our application code
//--------------------------------------------------------------       
void main(void)
{

  static INT32  TimeDifference = 0 ;
  
  (void)hardwareinit();
     
  (void)printf("\n\r Analog Signal Detection and Rapid Resposne System \n\r");

  (void)printf("\n\r Waiting for trigger to start\n");
  
  while(FunctionEnable==0);
  
  (void)printf("\n\r Press any key to exit\n");
    
  while(1)
  {
  
	   TimeDifference	=	(TC1 >= CurrentTime)	?	(TC1 - CurrentTime) :	(TC1 + ( 65536 - CurrentTime))	;  
    
	   if(TimeDifference	>	20000) 
     {
        (void)printf("\n\rTime Difference = %u ",TimeDifference);
     
        PORTB	=	0xFF	;	
     }
     
     while(PORTB!=0);

  }
  
}