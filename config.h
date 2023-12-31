#define SPEED_MODE_REFRESH_MICROSECONDS 10000


/*!!!MAGIC NUMBER DEFINITIONS!!!*/

/*How many counts per full encoder revolution*/
#define ENCODER_COUNTS_FULL_REV 4096.0

/*How many steps per full servo revolution*/
#define STEPPER_STEPS_FULL_REV 4096.0

/*Thou per rev for your leadscrew*/
#define LEADSCREW_THOU_PER_REV 125.0

/*Any final drive calculations, like or a belt system or gear reduction*/
#define FINAL_DRIVE_RATIO 4
#define FINAL_DRIVE_RATIO_FLOAT 4.0

/*The encoder final drive calculations, like or a belt system or gear reduction*/
#define ENCODER_FINAL_DRIVE_RATIO 2.666666666666666666

/*I don't know why you'd ever need a feed higher than 150 thou per rev, but if you did you could set it here*/
#define MAX_FEED_RATE 150

/*The amount you want the fractional-thou feed rates to count up and down*/

#define FRACTIONAL_FEED 0.1

/*===============================================*/

/*!!!UI REFRESH RATES IN HZ!!!*/
#define LCD_REFRESH_RATE 1
#define BUTTON_REFRESH_RATE 30

/*===============================================*/

/*!!!INPUT BUTTON DEFINES!!!*/

/*UI Buttons*/
#define FEED_INCREASE_BUTTON 4
#define FEED_DECREASE_BUTTON 5
#define MODE_SELECT_BUTTON 17
#define DIRECTION_BUTTON 16
#define ON_OFF_BUTTON 26

/*===============================================*/

/*!!!STEPPER DRIVER DEFINES!!!*/
#define STEPPER_DIRECTION_PIN 18
#define STEPPER_ENABLE_PIN 35
#define STEPPER_STEP_PIN 19

/*Steps per second squared.  For my servo, I want it to accelerate essentially instantly.  The way the program works, and the speed
of the ESP32, means that I'm rarely ever sending more than a single step at a time, so the acceleration doesn't factor in very much*/
#define STEPPER_ACCELERATION 1000000

/*===============================================*/

/*!!!ENCODER DEFINES!!!*/
#define ENCODER_PIN_A 34
#define ENCODER_PIN_B 35
