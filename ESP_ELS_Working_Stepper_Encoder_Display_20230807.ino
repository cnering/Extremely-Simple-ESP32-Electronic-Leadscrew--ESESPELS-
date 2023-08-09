#include <LiquidCrystal_I2C.h>
#include <ESP32Encoder.h>
#include "FastAccelStepper.h"
#include <Arduino.h>

//git test
/*!!!STEPPER DRIVER!!!*/
#define dirPinStepper 34
#define enablePinStepper 35
#define stepPinStepper 36

int step_drives_per_second = 0;

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

/*!!!ENCODER!!!*/
ESP32Encoder encoder;

/*!!!LCD!!!*/

// set the LCD number of columns and rows
int lcdColumns = 20;
int lcdRows = 4;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

char current_LCD_line_1[20];
char current_LCD_line_2[20];
char current_LCD_line_3[20];
char current_LCD_line_4[20];

int feed_rate = 0;


u_int64_t display_last_updated_time = esp_timer_get_time();

u_int64_t last_LCD_timer = 0;

/*!!!INPUT BUTTONS!!!*/

#define FEED_INCREASE_BUTTON 15
#define FEED_DECREASE_BUTTON 5
#define MODE_SELECT_BUTTON 17

#define FEED_MODE 0
#define TPI_MODE 1
#define PITCH_MODE 2

#define DISPLAY_REFRESH_HZ 1

int last_display_refresh = 0;

int feed_up_hold = 0;
int feed_down_hold = 0;

int run_mode_debounce = 0;

int run_mode = 0;
int tpi = 0;
int pitch = 0;


String metric_pitches[20] = {};



/*!!!TASK!!!*/
/*!!!Task - setting it up so stepper and encoder are on one core, everything else is on the other core!!!*/
TaskHandle_t Task1;


long last_count = 0;

u_int64_t last_timer = 0;

u_long calculations_per_second = 0;

u_long rpm_display_last_count = 0;

//float last_rotation = 0.0;

void setup(){

  metric_pitches[0] = "0.35";
  metric_pitches[1] = "0.40";
  metric_pitches[2] = "0.45";
  metric_pitches[3] = "0.50";
  metric_pitches[4] = "0.60";
  metric_pitches[5] = "0.70";
  metric_pitches[6] = "0.80";
  metric_pitches[7] = "1.00";
  metric_pitches[8] = "1.25";
  metric_pitches[9] = "1.50";
  metric_pitches[10] = "1.75";
  metric_pitches[11] = "2.00";
  metric_pitches[12] = "2.50";
  metric_pitches[13] = "3.00";
  metric_pitches[14] = "3.50";
  metric_pitches[15] = "4.00";
  metric_pitches[16] = "4.50";
  metric_pitches[17] = "5.00";
  metric_pitches[18] = "5.50";
  metric_pitches[19] = "6.00";

  Serial.begin(115200);

  /*Input buttons*/
  pinMode(FEED_INCREASE_BUTTON, INPUT_PULLUP);
  pinMode(FEED_DECREASE_BUTTON, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(16, INPUT_PULLUP);
  pinMode(MODE_SELECT_BUTTON, INPUT_PULLUP);


  /*!!!STEPPER DRIVER!!!*/
  stepper = engine.stepperConnectToPin(0,DRIVER_RMT);
  if (stepper) {
      //stepper->setAutoEnable(true);

      //stepper->setSpeedInUs(1000);  // the parameter is us/step !!!
      //stepper->setAcceleration(10000);
      //stepper->move(70);
      //stepper->runForward();
    } else {
      Serial.println("Stepper Not initialized!");
      delay(1000);
    }

  /*!!!ENCODER!!!*/
  // Enable the weak pull down resistors
  //ESP32Encoder::useInternalWeakPullResistors=DOWN;
  // Enable the weak pull up resistors
  ESP32Encoder::useInternalWeakPullResistors = UP;

  //CAN ATTACH MULTIPLE ENCODERS TO FIX PCNT/RMT ISSUE
  //Uncomment this to run two encoders to fix the stupid PRNT library thing
  //encoder0.attachFullQuad(17, 16);
  encoder.attachFullQuad(33, 32);

  /*!!!LCD!!!*/
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();



  /*!!!TASK!!!*/
  xTaskCreatePinnedToCore(
    high_priority_loop, /* The realtime loop that calculates stepper pulses and reads the encoder */
    "Encoder and Stepper", /* Name of the task */
    1000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &Task1,  /* Task handle. */
    0); /* Core where the task should run */


}

void loop(){
  /*UI updates - check the buttons and set the UI values*/
  buttonCheck();
  /*Update the display, including calculating things like RPM, thread pitch, feed rate, etc*/
  lcdUpdate();
  /*Display refresh, currently 10hz*/
  delay(100);
}

void high_priority_loop(void * parameter){
  for(;;) {
    u_int64_t cur_timer = esp_timer_get_time();
    
    //debugging, figuring out how many calculations per second can be achieved, this should not run in prod
    calculations_per_second++;
    
    /*record current encoder counts to compare to previous counts to find distance travelled since last check*/
    int32_t cur_count = encoder.getCount();
    
    int counts_delta = cur_count - last_count;
    
    /*if we haven't moved, don't do anything, otherwise drive to new location*/
    if(counts_delta != 0){
      /*get the numerical number of rotations since we last checked*/
      /*encoder is 600 p/r, with quadrature that's 2,400 counts per rotation.  This gives us a decimal representation of the percentage that the spindle has turned since we last checked*/
      /*2,400 counts per revolution on an 8 TPI leadscrew means a resolution of 0.000052 inches per encoder step, which is insane*/
      float rotation = counts_delta/2400.0;
      
      /*Set the global last_rotation to this value, MAY NOT NEED*/
      //last_rotation = rotation;
      stepper->move(counts_delta);
      step_drives_per_second++;
    }
    /*Debugging, just outputting the calculations per second*/
    if(cur_timer - last_timer > 1000000){
      Serial.println(calculations_per_second);
      Serial.println(step_drives_per_second);
      calculations_per_second = 0;
      step_drives_per_second = 0;
      last_timer = cur_timer;
    }

    last_count = cur_count;

    /*TODO: Logic to calculate stepper position from encoder rotation percentage*/
  //  stepper->move(1);
  }
}

void moveStepperToPosition(int counts){
  stepper->move(counts);
}



void lcdUpdate(){

  last_display_refresh++;

  if(last_display_refresh >= 10/DISPLAY_REFRESH_HZ){

    last_display_refresh = 0;

    String encoder_pos = String("Encoder = " + String(String((int32_t)encoder.getCount())+"          ").substring(0,20));

    /*first line*/
    
    for(int i = 0; i < 20; i++){
      if (encoder_pos.charAt(i) != current_LCD_line_1[i]){
        lcd.setCursor(i,0);
        lcd.print(encoder_pos[i]);
        current_LCD_line_1[i] = encoder_pos.charAt(i);
      }
    }
    
    
    /*second line*/
    long rpm_display_current_count = (int32_t)encoder.getCount();
    u_int64_t display_current_update_time = esp_timer_get_time();
    long rpm_delta = rpm_display_current_count - rpm_display_last_count;
    u_int64_t micros_since_last_update = display_current_update_time - display_last_updated_time;
    float seconds_since_last_update = micros_since_last_update/1000000.0;
    float current_RPMs = rpm_delta/2400.0/seconds_since_last_update*60.0;
    String RPMs_current = "RPMs = " + String(String(current_RPMs)+String("                    ")).substring(0,13);
    rpm_display_last_count = rpm_display_current_count;
    display_last_updated_time = display_current_update_time;
    for(int i = 0; i < 20; i++){
      if (RPMs_current.charAt(i) != current_LCD_line_2[i]){
        lcd.setCursor(i,1);
        lcd.print(RPMs_current[i]);
        current_LCD_line_2[i] = RPMs_current.charAt(i);
      }
    }

    /*third line*/
    String sfm_current = "SFM @ 1in: "+ String(String(current_RPMs/3.82) + String("            ")).substring(0,10);

    for(int i = 0; i < 20; i++){
      if (sfm_current.charAt(i) != current_LCD_line_3[i]){
        lcd.setCursor(i,2);
        lcd.print(sfm_current[i]);
        current_LCD_line_3[i] = sfm_current.charAt(i);
      }
    }
  }

  String feed_string = "";
  /*fourth line*/
  if(run_mode == 0){
    feed_string = String("Feed: 0.") +String(String("000") + String(feed_rate)).substring(String(feed_rate).length(),String(String("000") + String(feed_rate)).length()) + String("         ");
  } else if(run_mode == 1){
    feed_string = String("TPI: ") +String(String("000") + String(tpi)).substring(String(tpi).length(),String(String("000") + String(tpi)).length()) + String("            ");
  } else if(run_mode == 2){
    feed_string = String("Pitch: ") + metric_pitches[pitch] + "         ";
  } else{
    //do nothing
  }
  

   for(int i = 0; i < 20; i++){
    if (feed_string.charAt(i) != current_LCD_line_4[i]){
      lcd.setCursor(i,3);
      lcd.print(feed_string[i]);
      current_LCD_line_4[i] = feed_string.charAt(i);
    }
  } 
}

void buttonCheck(){
  if(!digitalRead(FEED_INCREASE_BUTTON)){
    if(feed_rate < 150){
      feed_hold_check(FEED_INCREASE_BUTTON);
    }
  } else{
    feed_up_hold = 0;
  }
  if(!digitalRead(FEED_DECREASE_BUTTON)){
    if(feed_rate > 1){
      feed_hold_check(FEED_DECREASE_BUTTON);
    }
  } else{
    feed_down_hold = 0;
  }
  if(!digitalRead(MODE_SELECT_BUTTON)){
    run_mode_debounce++;
    if(run_mode_debounce == 2){
      if(run_mode == 2){
        run_mode = 0;
      } else{
        run_mode++;
      }
    }
  } else{
    run_mode_debounce = 0;
  }
}

void feed_hold_check(int pin){
  if(pin == FEED_INCREASE_BUTTON){
    feed_up_hold++;
    if(feed_up_hold == 1 || feed_up_hold > 10){
      if(run_mode == FEED_MODE){
        feed_rate++;
      } else if(run_mode == TPI_MODE){
        tpi++;
      } else if(run_mode == PITCH_MODE && pitch < 20){
        pitch++;
      } else{
        //do nothing
      }
      
    }
  }
  if(pin == FEED_DECREASE_BUTTON){
    feed_down_hold++;
    if(feed_down_hold == 1 || feed_down_hold > 10){
      if(run_mode == FEED_MODE){
        feed_rate--;
      } else if(run_mode == TPI_MODE){
        tpi--;
      } else if(run_mode == PITCH_MODE && pitch > 0){
        pitch--;
      } else{
        //do nothing
      }
    }
  }
}
