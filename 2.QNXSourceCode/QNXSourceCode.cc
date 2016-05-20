/******************************************************************************
 * Ultrasonic Range Finder Operation
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
 *  		Siddharth Ramkrishnan (sxr4316@rit.edu)	: 04.25.2016
 *
 *****************************************************************************/

#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <sys/netmgr.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

static int			PulsData=0,		FuncEnable=0,	OutData = 0,	ClockPulse=0;

static uintptr_t	DIOCtlHandle,	PORTAHandle,	PORTBHandle,	PORTCHandle;

static uintptr_t	ADCTrigHandle,	ADCMsbHandle,	ADCStatusHandle;

// structure that sets the start and interval time for the timer

static struct 		itimerspec		time_info;

void HardwareInitialization()
{
	// Grant thread access to hardware registers

	(void)ThreadCtl( _NTO_TCTL_IO, NULL );

	ADCTrigHandle		=	mmap_device_io(1,0x280);

	ADCMsbHandle		=	mmap_device_io(1,0x281);

	ADCStatusHandle		=	mmap_device_io(1,0x283);

	PORTAHandle			=	mmap_device_io(1,0x288);

	PORTBHandle			=	mmap_device_io(1,0x289);

	PORTCHandle			=	mmap_device_io(1,0x28A);

	DIOCtlHandle		=	mmap_device_io(1,0x28B);

	// Indicate only Channel 0 has to be Analog to Digital Converted

	(void)out8(mmap_device_io(1,0x282),	0x00);

	// Indicate all DIO ports except Port C are output ports

	(void)out8(DIOCtlHandle,	0x01);

	(void)out8(PORTAHandle,		0x00);

	(void)out8(PORTBHandle,		0x00);

	// Reset ADC Trigger Port

	(void)out8(ADCTrigHandle,	0x00);

}

int ADCDataRead()
{
	char MsbData;

	(void)ThreadCtl( _NTO_TCTL_IO, NULL );

	//(void)out8(ADCStatusHandle,	0x00);

	(void)out8(ADCTrigHandle,	0x80);

	while(in8(ADCStatusHandle) &0x80);

	usleep(1000);

	MsbData		=	in8(ADCMsbHandle);

	return (int)((int)MsbData*256);
}

void* VoltageUpdation(void* arg)
{
	static	int		MeasuredData;

	while(FuncEnable)
	{
		MeasuredData	=	ADCDataRead();

		if((MeasuredData>=(-15000))&&(MeasuredData<=15000))
		{
			(void)out8(PORTBHandle,0x00);

			PulsData	=	(MeasuredData / 1024)	+	16;
		}
		else
		{
			(void)out8(PORTBHandle,0xFF);

			sleep(1);
		}

	printf("\r Measured Data : %d Transmitted Data : %d", MeasuredData, OutData);

	}

	pthread_exit(NULL);

	return arg;
}

void SignalTransmission(int a)
{
	OutData		=	( ClockPulse * 128 )	+	( PulsData & 0x1F );

	ClockPulse	=	(ClockPulse==0)	?	0x01	:	0x00	;

	(void)out8(PORTAHandle,	OutData)	;
}

void TimerInitialization()
{
    // timer being used
    static timer_t timer;

    // structure needed for timer_create
    static struct sigevent   event;

    // structure needed to attach a signal to a function
    static struct sigaction  action;

    // itimerspec:
    //      it_value is used for the first tick
    //      it_interval is used for the interval between ticks

	time_info.it_value.tv_nsec    =  	10000	;

	time_info.it_interval.tv_nsec = 	10000	;

    // this is used to bind the action to the function
    // you want called ever timer tick
    action.sa_handler  = &SignalTransmission;

    // this is used to queue signals
    action.sa_flags = SA_SIGINFO;

    // this function takes a signal and binds it to your action
    // the last param is null because that is for another sigaction
    // struct that you want to modify
    sigaction(SIGUSR1, &action, NULL);

    // SIGUSR1 is an unused signal provided for you to use

    // this macro takes the signal you bound to your action,
    // and binds it to your event
    SIGEV_SIGNAL_INIT(&event, SIGUSR1);

    // now your function is bound to an action
    // which is bound to a signal
    // which is bound to your event

    // now you can create the timer using your event
    timer_create(CLOCK_REALTIME, &event, &timer);

    // and then start the timer, using the time_info you set up
    timer_settime(timer, 0, &time_info, 0);
}

int main()
{
	static int thread, policy;

	static pthread_t VoltageThread;

	static pthread_attr_t threadAttributes;

	static struct sched_param parameters;

	HardwareInitialization();

	(void) pthread_attr_init(&threadAttributes);

	(void) pthread_getschedparam(pthread_self(), &policy, &parameters);

	parameters.sched_priority--;

	pthread_attr_setschedparam(&threadAttributes, &parameters);

	(void)printf("\n\r Analog Signal Detection and Rapid Resposne System \n\r");

	(void)printf("\n\r Waiting for trigger to start\n");

	while((in8(PORTCHandle)&0x01)==0);

	while((in8(PORTCHandle)&0x01)!=0);

		FuncEnable	=	1;

	TimerInitialization();

	thread = pthread_create(&VoltageThread, &threadAttributes, VoltageUpdation, NULL);

	while(1);

	return 0;
}
