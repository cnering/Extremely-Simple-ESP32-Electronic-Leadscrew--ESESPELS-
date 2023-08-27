String metric_pitches[22] = {};
float metric_thou_per_rev[22] = {};
String inch_TPIs[24] = {};
float inch_thou_per_rev[24] = {};

  /*Defining all the metric pitches for the UI*/

  void populateArrays(){

  /*Defining metric picthes for the UI display*/
  
  metric_pitches[0] = "0.25";
  metric_pitches[1] = "0.30";
  metric_pitches[2] = "0.35";
  metric_pitches[3] = "0.40";
  metric_pitches[4] = "0.45";
  metric_pitches[5] = "0.50";
  metric_pitches[6] = "0.60";
  metric_pitches[7] = "0.70";
  metric_pitches[8] = "0.80";
  metric_pitches[9] = "1.00";
  metric_pitches[10] = "1.25";
  metric_pitches[11] = "1.50";
  metric_pitches[12] = "1.75";
  metric_pitches[13] = "2.00";
  metric_pitches[14] = "2.50";
  metric_pitches[15] = "3.00";
  metric_pitches[16] = "3.50";
  metric_pitches[17] = "4.00";
  metric_pitches[18] = "4.50";
  metric_pitches[19] = "5.00";
  metric_pitches[20] = "5.50";
  metric_pitches[21] = "6.00";

/*Defining metric pitches in thou*/

  metric_thou_per_rev[0] = 9.842525;
  metric_thou_per_rev[1] = 11.81103;
  metric_thou_per_rev[2] = 13.779535;
  metric_thou_per_rev[3] = 15.74804;
  metric_thou_per_rev[4] = 17.716545;
  metric_thou_per_rev[5] = 19.68505;
  metric_thou_per_rev[6] = 23.62206;
  metric_thou_per_rev[7] = 27.55907;
  metric_thou_per_rev[8] = 31.49608;
  metric_thou_per_rev[9] = 39.3701;
  metric_thou_per_rev[10] = 49.212625;
  metric_thou_per_rev[11] = 59.05515;
  metric_thou_per_rev[12] = 68.897675;
  metric_thou_per_rev[13] = 78.7402;
  metric_thou_per_rev[14] = 98.42525;
  metric_thou_per_rev[15] = 118.1103;
  metric_thou_per_rev[16] = 137.79535;
  metric_thou_per_rev[17] = 157.4804;
  metric_thou_per_rev[18] = 177.16545;
  metric_thou_per_rev[19] = 196.8505;
  metric_thou_per_rev[20] = 216.53555;
  metric_thou_per_rev[21] = 236.2206;

  /*Defining all inch TPIs for the UI*/

  inch_TPIs[0] = "8";
  inch_TPIs[1] = "9";
  inch_TPIs[2] = "10";
  inch_TPIs[3] = "11";
  inch_TPIs[4] = "11.5";
  inch_TPIs[5] = "12";
  inch_TPIs[6] = "13";
  inch_TPIs[7] = "14";
  inch_TPIs[8] = "16";
  inch_TPIs[9] = "18";
  inch_TPIs[10] = "19";
  inch_TPIs[11] = "20";
  inch_TPIs[12] = "24";
  inch_TPIs[13] = "26";
  inch_TPIs[14] = "27";
  inch_TPIs[15] = "28";
  inch_TPIs[16] = "32";
  inch_TPIs[17] = "36";
  inch_TPIs[18] = "40";
  inch_TPIs[19] = "44";
  inch_TPIs[20] = "48";
  inch_TPIs[21] = "56";
  inch_TPIs[22] = "64";
  inch_TPIs[23] = "80";


/*Defining inch TPI thou per revs*/

  inch_thou_per_rev[0] =	125;
  inch_thou_per_rev[1] =	111.1111111;
  inch_thou_per_rev[2] =	100;
  inch_thou_per_rev[3] =	90.90909091;
  inch_thou_per_rev[4] =	86.95652174;
  inch_thou_per_rev[5] =	83.33333333;
  inch_thou_per_rev[6] =	76.92307692;
  inch_thou_per_rev[7] =	71.42857143;
  inch_thou_per_rev[8] =	62.5;
  inch_thou_per_rev[9] =	55.55555556;
  inch_thou_per_rev[10] =	52.63157895;
  inch_thou_per_rev[11] =	50;
  inch_thou_per_rev[12] =	41.66666667;
  inch_thou_per_rev[13] =	38.46153846;
  inch_thou_per_rev[14] =	37.03703704;
  inch_thou_per_rev[15] =	35.71428571;
  inch_thou_per_rev[16] =	31.25;
  inch_thou_per_rev[17] =	27.77777778;
  inch_thou_per_rev[18] =	25;
  inch_thou_per_rev[19] =	22.72727273;
  inch_thou_per_rev[20] =	20.83333333;
  inch_thou_per_rev[21] =	17.85714286;
  inch_thou_per_rev[22] =	15.625;
  inch_thou_per_rev[23] =	12.5;

  }


  /*OLD THING*/
/*
void calculateStepsToMove(int encoder_counts){
  //encoder pulses 2400 times per rev
  //leadscrew moves .125" per rev
  //each step moves the saddle .00005208 inches

  //multiply everything by 1,000,000, that gives us 52 micro-inches per step

  //if we do everything in microinches, we don't need to mess with floats maybe

  //first, get the thou per rotation
  //then, convert it to microinches

  //find the number of millisteps so we can ceep track of fractional steps without float imprecision
  int milli_steps = encoder_counts * 1000;

  //1 thou per revolution = 1 thou*8

  //distance per revolution

  //2400 at 1:1 is .125
  //2400 at X is 0.001

  //if matched, then it works out to 125 thou per rotation

  //2400 x RATIO = .125 * 2400
  //2400 x .125 * RATIO(.125/.125) = 300
  //2400 x .125 * (.001/.125) = 2.4 steps

  float final_ratio = (feed_rate/125000.0);

  long long milli_steps_needed = 2400000 * 125 * feed_rate*1000/125000*milli_steps;
  int milli_steps_remainder = milli_steps_needed%1000;

  //Serial.println(feed_rate);
  //Serial.println(milli_steps);
  //Serial.println(milli_steps_needed);
  //Serial.println(milli_steps_remainder);

  if(milli_steps_remainder != 0){
    if(milli_steps_remainder + milli_steps_carryover > 1000){
      milli_steps_needed = milli_steps_needed + 1000;
      milli_steps_carryover = milli_steps_carryover + milli_steps_remainder - 1000;
    } else{
      milli_steps_carryover = milli_steps_carryover + milli_steps_remainder;
    }
  }




  //2400 at 1 thou = (.001/8) / 2400

  //long microinches_of_travel_needed = (feed_rate * 1000 * encoder_counts)/2400*LEADSCREW_TPI;

  //int steps_from_travel_needed = microinches_of_travel_needed/52;

  //Serial.println("encoder = " + encoder_counts);
  //Serial.println("microinches needed = " + microinches_of_travel_needed);
  //Serial.println("steps calculated = " + steps_from_travel_needed);

  //next, figure out how many microinches we need to move

  long steps_from_travel_needed = milli_steps_needed / 1000;

  moveStepperToPosition(steps_from_travel_needed);

  //delay(100);

}
*/