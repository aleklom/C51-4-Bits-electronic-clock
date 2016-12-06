/**
 * Very very simple demonstration code written in C language
 * @file ysz4_orig_clock.c
 */

///////////////////////////////////////////////////////
//        This source code is free of charge!        //
//        Feel free to use it.                       //
///////////////////////////////////////////////////////

// Based on:
//     http://www.uelectronics.info/2014/11/08/kitchen-timer-8051/  article's source code
// Initial source code on github:
//     https://github.com/maly/51clock

///////////////////////////////////////////////////////

// YSZ-4 C51 4 bits clock firmware source code
#include <at89x051.h>

// 12000000 Hz mean 12000000 periods per second.
// 1 machine cycle is 12 periods of quartz oscillator frequency (our is 12000000Hz or 12MHz).
// On 12MHz oscillator every machine cycle will be 12000000 / 12 = 1000000Hz,
// this mean 1/1000000 = 0.0000001s = 0.001ms = 1us.
// timer/counter 0 in timer mode(TMOD = 0x1) counts machine cycles in pair TH0 & TL0 8 bit registers.
// Maximum value of pair TH0:TL0(16 bits) is 65535 or 0xffff.
// 0xffff + 0x0001 -> 0x0000 is overflow.
// ** TF0(TCON.5) timer 0 overflow flag. Set by hardware on Timer/Counter overflow.
// ** Cleared by hardware when processor vectors to interrupt routine.
// If we need interrupt after every 50ms or 50000us, we shall write to pair TH0:TL0
// 65535 - 50000 + 1 = 15536 value.
#define TIMER0_VALUE 15536

#define POS1 P3_0
#define POS2 P3_1
#define POS3 P3_2
#define POS4 P3_7

//#define test 1

#ifdef test
#define POS_ON 1
#define POS_OFF 0
#define pauza ;
#else
#define POS_ON 0
#define POS_OFF 1
#define pauza while (--j != 0) {;}
#endif

// display mode types
// submenu id's
#define DM_SUBM_A   0
#define DM_SUBM_B   1
#define DM_SUBM_C   2
#define DM_SUBM_D   3
#define DM_SUBM_E   4
#define DM_SUBM_F   5
#define DM_SUBM_G   6
#define DM_SUBM_H   7
#define DM_SUBM_I   8
// seconds correction
#define DM_SEC_COR  9
// hour:minute, minute:second
#define DM_TIME_HM 10
#define DM_TIME_MS 11

// clocks
#define TIME_CUR 0
#define TIME_A1  1
#define TIME_A2  2

// time increment type: hour or minute or second
#define INC_HOUR 0
#define INC_MIN  1
#define INC_SEC  2
// if currently seconds equal to 59, then seconds equal to 00, minutes equal to minutes + 1, etc.
#define INC_NORM 3
// increment only seconds or minutes or hours
// if currently seconds equal to 59, then after increment seconds equal to 00,
// minutes left unchanged, etc.
#define INC_ONLY 4

// S1 & S2 keys state
#define K_PRESSED  0
#define K_RELEASED 1

// boolean values
#define TRUE  1
#define FALSE 0

typedef unsigned char byte;

typedef struct {
	byte hour_h, hour_l, min_h, min_l, sec_h, sec_l;
} time_str;

volatile time_str time_table[3] = {
	{ 1,2,5,9,0,0 }, // current time
	{ 1,3,0,1,0,0 },  // alarm1
	{ 1,3,0,2,0,0 }   // alarm2
};

volatile byte flag_beep_hour = FALSE;
byte j; //univerzalni zpozdovac

/*
 AAAA
F    B  
F    B
F    B
 GGGG
E    C
E    C
E    C
 DDDD

P1 = 0b A F B E D C G dp
*/

#define	DIGIT_OFF	10
#define	DIGIT_O		0
#define	DIGIT_N		11
#define	DIGIT_F		17

byte table[21] = {
	0b11111100, //0
	0b00100100, //1
	0b10111010, //2
	0b10101110, //3
	0b01100110, //4
	0b11001110, //5
	0b11011110, //6
	0b10100100, //7
	0b11111110, //8
	0b11101110, //9
// turn off digit
	0b00000000, //  :10 :nothing on display
// on/off
	0b11110100, //N :11
// submenu "abcdefghi"
	0b11110110, //A :12 :table[12+subm] where subm is DM_SUBM_x
	0b01011110, //B :13
	0b11011000, //C :14
	0b00111110, //D :15
	0b11011010, //E :16
	0b11010010, //F :17
	0b11011100, //G :18
	0b01110110, //H :19
	0b01010000  //I :20
};

volatile byte intflag; // 50ms tick counter

// interrupt body
void Timer0_ISR(void) __interrupt (1) {
	intflag--; //decrement after 50ms
	TL0 = TIMER0_VALUE & 0xff; //load timer0 low byte
	TH0 = (TIMER0_VALUE) >> 8; //load timer0 high byte
}

// time incrementation by second
// return TRUE if hour reached
void time_inc(byte time_type, byte inc_hms, byte inc_type) {
    switch (inc_hms) {
	case INC_SEC:
	    time_table[time_type].sec_l++;
	    if (time_table[time_type].sec_l == 10) {
	        if (time_table[time_type].sec_h == 5) { // minute reached
		    time_table[time_type].sec_h = 0;
		    time_table[time_type].sec_l = 0;
		    // leave without minutes incrementation
		    if (inc_type == INC_ONLY) break;
		    // there is no "break", flow to minute incrementation
	        } else {
		    time_table[time_type].sec_l = 0;
		    time_table[time_type].sec_h++;
		    // increment only seconds
		    break;
		}
	    } else break;
	case INC_MIN:
	    time_table[time_type].min_l++;
	    if (time_table[time_type].min_l == 10) {
	        if (time_table[time_type].min_h == 5) { // hour reached
		    time_table[time_type].min_h = 0;
		    time_table[time_type].min_l = 0;
		    // leave without hours incrementation
		    if (inc_type == INC_ONLY) break;
		    // inform that hour reached between 8.00 and 20.00
		    if ((time_table[time_type].hour_l > 7) && // 8.00
		        (time_table[time_type].hour_h < 2)) // 20.00
			flag_beep_hour = TRUE;
		    // there is no "break", flow to hour incrementation
	        } else {
	            time_table[time_type].min_l = 0;
		    time_table[time_type].min_h++;
		    // increment only hours
		    break;
		}
	    } else break;
	case INC_HOUR:
	    time_table[time_type].hour_l++;
	    if (time_table[time_type].hour_h == 2 && time_table[time_type].hour_l == 4) { // midnight
		time_table[time_type].hour_h = 0;
		time_table[time_type].hour_l = 0;
		// leave without day of week incrementation
		if (inc_type == INC_ONLY) break;
		// there is no "break", flow to day of week incrementation
	    } else {
		if (time_table[time_type].hour_l == 10) {
		    time_table[time_type].hour_l = 0;
		    time_table[time_type].hour_h++;
		}
		break;
	    }
    }
}

// seven segment display
volatile byte ssd[4]; // positions on display (from left to right: 0,1,2,3)

// decimal point led
byte dp = 0;

void show1(byte a) { // 1 from the left
	P1 = a; // set segment as usual
	POS1 = POS_ON;
	pauza
	POS1 = POS_OFF;
}
void show2(byte a) { // 2 from the left
	P1 = a | dp; // set segment with decimal point LED
	POS2 = POS_ON;
	pauza
	POS2 = POS_OFF;
}
void show3(byte a) { // 3 from the left
	P1 = a; // set segment as usual
	POS3 = POS_ON;
	pauza
	POS3 = POS_OFF;
}
void show4(byte a) { // 4 from the left
	P1 = a; // set segment as usual
	POS4 = POS_ON;
	pauza
	POS4 = POS_OFF;
}

void prepare_onoff(byte flag) {
    if (flag) {
	ssd[1] = DIGIT_OFF; // display nothing
	ssd[2] = DIGIT_O; // 'O' like '0'
	ssd[3] = DIGIT_N; // 'N'
    } else {
	ssd[1] = DIGIT_O; // 'O' like '0'
	ssd[2] = DIGIT_F; // 'F'
	ssd[3] = DIGIT_F; // 'F'
    }
}

volatile byte display_mode = DM_TIME_HM;

volatile byte flag_on_time_alarm = TRUE; // on time alarm switch from 8.00 to 20.00
volatile byte flag_alarm1_active = FALSE; // alarm1 is not active
volatile byte flag_alarm2_active = FALSE; // alarm2 is not active
volatile byte flag_alarm1_on = TRUE; // alarm1 is on
volatile byte flag_alarm2_on = TRUE; // alarm2 is on

void display() {
    if (display_mode >= DM_TIME_HM) { // display current time only
	if (display_mode == DM_TIME_HM) { // hours:minutes
	    ssd[0] = time_table[TIME_CUR].hour_h;
	    ssd[1] = time_table[TIME_CUR].hour_l;
	    ssd[2] = time_table[TIME_CUR].min_h;
	    ssd[3] = time_table[TIME_CUR].min_l;
	} else { // minutes:seconds
	    ssd[0] = time_table[TIME_CUR].min_h;
	    ssd[1] = time_table[TIME_CUR].min_l;
	    ssd[2] = time_table[TIME_CUR].sec_h;
	    ssd[3] = time_table[TIME_CUR].sec_l;
	}
	show1(table[ssd[0]]);
	show2(table[ssd[1]]);
	show3(table[ssd[2]]);
	show4(table[ssd[3]]);
    } else if (display_mode == DM_SEC_COR) { // seconds correction mode
	ssd[0] = time_table[TIME_CUR].min_h;
	ssd[1] = time_table[TIME_CUR].min_l;
	ssd[2] = time_table[TIME_CUR].sec_h;
	ssd[3] = time_table[TIME_CUR].sec_l;
	show1(table[ssd[0]]);
	show2(table[ssd[1]]);
	show3(table[ssd[2]]);
	show4(table[ssd[3]]);
    } else { // display submenu items
	show1(table[12+display_mode]); // display submenu letter A..I
	
	switch (display_mode) {
	    case DM_SUBM_A: // current time: correction for hours
		ssd[1] = DIGIT_OFF; // display nothing
		ssd[2] = time_table[TIME_CUR].hour_h;
		ssd[3] = time_table[TIME_CUR].hour_l;	    
		break;
	    case DM_SUBM_B: // current time: correction for minutes
		ssd[1] = DIGIT_OFF; // display nothing
		ssd[2] = time_table[TIME_CUR].min_h;
		ssd[3] = time_table[TIME_CUR].min_l;
		break;
	    case DM_SUBM_C: // on time alarm switch
		prepare_onoff(flag_on_time_alarm);
		break;
	    case DM_SUBM_D: // the first alarm clock switch
		prepare_onoff(flag_alarm1_on);
		break;
	    case DM_SUBM_E: // the first alarm clock set for hours
		ssd[1] = DIGIT_OFF; // display nothing
		ssd[2] = time_table[TIME_A1].hour_h;
		ssd[3] = time_table[TIME_A1].hour_l;	    
		break;
	    case DM_SUBM_F: // the first alarm clock set for minutes
		ssd[1] = DIGIT_OFF; // display nothing
		ssd[2] = time_table[TIME_A1].min_h;
		ssd[3] = time_table[TIME_A1].min_l;
		break;
	    case DM_SUBM_G: // the second alarm clock switch
		prepare_onoff(flag_alarm2_on);
		break;
	    case DM_SUBM_H: // the second alarm clock set for hours
		ssd[1] = DIGIT_OFF; // display nothing
		ssd[2] = time_table[TIME_A2].hour_h;
		ssd[3] = time_table[TIME_A2].hour_l;	    
		break;
	    case DM_SUBM_I: // the second alarm clock set for minutes
		ssd[1] = DIGIT_OFF; // display nothing
		ssd[2] = time_table[TIME_A2].min_h;
		ssd[3] = time_table[TIME_A2].min_l;
		break;	
	}
	show2(table[ssd[1]]);
	show3(table[ssd[2]]);
	show4(table[ssd[3]]);
    }    
}

byte alarm_time_in_sec = 0;

void check_alarm() {
    if (flag_alarm1_on && !flag_alarm1_active &&
	time_table[TIME_CUR].hour_h == time_table[TIME_A1].hour_h &&
	time_table[TIME_CUR].hour_l == time_table[TIME_A1].hour_l &&
	time_table[TIME_CUR].min_h == time_table[TIME_A1].min_h &&
	time_table[TIME_CUR].min_l == time_table[TIME_A1].min_l) {
	// activate alarm1 and exclude activation again
	flag_alarm1_active = TRUE;
	// add additional 8 seconds
	alarm_time_in_sec = 8;
    }

    if (flag_alarm2_on && !flag_alarm2_active &&
	time_table[TIME_CUR].hour_h == time_table[TIME_A2].hour_h &&
	time_table[TIME_CUR].hour_l == time_table[TIME_A2].hour_l &&
	time_table[TIME_CUR].min_h == time_table[TIME_A2].min_h &&
	time_table[TIME_CUR].min_l == time_table[TIME_A2].min_l) {
	// activate alarm2 and exclude activation again
	flag_alarm2_active = TRUE;
	// add additional 8 seconds
	alarm_time_in_sec = 8;
    }
    
}

volatile byte flag_one_second_started = FALSE;
volatile byte delta_buzzer_enabled = 0;
volatile byte delta_buzzer_disabled = 0;
volatile byte beep_intflag;
void beep_alarm(void) {
    if (alarm_time_in_sec > 0) {
	if (!flag_one_second_started) {
	    flag_one_second_started = TRUE;
	    delta_buzzer_enabled = 10; // beep half of second
	}
	if (beep_intflag != intflag) {
	    beep_intflag = intflag;
	    
	    if (delta_buzzer_enabled > 0) {
		P3_3 = 0; // enable buzzer beep
		delta_buzzer_enabled --;
		if (delta_buzzer_enabled == 0) delta_buzzer_disabled = 10; // silence half of second
	    }
	    
	    if (delta_buzzer_disabled > 0) {
		P3_3 = 1; // disable buzzer beep
		delta_buzzer_disabled --;
		if (delta_buzzer_disabled == 0) {
		    flag_one_second_started = FALSE; // one second is out
		    alarm_time_in_sec --; // descrement one second
		}
	    }
	}
    }
}

void beep_hour(void) {
    if (flag_beep_hour) {
	flag_beep_hour = FALSE; // deactivate flag_beep_hour
	if (alarm_time_in_sec < 3) alarm_time_in_sec = 3; // set alarm to three seconds long
    }
}

// Clock is running? By default - yes.
volatile byte flag_run;

volatile byte S1_ctr, S2_ctr;
volatile byte long_S1, long_S2;
volatile byte short_S2_ctr;
volatile byte S1_prev_state, S2_prev_state;
volatile byte flag_S1_long_press, flag_S2_long_press;

void S1_released(void) {
    if (S1_prev_state == K_PRESSED) {
	S1_ctr = 30;
	long_S1 = 0;
    }

    flag_S1_long_press = FALSE;
    S1_prev_state = K_RELEASED; 
}

void S1_pressed(void) {
    if (--S1_ctr == 0) {
	S1_ctr = 30; // autorepeat
	if (!flag_S1_long_press) {
	    long_S1 ++;
	    if (long_S1 > 5) {
		flag_S1_long_press = TRUE;
		
		if (display_mode >= DM_TIME_HM) {
		    display_mode = DM_SUBM_A;
		    //EA = 0; // disable all interrupts
		    flag_run = FALSE;	// disable time flow
		} else { // display in submenu mode
		    switch (display_mode) {
			case DM_SUBM_A: // current time: correction for hours
			    display_mode = DM_SUBM_B;
			    break;
			case DM_SUBM_B: // current time: correction for minutes
			    display_mode = DM_SUBM_C;
			    break;
			case DM_SUBM_C: // on time alarm switch
			    display_mode = DM_SUBM_D;
			    break;
			case DM_SUBM_D: // the first alarm clock switch
			    if (flag_alarm1_on) display_mode = DM_SUBM_E;
			    else display_mode = DM_SUBM_G;
			    break;
			case DM_SUBM_E: // the first alarm clock set for hours
			    display_mode = DM_SUBM_F;
			    break;
			case DM_SUBM_F: // the first alarm clock set for minutes
			    display_mode = DM_SUBM_G;
			    break;
			case DM_SUBM_G: // the second alarm clock switch
			    if (flag_alarm2_on) display_mode = DM_SUBM_H;
			    else display_mode = DM_TIME_HM;
			    break;
			case DM_SUBM_H: // the second alarm clock set for hours
			    display_mode = DM_SUBM_I;
			    break;
			case DM_SUBM_I: // the second alarm clock set for minutes
			    display_mode = DM_TIME_HM;
			    flag_run = TRUE;	// enable time flow
			    break;	
		    }
		}
	    }
	}
    }
    S1_prev_state = K_PRESSED;
}

void S2_released(void) {
    if (S2_prev_state == K_PRESSED) {
	S2_ctr = 30;
	long_S2 = 0;

	if (display_mode >= DM_TIME_HM) {
	    if (!flag_S1_long_press) {
		if (display_mode == DM_TIME_HM) {
		    display_mode = DM_TIME_MS;
		} else {
		    display_mode = DM_TIME_HM;
		}
	    }
	} else if (display_mode == DM_SEC_COR) {
	    if (!flag_S2_long_press) {
		if (short_S2_ctr < 2) { // count short presses
		    short_S2_ctr++;
		} else {
		    display_mode = DM_TIME_MS;
		    short_S2_ctr = 0; // S2 key short press counter set to zero after second press
		    flag_run = TRUE;
		}
	    }
	} else {
	    switch (display_mode) {
		case DM_SUBM_A: // current time: correction for hours
		    time_inc(TIME_CUR, INC_HOUR, INC_ONLY); // increment clock hour
		    break;
		case DM_SUBM_B: // current time: correction for minutes
		    time_inc(TIME_CUR, INC_MIN, INC_ONLY); // increment clock minute
		    break;
		case DM_SUBM_C: // on time alarm switch
		    if (flag_on_time_alarm) flag_on_time_alarm = FALSE;
		    else flag_on_time_alarm = TRUE;
		    break;
		case DM_SUBM_D: // the first alarm clock switch
		    if (flag_alarm1_on) flag_alarm1_on = FALSE;
		    else flag_alarm1_on = TRUE;
		    break;
		case DM_SUBM_E: // the first alarm clock set for hours
		    time_inc(TIME_A1, INC_HOUR, INC_ONLY); // increment alarm1 hour
		    break;
		case DM_SUBM_F: // the first alarm clock set for minutes
		    time_inc(TIME_A1, INC_MIN, INC_ONLY); // increment alarm1 minute
		    break;
		case DM_SUBM_G: // the second alarm clock switch
		    if (flag_alarm2_on) flag_alarm2_on = FALSE;
		    else flag_alarm2_on = TRUE;
		    break;
		case DM_SUBM_H: // the second alarm clock set for hours
		    time_inc(TIME_A2, INC_HOUR, INC_ONLY); // increment alarm2 hour
		    break;
		case DM_SUBM_I: // the second alarm clock set for minutes
		    time_inc(TIME_A2, INC_MIN, INC_ONLY); // increment alarm2 minute
		    break;	
	    }
	}
    }

    flag_S2_long_press = FALSE;
    S2_prev_state = K_RELEASED;
}

void S2_pressed(void) {
    if (--S2_ctr == 0) {
	S2_ctr = 30; // autorepeat
	if (!flag_S2_long_press) {
	    long_S2 ++;
	    if (long_S2 > 5) {
		flag_S2_long_press = TRUE;

		flag_run = FALSE;

		if (display_mode == DM_TIME_MS) {
		    display_mode = DM_SEC_COR;
		    time_table[TIME_CUR].sec_h = 0;
		    time_table[TIME_CUR].sec_l = 0;
		}
	    }
	}
    }
    S2_prev_state = K_PRESSED;
}

volatile byte S1_press_ctr, S2_press_ctr;
volatile byte S1_press_started, S2_press_started;
void beep_press(void) {
    if (P3_4 == 0 && S1_prev_state == K_RELEASED) {
	P3_3 = 0;	// enable buzzer beep
	S1_press_ctr = 45;
	S1_press_started = TRUE;
    }
    if (S1_press_started == TRUE) {
        if (S1_press_ctr > 0) S1_press_ctr --;
        else {
            P3_3 = 1;	// disable buzzer beep
            S1_press_started = FALSE;
        }
    }

    if (P3_5 == 0 && S2_prev_state == K_RELEASED) {
	P3_3 = 0;	// enable buzzer beep
	S2_press_ctr = 45;
	S2_press_started = TRUE;
    }
    if (S2_press_started == TRUE) {
        if (S2_press_ctr > 0) S2_press_ctr --;
        else {
            P3_3 = 1;	// disable buzzer beep
            S2_press_started = FALSE;
        }
    }
}


// entry point of the program
void main() {
    // turn off display
    POS1 = POS_OFF;
    POS2 = POS_OFF;
    POS3 = POS_OFF;
    POS4 = POS_OFF;    
    
    //set P3.4 and P3.5 to input
    P3_4 = 1; //S1 key
    P3_5 = 1; //S2 key    
    
    //set every bit of display to zero
    ssd[0] = 0;
    ssd[1] = 0;
    ssd[2] = 0;
    ssd[3] = 0;
    
    //set timer0 to interrupt every 50ms using 12MHz oscillator
    TMOD = 0x1; // 16bit timer mode, TL0 & TH0 are cascaded; there is no prescaler
    TL0 = TIMER0_VALUE & 0xff; // low byte
    TH0 = (TIMER0_VALUE) >> 8; // high byte
    TR0 = 1; // timer0 allow to run
    ET0 = 1; // enable timer0 interrupt
    EA = 1;  // enable all interrupts, clock & alarms active
    intflag = 20; // 50ms * 20 = 1000ms, not bigger than 1 second
    flag_run = TRUE;	// enable time flow

    S1_ctr = 30;
    long_S1 = 0;
    flag_S1_long_press = FALSE;
    S1_prev_state = K_RELEASED; 

    S2_ctr = 30;
    long_S2 = 0;
    flag_S2_long_press = FALSE;
    S2_prev_state = K_RELEASED; 

    while (1) {
        beep_press();
        
	// S1 released
	if (P3_4 == 1) {
	    S1_released();
	}
	// S1 pressed
	if (P3_4 == 0) {
	    S1_pressed();
	}
	// S2 released
	if (P3_5 == 1) {
	    S2_released();
	}
	// S2 pressed
	if (P3_5 == 0) {
	    S2_pressed();
	}

	display(); // display information on ssd display

	check_alarm(); // check alarm1 & alarm2	
	beep_alarm();
	beep_hour();

	if (intflag == 10) { dp = 0; } // every half second turn off decimal point LED

	if (intflag == 0) { // one second exceeded
	    intflag = 20; // reload 50ms tick counter to 1 second
	    if (flag_run) {
		dp = 1; // turn on decimal point LED
		time_inc(TIME_CUR, INC_SEC, INC_NORM); // increment clock
	    }
	}
    } // while (1)
}