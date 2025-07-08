#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif

#define CORE_DEBUG_LEVEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG


#include <LiquidCrystal_I2C.h>
#include <ESP32Encoder.h>
#include "FastAccelStepper.h"
#include <Arduino.h>
#include "config.h"
#include "tables.h"
#include <esp32-hal-log.h>


/*!!!MODE DEFINES!!!*/
#define FEED_MODE 0
#define TPI_MODE 1
#define PITCH_MODE 2

/*!!!Running Mode!!!*/
/*I have two modes, STEPS_MODE which counts the encoder pulses as quickly as possible
  and then calculates the number of STEPS it has to move, and SPEED_MODE which instead
  calculates the current rotational speed of the spindle and calculates the microseconds per step
  for that speed.  STEP_MODE is nominally more accurate, but it seems like it causes some vibration/judder at lower
  speeds.  SPEED_MODE is less accurate (it can't handle extremely slow rotations) but seems to make the machine
  much happier*/
int steps_mode = 1;
int steps_hold = 0;

int zero_speed_counter = 0;

/*!!!UI GLOBAL VARIABLES!!!*/

// set the LCD number of columns and rows
int lcdColumns = 20;
int lcdRows = 4;

/*To calculate RPMs, we need to store the previous encoder count and compare it to the current encoder count, and compare that to how much time has passed*/
int64_t rpm_display_last_count = 0;
u_int64_t display_last_updated_time = esp_timer_get_time();

/*Store the last time the display was updated so we can run at the desired refresh rate*/
unsigned long display_millis = 0;


LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

/*Defining the char arrays that will be popualted/compared against for the LCD*/
char current_LCD_line_1[20];
char current_LCD_line_2[20];
char current_LCD_line_3[20];
char current_LCD_line_4[20];

/*===============================================*/

/*These are the directions from the UI button.  The servo driver is aware of the spindle direction, but if you want the leadscrew to
  spin the opposite way you can set that via the UI via the FWD/REV button*/
#define UI_DIRECTION_FORWARD 1
#define UI_DIRECTION_BACKWARD -1

/*!!!INPUT BUTTON GLOBAL VARIABLES!!!*/

/*global variables for current feed positions and rate.  Want to set the defaults to the slowest feed in case you start with it engaged and then run at 2000 RPMs or something*/
/*The current scheme lets you do down to 1 thou per rev, if I can't imagine why you'd ever need less than that*/
/*2023-12-24 I can now imagine why you'd need less than that!  For parting it can be nice to go down even lower than 1 thou per rev, so I'm changing this to a float*/
float feed_rate = 1;
int tpi_current_selected = 0;
int metric_pitch_current_selected = 0;

/*direction - positive = forward, negative = backwards*/
int UI_direction = UI_DIRECTION_FORWARD;

/*If the feed up/down buttons are held, pause for a bit and then repidly traverse the arrays*/
int feed_up_hold = 0;
int feed_down_hold = 0;
unsigned long button_check_refresh_millis = 0;

/*Manual debouncing, since this UI updates infrequently and isn't very responsive, I just manually did the debounce delays*/
int run_mode_debounce = 0;
int feed_direction_debounce = 0;
int on_off_debounce = 0;

/*Defaulting to FEED MODE, can be changed if a different default mode is desired*/
int run_mode = FEED_MODE;

/*The UI allows you to stop the servo via the ON/OFF button*/
int UI_on_off = 1;

/*===============================================*/

/*!!!STEPPER DRIVER GLOBAL VARIABLES!!!*/
/*
  Carryover for when we need to move fractional steps.  The program calculates the number of steps to move as often as possible.  There is no
  rate limiting anywhere.  Thus, especially with low RPMs and low feeds, there are many times where the encoder counts read since the last time call for
  a fraction of a step (like 0.5234 steps).  This is a problem because servos/steppers can't move more than a full step.  Cranking up microstepping won't fix this
  either because you will ALWAYS have some sort of fractional step.  Instead, what I do is keep a running total of the carryover steps, and once that running total
  gets larger than 1 step, then I increment the number of steps to move and decrement the running total.  For example:

  Check 1 - 0.245 steps (add to carryover, 0 + 0.245 = 0.245.  0.245 < 1, so do nothing)
  Check 2 - 0.425 steps (less than 1, add to carryover, 0.245 + 0.425 = 0.670.  0.670 < 1, so do nothing)
  Check 3 - 0.400 steps (less than 1, add to carryover, 0.670 + 0.400 = 1.070.  1.070 > 1, so increment the number of steps called for by 1.  Running carryover now = 1.070 - 1.0 = 0.070)

  This way, you will never be any more than 1 commanded step away from where you should be.  In my case, with a 8TPI leadscrew and 2400 count encoder/servo, that equals out to aroung 50 microinches of error

*/
float running_steps_carryover = 0.0;

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

/*===============================================*/

/*!!!ENCODER!!!*/
ESP32Encoder encoder;
u_int64_t last_timer = 0;

/*===============================================*/

/*!!!HIGH PRIORITY THREAD TASK!!!*/
/*!!!Task - setting it up so stepper and encoder are on one core, everything else is on the other core!!!*/
TaskHandle_t stepper_and_encoder_driver;

long last_count = 0;

/*===============================================*/

/*!!!DEBUGGING VARIABLES!!!*/


//u_int64_t last_timer = 0;
//u_long calculations_per_second = 0;



void setup(){

  Serial.begin(115200);

  //In tables.h, populate arrays with hardcoded TPIs and metric thread pitches
  populateArrays();

  /*Input button modes set*/
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

      //for my server, the us/step max is 300khz or 3.33 us/step.  I set mine to 5 for a safety margin, meaning I can generate up to 200,000 steps/sec
      //which is FAR beyond what this project would ever need to do
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

  /*
  CAN ATTACH MULTIPLE ENCODERS TO FIX PCNT/RMT ISSUE
  There is a bug where if you have two different devices connected to the ESP PCNT pulse counter, it gets
  confused and they fight with each other.  There are two solutions, once is to use RMT for the stepper driver,
  and the other is to define two encoders and use the second one.  I opted for the RMT fix as it seems to work OK
  */
  
  //Uncomment this to run two encoders to fix the stupid PCNT library thing
  //encoder0.attachFullQuad(17, 16);
  
  encoder.attachFullQuad(ENCODER_PIN_A, ENCODER_PIN_B);

  /*!!!LCD!!!*/
  // initialize LCD
  lcd.init();
  // turn on LCD backlight                      
  lcd.backlight();



  /*!!!TASK!!!*/
  xTaskCreatePinnedToCore(
    high_priority_loop, /* The realtime loop that calculates stepper pulses and reads the encoder.  It is on its own CPU core so it never gets delayed/blocked*/
    "Encoder and Stepper", /* Name of the task */
    1000,  /* Stack size in words */
    NULL,  /* Task input parameter */
    0,  /* Priority of the task */
    &stepper_and_encoder_driver,  /* Task handle. */
    0); /* Core where the task should run */


}

void loop(){
  //This all runs on CPU1
  /*UI updates - check the buttons and set the UI values*/
  buttonCheck();
  
  /*Update the display, including calculating things like RPM, thread pitch, feed rate, etc*/
  lcdUpdate();
}

void high_priority_loop(void * parameter){
  for(;;) {
    u_int64_t cur_timer = esp_timer_get_time();
    int microseconds = cur_timer - last_timer;
    /*SPEED_MODE operates on a timer and updates the speed some number of times a second, whereas STEP_MODE updates as often as possible*/
    if (microseconds > SPEED_MODE_REFRESH_MICROSECONDS || steps_mode == 1){
      //debugging, figuring out how many calculations per second can be achieved, for performance reasons this should not run in prod
      //calculations_per_second++;
      
      /*record current encoder counts to compare to previous counts to find distance travelled since last check*/
      int64_t cur_count = encoder.getCount();
      

      /*
      Does not handle rollovers, however.  Encoder.getcount() returns a 64-bit int, meaning that it can count up to
      9,223,372,036,854,775,807 before rolling over.  At 5,000 RPMs, and 2,400 counts/rev, that means that the encoder
      can run for approximately 1.4 million years without ever overflowing.
      */
      int counts_delta = cur_count - last_count;
      
      
      //Only run either STEPS_MODE or SPEED_MODE
      if(steps_mode){
        /*if we haven't moved, or the UI has commanded the servo to be OFF, don't do anything, otherwise drive to new location*/
        if(counts_delta != 0 && UI_on_off == 1){
          calculateStepsToMove(counts_delta);
        }
      } else{
        if(counts_delta != 0 && UI_on_off == 1){  
          /*any movement should reset the zero speed counter*/
          zero_speed_counter = 0;
          calculateSpeedToMove(counts_delta, microseconds);
        }
        else{
          //If we haven't had any encoder movement, we need to start seeing if the lathe is truly stopped.  If so, then accelerate the stepper to a stop
          zero_speed_counter++;
          if(zero_speed_counter > ZERO_SPEEDS_TO_STOP){
            stepper->moveByAcceleration(-10000,false);
          }
        }
      }
      /*Debugging, just outputting the calculations per second*/
      /*if(cur_timer - last_timer > 1000000){
        Serial.println(calculations_per_second);
        Serial.println(step_drives_per_second);
        calculations_per_second = 0;
        step_drives_per_second = 0;
        last_timer = cur_timer;
      }*/

      last_count = cur_count;
      last_timer = cur_timer;
    }
  }
}
void calculateSpeedToMove(int encoder_counts, u_int64_t microseconds){

  /*The basic idea here is to calculate the speed (in rotations per second) that the spindle is moving, and then
    use this speed to tell the stepper what speed it should move.  Instead of moving by steps, we instead update the 
    speed at which the stepper should be moving each time.  This way, instead of sending huge bursts of steps to the
    stepper, we're only sending small speed updates.  This seems to help with low-speed jitter, and appears to be
    plenty accurate*/

  float current_feed_rate = 0.0;

  //Set the current feed rate to be in thou/rev for any mode we're in

  if(run_mode == FEED_MODE){
    current_feed_rate = feed_rate;
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


  float rotation_proportion = encoder_counts / ENCODER_COUNTS_FULL_REV / ENCODER_FINAL_DRIVE_RATIO;
  
  /*this is the speed in rotations per second of the spindle.  We want to convert this into rotations per second, so we divide the microseconds by 1 million*/
  float current_rotations_per_second = rotation_proportion/(microseconds/1000000.0);

  /*Calculate the feed rate (in thou per second).  To do this, take the current spindle rotations per second, and multiply this by the feed rate in thou.  This gives the total number of thou 
    that the leadscrew should drive the carraige in one second at the current spindle speed*/
  float thou_per_second_feed = current_rotations_per_second * current_feed_rate;
  
  //now we need to turn that into uS per step
  //first, calculate exactly how many steps that feed rate will be for 1 second, making sure to
  //correct for the leadscrew having a reduction
  float stepper_steps_to_move_in_one_second = STEPPER_STEPS_FULL_REV*(thou_per_second_feed/LEADSCREW_THOU_PER_REV);

  /*Now, take that number and figure out how many microseconds per step that is.  For instance, if your spindle is spinning at 1 RPS,
    and you want to feed at 125 thou per rev, then you need to command a full revolution in 1 second.  This is STEPPER_STEPS_PER_REV (for example, 500)
    steps, so you need to divide 1,000,000 (microseconds in a second) buy that value, and then you'll get the microseconds_per_step which is 1,000,000/500 = 2,000 microseconds per step*/
  float usec_per_step = 1000000.0/stepper_steps_to_move_in_one_second;

  /*Set the stepper usec_per_step to this value, corrected for your final drive ratio*/
  stepper->setSpeedInUs(usec_per_step/FINAL_DRIVE_RATIO_FLOAT);
  stepper->moveByAcceleration(STEPPER_ACCELERATION*UI_direction*rotation_direction);
}

void calculateStepsToMove(int encoder_counts){


  /*
  This is the big one, where we calculate the number of steps to move based on the
  delta of encoder counts since we last checked.  We first need to check what mode we're in (feed vs pitch/TPI)
  And then figure out how many steps to move.  I decided to go with converting everything into thou/rev, and also
  proportion of rotation.  So if the encoder moves 1 step, then we have 1/2400th of a revolution.  From there, we 
  can use the leadscrew TPI to figure out how much to actually turn the servo to get that amount of movement

  Note the ESP is so incredibly fast that you very rarely get more than a single encoder pulse at a time, but
  this logic works for any amount of time between encoder reads (although the longer you go between reads, the
  more behind your servo will be until the next read occurs)

  */


  /*
    Using floats for everything here as we need to keep track of decimals, and while floats are slower than whole numbers
    it's not enough to matter.  Furthermore, while floats do have some inbuilt inaccuracy, the high number of counts per rev
    and microsteps means that these inaccuracies are in the microinches
  */

    /*Note, this is distinct from the UI variable, this is simply making sure the servo turns in lock step with the spindle direction*/
  int rotation_direction = 1;
  if(encoder_counts < 0){
    rotation_direction = -1;
  }

  encoder_counts = abs(encoder_counts);

  float current_feed_rate = 0.0;

  //Set the current feed rate to be in thou/rev for any mode we're in

  if(run_mode == FEED_MODE){
    current_feed_rate = feed_rate;
  } else if(run_mode == TPI_MODE){
    current_feed_rate = inch_thou_per_rev[tpi_current_selected];
  } else if(run_mode == PITCH_MODE){
    current_feed_rate = metric_thou_per_rev[metric_pitch_current_selected];
  } else{
    current_feed_rate = 0.0;
  }


  /*Since we've determined the direction, we can use the absolute value now to make our other calculations less complicated*/
  /*20231205 - actually this was dumb, we want to be aware of the current rotation direction to allow fwd/reverse threading, so don't do this*/
  /*encoder_counts = abs(encoder_counts);*/


  //Calculating the proportion of a full revolution of the encoder sinced we last checked.  Since my servo has the same number of
  //microsteps as my encoder it's 1:1, however changing the defines at the top allows you to use any encoder/microstep combo
  float rotation_proportion = encoder_counts / ENCODER_COUNTS_FULL_REV / ENCODER_FINAL_DRIVE_RATIO;
  float total_thou_to_move = 0.0;
  float final_steps = 0.0;

  /*
    To find the total number of thou to move, we take the current feed rate in thou/rev, and multiply it by the proportion of
    a full revolution we've made. So if we made 1/2 a turn, and we want to move 50 thou per rev, we now need to move 25 thou (50 thou/rev * .5 revs)
  */
  total_thou_to_move = current_feed_rate * rotation_proportion;

  //Calculating the number of steps that distance translates into.
  final_steps = total_thou_to_move / (LEADSCREW_THOU_PER_REV/STEPPER_STEPS_FULL_REV);

  /*Need to multiply by the final drive ratio BEFORE calculating carryover*/
  final_steps = final_steps * FINAL_DRIVE_RATIO;

  //This way our carryover is always positive, so we can just subtract 1 from it every time to reset it
  //Casting it to an int to find the remaining decimal
  float current_carry_over = abs(final_steps) - abs((int)final_steps);
 


  //If our carryover is over 1, then increase the steps commanded to 'catch up'
  //otherwise, store the carryover so you can check it next time
  if(current_carry_over + running_steps_carryover > 1.0){
    final_steps++;
    running_steps_carryover = current_carry_over + running_steps_carryover - 1.0;
  } else{
    running_steps_carryover = running_steps_carryover + current_carry_over;
  }
  moveStepperToPosition(final_steps*rotation_direction*UI_direction);

}

void moveStepperToPosition(long counts){
  //FastAccelStepper makes this so easy, it feels kind of silly to have this as its own function but hey
  stepper->move(counts);
}

void lcdUpdate(){

  /*The RPM and SFM parts update constantly, so we only want to refresh them on a schedule (otherwise they get all smeary)*/
  if(millis() - display_millis > 1000/LCD_REFRESH_RATE){

    /*first line PROD*/
    //First line displays the program and 
    //Version and name, only need to run a single time
    String version = "ESESPELS v.91    STP";
    if(display_millis == 0){
      lcdLineUpdate(0, version, current_LCD_line_1);
    }
    display_millis = millis();

    //I know strings are evil but this section is not time sensitive and I don't care

    /*
      DEBUGGING - uncomment if you want the first line to show the encoder steps and the stepper steps.  This is useful for
      making sure that your math is right and the code is commanding the correct number of steps per encoder.  An easy test
      is to turn it on, set the feed rate to some number, and then manually spin the spindle one full revolution using the
      E (encoder) number.  Then see the commanded steps with the S (steps) number.  This will help you determine if you're
      missing steps due to a lack or torque, or if the steps are correct and the math is just wrong somewhere
     */
    //String encoder_pos = String("E=" + String(String((int32_t)encoder.getCount())+"     S="+ String(total_steps_moved) + "          ").substring(0,20));

    /*first line DEBUGGING*/
    
    //for(int i = 0; i < 20; i++){
    //  if (encoder_pos.charAt(i) != current_LCD_line_1[i]){
    //    lcd.setCursor(i,0);
    //    lcd.print(encoder_pos[i]);
    //    current_LCD_line_1[i] = encoder_pos.charAt(i);
    //  }
    //}
    
    
    /*second line*/
    /*
      This line calculates the RPMs currently being read by the encoder.  This is very inefficient but
      since this has basically an entire CPU core to itself, it can take forever and not mess
      with the step generation
    */

    //We need the current encoder count and the ESP microseconds time to figure out how long it's been since we
    //last calculated the RPMs
    int64_t rpm_display_current_count = encoder.getCount();
    int64_t display_current_update_time = esp_timer_get_time();
    int64_t rpm_delta = rpm_display_current_count - rpm_display_last_count;


    int64_t micros_since_last_update = display_current_update_time - display_last_updated_time;


    //There are 1,000,000 microseconds in a second, so figure out exactly how many seconds it's been since the
    //last time we read the encoder
    float seconds_since_last_update = micros_since_last_update/1000000.0;

    //The actual RPM calculation.  I measured with an RPM detection device and it was within 1%
    float current_RPMs = rpm_delta/ENCODER_COUNTS_FULL_REV/ENCODER_FINAL_DRIVE_RATIO/seconds_since_last_update*60.0;
    String RPMs_current = "RPMs = " + String(String(current_RPMs)+String("                    ")).substring(0,13);
    rpm_display_last_count = rpm_display_current_count;
    display_last_updated_time = display_current_update_time;
    lcdLineUpdate(1, RPMs_current, current_LCD_line_2);

    
    /*third line*/
    String sfm_current = "SFM @ 1in: "+ String(String(current_RPMs/3.82) + String("            ")).substring(0,10);

    lcdLineUpdate(2, sfm_current, current_LCD_line_3);
  }
  
  /*The feed string is updated only from the UI, so we want this to update as often as possible*/

  String feed_string = "";
  /*fourth line*/

  String fwd_rev = String("FWD");
  String on_off = String("ON ");

  if(UI_direction == UI_DIRECTION_BACKWARD){
    fwd_rev = String("REV");
  }
  if(UI_on_off == -1){
    on_off = String("OFF");
  }


  if(run_mode == FEED_MODE){
    /*To deal with the display of sub-thou feed oer revs*/
    int feed_rate_int = int(feed_rate);
    if(feed_rate >= 1){
      feed_string = String("Feed: 0.") +String(String("000") + String(feed_rate_int)).substring(String(feed_rate_int).length(),String(String("000") + String(feed_rate_int)).length()) + String("  " + String(on_off) + String(" ") + fwd_rev);
    } else{
      feed_string = String("Feed: 0.000") + String(String(feed_rate_int*10))+ String(" " + String(on_off) + String(" ") + fwd_rev);
    }
  } else if(run_mode == TPI_MODE){
    feed_string = String("TPI: ") + inch_TPIs[tpi_current_selected] + String("    ") + String(on_off) + String(" " + fwd_rev + "  ");
  } else if(run_mode == PITCH_MODE){
    feed_string = String("Pitch: ") + metric_pitches[metric_pitch_current_selected] + "  " + String(on_off) + " " + fwd_rev;
  } else{
    //do nothing
  }

  lcdLineUpdate(3, feed_string, current_LCD_line_4);
}

void lcdLineUpdate(int lcd_line, String &feed_string, char line_to_write[]){
  /*
    Does the actual LCD updating.  The LCD module I chose is super duper slow, so this
    only updates characters that have changed, which keeps the display from being a
    smeary mess
  */
  for(int i = 0; i < lcdColumns; i++){
    if (feed_string.charAt(i) != line_to_write[i]){
      lcd.setCursor(i,lcd_line);
      lcd.print(feed_string[i]);
      line_to_write[i] = feed_string.charAt(i);
    }
  }
}

void buttonCheck(){
  //Checking if buttons have been pressed/held in, and what to do about it
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
      steps_hold++;
      /*Hold mode for 3 seconds to change from STEPS to SPEED mode*/
      if(steps_hold > BUTTON_REFRESH_RATE*3){
        if(steps_mode == 0){
          steps_mode = 1;
          String version = "ESESPELS v.91    STP";
          /*reset everything to default/zero when we switch modes*/
          stepper->setAcceleration(STEPPER_ACCELERATION);
          encoder.setCount(0);
          lcdLineUpdate(0, version, current_LCD_line_1);
          steps_hold=0;
        } else{
          steps_mode = 0;
          String version = "ESESPELS v.91    SPD";
          encoder.setCount(0);
          lcdLineUpdate(0, version, current_LCD_line_1);
          steps_hold=0;
        }
      }
      run_mode_debounce++;
      if(run_mode_debounce == 2){
        if(run_mode == PITCH_MODE){
          run_mode = FEED_MODE;
        } else{
          run_mode++;
        }
      }
    } else{
      steps_hold = 0;
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
  //Bounds checking for feed holds so that we never command a feed greater than what we have allowed
  if(pin == FEED_INCREASE_BUTTON){
    feed_up_hold++;
    if(feed_up_hold == 1 || feed_up_hold > BUTTON_REFRESH_RATE){
      if(run_mode == FEED_MODE && feed_rate < MAX_FEED_RATE){
        if (feed_rate >= 1){
          feed_rate++;
        } else{
          feed_rate = feed_rate + FRACTIONAL_FEED;
        }
      } else if(run_mode == TPI_MODE && tpi_current_selected > 0){
        tpi_current_selected--;
      } else if(run_mode == PITCH_MODE && metric_pitch_current_selected < sizeof(metric_pitches)/sizeof(metric_pitches[0]) - 1){
        metric_pitch_current_selected++;
      } else{
        //do nothing
      }
      
    }
  }
  if(pin == FEED_DECREASE_BUTTON){
    feed_down_hold++;
    if(feed_down_hold == 1 || feed_down_hold > BUTTON_REFRESH_RATE){
      if(run_mode == FEED_MODE && feed_rate >= 0.1){
        if(feed_rate > 1){
          feed_rate--;
        } else{
          feed_rate = feed_rate - FRACTIONAL_FEED;
        }
        
      } else if(run_mode == TPI_MODE && tpi_current_selected < sizeof(inch_TPIs)/sizeof(inch_TPIs[0]) - 1){
        tpi_current_selected++;
      } else if(run_mode == PITCH_MODE && metric_pitch_current_selected > 0){
        metric_pitch_current_selected--;
      } else{
        //do nothing
      }
    }
  }
}

