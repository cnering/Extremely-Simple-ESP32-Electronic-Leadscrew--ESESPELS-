#include <LiquidCrystal_I2C.h>
#include <ESP32Encoder.h>
#include "FastAccelStepper.h"
#include <Arduino.h>
#include "tables.h"

/*!!!MAGIC NUMBER DEFINITIONS*/
#define ENCODER_COUNTS_FULL_REV 2400.0
#define STEPPER_STEPS_FULL_REV 2400.0
#define LEADSCREW_THOU_PER_REV 125.0
#define FINAL_DRIVE_RATIO 4

/*===============================================*/

/*!!!MODE DEFINES!!!*/
#define FEED_MODE 0
#define TPI_MODE 1
#define PITCH_MODE 2

/*===============================================*/

/*!!!UI REFRESH RATES IN HZ!!!*/
#define LCD_REFRESH_RATE 1
#define BUTTON_REFRESH_RATE 30

/*!!!UI GLOBAL VARIABLES!!!*/
// set the LCD number of columns and rows
int lcdColumns = 20;
int lcdRows = 4;
u_long rpm_display_last_count = 0;
u_int64_t display_last_updated_time = esp_timer_get_time();
unsigned long display_millis = 0;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

char current_LCD_line_1[20];
char current_LCD_line_2[20];
char current_LCD_line_3[20];
char current_LCD_line_4[20];

/*===============================================*/

/*!!!INPUT BUTTON DEFINES!!!*/
#define FEED_INCREASE_BUTTON 4
#define FEED_DECREASE_BUTTON 5
#define MODE_SELECT_BUTTON 17
#define DIRECTION_BUTTON 16
#define ON_OFF_BUTTON 14

#define MAX_FEED_RATE 150

/*!!!INPUT BUTTON GLOBAL VARIABLES!!!*/
/*global variables for current feed positions and rate*/
int feed_rate = 1;
int tpi_current_selected = 23;
int metric_pitch_current_selected = 21;
/*direction - positive = forward, negative = backwards*/
int UI_direction = 1;

int last_display_refresh = 0;

int feed_up_hold = 0;
int feed_down_hold = 0;

int run_mode_debounce = 0;
int feed_direction_debounce = 0;
int on_off_debounce = 0;

int run_mode = FEED_MODE;

int UI_on_off = 1;

unsigned long button_check_refresh_millis = 0;


/*===============================================*/

/*!!!STEPPER DRIVER DEFINES!!!*/
#define STEPPER_DIRECTION_PIN 18
#define STEPPER_ENABLE_PIN 35
#define STEPPER_STEP_PIN 19

#define STEPPER_ACCELERATION 1000000

/*!!!STEPPER DRIVER GLOBAL VARIABLES!!!*/
//Carryover for when we need to move fractional steps, this way we don't lose accuracy
float running_steps_carryover = 0.0;

//Debugging, calculating the total number of steps moved to verify the steps being commanded are the correct steps
long total_steps_moved = 0;

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

/*===============================================*/

/*!!!ENCODER DEFINES!!!*/
#define ENCODER_PIN_A 33
#define ENCODER_PIN_B 32

/*!!!ENCODER GLOBAL VARIABLES!!!*/

/*!!!ENCODER!!!*/
ESP32Encoder encoder;

/*===============================================*/

/*!!!HIGH PRIORITY THREAD TASK!!!*/
/*!!!Task - setting it up so stepper and encoder are on one core, everything else is on the other core!!!*/
TaskHandle_t Task1;

long last_count = 0;

/*===============================================*/

/*!!!DEBUGGING VARIABLES!!!*/


//u_int64_t last_timer = 0;
//u_long calculations_per_second = 0;



void setup(){

  populateArrays();

  Serial.begin(115200);

  /*Input buttons*/
  pinMode(FEED_INCREASE_BUTTON, INPUT_PULLUP);
  pinMode(FEED_DECREASE_BUTTON, INPUT_PULLUP);
  pinMode(DIRECTION_BUTTON, INPUT_PULLUP);
  pinMode(ON_OFF_BUTTON, INPUT_PULLUP);
  pinMode(MODE_SELECT_BUTTON, INPUT_PULLUP);


  /*!!!STEPPER DRIVER!!!*/
  engine.init();
  stepper = engine.stepperConnectToPin(STEPPER_STEP_PIN,DRIVER_RMT);
  if (stepper) {
      stepper->setDirectionPin(STEPPER_DIRECTION_PIN);
      stepper->setAutoEnable(true);

      stepper->setSpeedInUs(5);  // the parameter is us/step !!!
      stepper->setAcceleration(STEPPER_ACCELERATION);
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
  encoder.attachFullQuad(ENCODER_PIN_A, ENCODER_PIN_B);

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
}

void high_priority_loop(void * parameter){
  for(;;) {
    u_int64_t cur_timer = esp_timer_get_time();
    
    //debugging, figuring out how many calculations per second can be achieved, this should not run in prod
    //calculations_per_second++;
    
    /*record current encoder counts to compare to previous counts to find distance travelled since last check*/
    int32_t cur_count = encoder.getCount();
    
    int counts_delta = cur_count - last_count;
    
    /*if we haven't moved, don't do anything, otherwise drive to new location*/
    if(counts_delta != 0 && UI_on_off == 1){
      /*get the numerical number of rotations since we last checked*/
      /*encoder is 600 p/r, with quadrature that's 2,400 counts per rotation.  This gives us a decimal representation of the percentage that the spindle has turned since we last checked*/
      /*2,400 counts per revolution on an 8 TPI leadscrew means a resolution of 0.000052 inches per encoder step, which is insane*/
      //float rotation = counts_delta/2400.0;
      
      /*Set the global last_rotation to this value, MAY NOT NEED*/
      //last_rotation = rotation;
      calculateStepsToMoveFLOAT(counts_delta);
      //stepper->move(counts_delta);
      //step_drives_per_second++;
    }
    /*Debugging, just outputting the calculations per second*/
    //if(cur_timer - last_timer > 1000000){
      //Serial.println(calculations_per_second);
      //Serial.println(step_drives_per_second);
    //  calculations_per_second = 0;
    //  step_drives_per_second = 0;
    //  last_timer = cur_timer;
    //}

    last_count = cur_count;

    /*TODO: Logic to calculate stepper position from encoder rotation percentage*/
  //  stepper->move(1);
  }
}

void calculateStepsToMoveFLOAT(int encoder_counts){

  float current_feed_rate = 0.0;
  if(run_mode == FEED_MODE){
    current_feed_rate = float(feed_rate);
  } else if(run_mode == TPI_MODE){
    current_feed_rate = inch_thou_per_rev[tpi_current_selected];
  } else if(run_mode == PITCH_MODE){
    current_feed_rate = metric_thou_per_rev[metric_pitch_current_selected];
  } else{
    current_feed_rate = 0.0;
  }

  int rotation_direction = 1;
  if(encoder_counts < 0){
    rotation_direction = -1;
  }
  encoder_counts = abs(encoder_counts);

  float rotation_proportion = encoder_counts / ENCODER_COUNTS_FULL_REV;
  float total_thou_to_move = 0.0;
  float final_steps = 0.0;

  total_thou_to_move = current_feed_rate * rotation_proportion;

  final_steps = total_thou_to_move / (LEADSCREW_THOU_PER_REV/STEPPER_STEPS_FULL_REV);

  /*Need to multiply by the final drive ratio BEFORE calculating carryover*/
  final_steps = final_steps * FINAL_DRIVE_RATIO;

  float current_carry_over = abs(final_steps) - abs((int)final_steps);
 

  if(current_carry_over + running_steps_carryover > 1.0){
    final_steps++;
    running_steps_carryover = current_carry_over + running_steps_carryover - 1.0;
  } else{
    running_steps_carryover = running_steps_carryover + current_carry_over;
  }
  moveStepperToPosition(final_steps*rotation_direction*UI_direction);

}

void moveStepperToPosition(long counts){
  total_steps_moved = total_steps_moved + counts;
  stepper->move(counts);
}

void lcdUpdate(){

  if(millis() - display_millis > 1000/LCD_REFRESH_RATE){
    display_millis = millis();

    String encoder_pos = String("E=" + String(String((int32_t)encoder.getCount())+"     S="+ String(total_steps_moved) + "          ").substring(0,20));

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
    float current_RPMs = rpm_delta/ENCODER_COUNTS_FULL_REV/seconds_since_last_update*60.0;
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

  String fwd_rev = String("FWD");
  String on_off = String("ON ");

  if(UI_direction == -1){
    fwd_rev = String("REV");
  }
  if(UI_on_off == -1){
    on_off = String("OFF");
  }


  if(run_mode == FEED_MODE){
    feed_string = String("Feed: 0.") +String(String("000") + String(feed_rate)).substring(String(feed_rate).length(),String(String("000") + String(feed_rate)).length()) + String("  " + String(on_off) + String(" ") + fwd_rev);
  } else if(run_mode == TPI_MODE){
    feed_string = String("TPI: ") + inch_TPIs[tpi_current_selected] + String("   ") + String(on_off) + String("   " + fwd_rev + "  ");
  } else if(run_mode == PITCH_MODE){
    feed_string = String("Pitch: ") + metric_pitches[metric_pitch_current_selected] + "  " + String(on_off) + " " + fwd_rev;
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
  if(millis() - button_check_refresh_millis > 1000/BUTTON_REFRESH_RATE){
    button_check_refresh_millis = millis();
    if(!digitalRead(FEED_INCREASE_BUTTON)){
        feed_hold_check(FEED_INCREASE_BUTTON);
    } else{
      feed_up_hold = 0;
    }
    if(!digitalRead(FEED_DECREASE_BUTTON)){
        feed_hold_check(FEED_DECREASE_BUTTON);
    } else{
      feed_down_hold = 0;
    }
    if(!digitalRead(MODE_SELECT_BUTTON)){
      run_mode_debounce++;
      if(run_mode_debounce == 2){
        if(run_mode == PITCH_MODE){
          run_mode = FEED_MODE;
        } else{
          run_mode++;
        }
      }
    } else{
      run_mode_debounce = 0;
    }
    if(!digitalRead(DIRECTION_BUTTON)){
      feed_direction_debounce++;
      if(feed_direction_debounce == 2){
        UI_direction = UI_direction * -1;
      }
    } else{
      feed_direction_debounce = 0;
    }
    if(!digitalRead(ON_OFF_BUTTON)){
        on_off_debounce++;
        if(on_off_debounce == 2){
          UI_on_off = UI_on_off * -1;
        }
      } else{
        on_off_debounce = 0;
      }
    }
  }

void feed_hold_check(int pin){
  if(pin == FEED_INCREASE_BUTTON){
    feed_up_hold++;
    if(feed_up_hold == 1 || feed_up_hold > BUTTON_REFRESH_RATE){
      if(run_mode == FEED_MODE && feed_rate < 150){
        feed_rate++;
      } else if(run_mode == TPI_MODE && tpi_current_selected < 23){
        tpi_current_selected++;
      } else if(run_mode == PITCH_MODE && metric_pitch_current_selected < 20){
        metric_pitch_current_selected++;
      } else{
        //do nothing
      }
      
    }
  }
  if(pin == FEED_DECREASE_BUTTON){
    feed_down_hold++;
    if(feed_down_hold == 1 || feed_down_hold > BUTTON_REFRESH_RATE){
      if(run_mode == FEED_MODE && feed_rate > 1){
        feed_rate--;
      } else if(run_mode == TPI_MODE && tpi_current_selected > 0){
        tpi_current_selected--;
      } else if(run_mode == PITCH_MODE && metric_pitch_current_selected > 0){
        metric_pitch_current_selected--;
      } else{
        //do nothing
      }
    }
  }
}

