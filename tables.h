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
/*Conversion from mm to thou from google, 39.3701*/

  for(int i = 0; i < sizeof(metric_pitches)/sizeof(metric_pitches[0]); i++){
    metric_thou_per_rev[i] = metric_pitches[i].toFloat()*39.3701;
  }

  /*Defining all inch TPIs for the UI*/

  inch_TPIs[0] = "80  ";
  inch_TPIs[1] = "64  ";
  inch_TPIs[2] = "56  ";
  inch_TPIs[3] = "48  ";
  inch_TPIs[4] = "44  ";
  inch_TPIs[5] = "40  ";
  inch_TPIs[6] = "36  ";
  inch_TPIs[7] = "32  ";
  inch_TPIs[8] = "28  ";
  inch_TPIs[9] = "27  ";
  inch_TPIs[10] = "26  ";
  inch_TPIs[11] = "24  ";
  inch_TPIs[12] = "20  ";
  inch_TPIs[13] = "19  ";
  inch_TPIs[14] = "18  ";
  inch_TPIs[15] = "16  ";
  inch_TPIs[16] = "14  ";
  inch_TPIs[17] = "13  ";
  inch_TPIs[18] = "12  ";
  inch_TPIs[19] = "11.5";
  inch_TPIs[20] = "11  ";
  inch_TPIs[21] = "10  ";
  inch_TPIs[22] = "9   ";
  inch_TPIs[23] = "8   ";


/*Defining inch TPI thou per revs*/

  for(int i = 0; i < sizeof(inch_thou_per_rev)/sizeof(inch_thou_per_rev[0]); i++){
    inch_thou_per_rev[i] = 1000.0/inch_TPIs[i].toFloat();
  }
}