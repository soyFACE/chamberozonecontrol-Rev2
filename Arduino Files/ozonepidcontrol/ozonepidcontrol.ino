#include "DFRobot_GP8403.h"
DFRobot_GP8403 dac(&Wire,0x5F);


const int OzonePin = A0;
const int ozone_pin_alternate = A3;
const int ballast_power_relay_pin = 12;
const int ballast_manual_on_sense_pin = 4;
const int ballast_auto_sense_pin = 2;
const int bulb_intensity_manual_sense_pin = 7;
const int bulb_intensity_auto_sense_pin = 8;
//const int BulbPin = 9;
const int CYCLETIME = 4000;
float setpoint = 150;
float process_value;
float process_value_alternate;
float error = 0;
float last_error = 0;
float ozonator_temp = 0;
float ozonator_light_intensity = 0;
int door_sense = 0;

//Find how to store these in non-volatile memory
float kp = 0.0600;//.045; // For the full-size chamber 5V reached at least 500-690. ~690/5/2 //.010;// V output stabilized at ~190ppb 190/5 = 38. This is what the tutorials say, but shouldn't it be 5/190? //.032;
float ki = 0.0002/CYCLETIME;  //.0002/CYCLETIME; //.006/CYCLETIME;  //.0025/CYCLETIME;
float kd = 0.0899*CYCLETIME; //.01*CYCLETIME; //.20*CYCLETIME;  //.08*CYCLETIME;

float Pcomponent = 0;
float Icomponent = 1.32; // tweak the initial I term to help things stabilize sooner.
float Dcomponent = 0;
float vout_in_volts;

int OZONE_ON = 0;
int RELAY_STATE = 0;
int BALLAST_MANUAL = 0;
int BALLAST_AUTO = 0;
int BULB_MANUAL = 0;
int BULB_AUTO = 0;
int STATE = 0;


float DACout = 0;
int DFRout = 0;
int OZONEGAIN = 250;
int CO2GAIN = 2000;
bool FIRST_CYCLE = true;


float last_time = millis();
float this_time;
float elapsed_time;

//COMMUNICATION GLOBALS
const byte numChars = 32;
char receivedChars[numChars];
const char *delim = " ,:/"; 
boolean newData = false;

void setup() {
  pinMode(ballast_power_relay_pin,OUTPUT);
  pinMode(ballast_manual_on_sense_pin, INPUT_PULLUP);
  pinMode(ballast_auto_sense_pin, INPUT_PULLUP);
  pinMode(bulb_intensity_manual_sense_pin, INPUT_PULLUP);
  pinMode(bulb_intensity_auto_sense_pin, INPUT_PULLUP);
  digitalWrite(ballast_power_relay_pin,0);
  Serial.begin(9600);
  //Serial.println("<Arduino is ready>");
  //pinMode(BulbPin,OUTPUT);
  //analogWrite(BulbPin,0);
  while(dac.begin()!=0){
    Serial.println("init error");
    delay(1000);
  }
  Serial.println("<Arduino is ready>");
  //Set DAC output range
  dac.setDACOutRange(dac.eOutputRange10V);
  dac.setDACOutVoltage(0,0);
  dac.setDACOutVoltage(10000,1);
}

void loop() {
control_loop();
recvWithStartEndMarkers();
}

void control_loop(){
  this_time = millis();
  elapsed_time = this_time - last_time;

  if(FIRST_CYCLE & elapsed_time >= CYCLETIME){
    //analogWrite(BulbPin,0);
    dac.setDACOutVoltage(0,0);
    BALLAST_MANUAL = !digitalRead(ballast_manual_on_sense_pin);
    BALLAST_AUTO = !digitalRead(ballast_auto_sense_pin);
    BULB_MANUAL = !digitalRead(bulb_intensity_manual_sense_pin);
    BULB_AUTO = !digitalRead(bulb_intensity_auto_sense_pin);
    last_time = this_time;
    process_value = analogRead(OzonePin);
    process_value = process_value/1023*OZONEGAIN;
    process_value_alternate = analogRead(ozone_pin_alternate);
    process_value_alternate = process_value_alternate/1023*CO2GAIN;
    error = setpoint - process_value;
    Pcomponent = error * kp * abs(error/setpoint);
    Icomponent += error * elapsed_time * ki * OZONE_ON;
    //if(Icomponent >=2){Icomponent = 2;}
    //if(Icomponent <= -.2){Icomponent = -0.2;}
    Dcomponent = (error - last_error) / elapsed_time * kd; // is there a way to make this hold the last value if the process value hasn't updated? Maybe averaging the current and last value would be easier. Matching the 4 second cycle of the monitor is probably easiest.
    last_error = error;
    FIRST_CYCLE = false;
  }
  
  if(!FIRST_CYCLE & elapsed_time >= CYCLETIME){
    last_time = this_time;
    process_value = analogRead(OzonePin);
    process_value = process_value/1023*OZONEGAIN;
    process_value_alternate = analogRead(ozone_pin_alternate);
    process_value_alternate = process_value_alternate/1023*CO2GAIN;
    
    error = setpoint - process_value;
    
    Pcomponent = error * kp * abs(error/setpoint);
    Icomponent += error * elapsed_time * ki * OZONE_ON;
    if(Icomponent >3.5){Icomponent = 3.5;}
    if(Icomponent < -.5){Icomponent = -0.5;}
    Dcomponent = (error - last_error) / elapsed_time * kd; // is there a way to make this hold the last value if the process value hasn't updated? Maybe averaging the current and last value would be easier. Matching the 4 second cycle of the monitor is probably easiest.
  
    
    vout_in_volts = Pcomponent + Icomponent + Dcomponent;
    if(vout_in_volts < 0){vout_in_volts = 0;}
    if(vout_in_volts > 10){vout_in_volts = 10;}
  
    DACout = floor(vout_in_volts/5*255);
    if(DACout <= 0){DACout = 0;} //This and the line below are to help prevent instability when the DACout is near the threshhold to keep the bulb on.
    if(DACout > 0 & DACout < 7){DACout = 7;} // This is the minimum DAC level that will consistently activate the UV bulb. Determined by eye.
    if(DACout > 255){DACout = 255;} // Prevent sending a value greater than 255 (the maximum value) to the DAC.
    DACout = DACout*OZONE_ON;
    //DACout = 255*OZONE_ON;
    //DACout = 20*OZONE_ON;
    //analogWrite(BulbPin,DACout);

    DFRout = floor(vout_in_volts * 1000);
    if(DFRout <= 0){DFRout = 0;} //This and the line below are to help prevent instability when the DACout is near the threshhold to keep the bulb on.
    if(DFRout > 0 & DFRout < 138){DACout = 138;} // This is the minimum DAC level that will consistently activate the UV bulb. Determined by eye.
    if(DFRout > 10000){DFRout = 10000;} // Prevent sending a value greater than 10000 (the maximum value) to the DFR.
    digitalWrite(ballast_power_relay_pin, OZONE_ON);
    DFRout = DFRout*OZONE_ON;
    dac.setDACOutVoltage(DFRout,0);
    BALLAST_MANUAL = !digitalRead(ballast_manual_on_sense_pin);
    BALLAST_AUTO = !digitalRead(ballast_auto_sense_pin);
    BULB_MANUAL = !digitalRead(bulb_intensity_manual_sense_pin);
    BULB_AUTO = !digitalRead(bulb_intensity_auto_sense_pin);


    
//    Serial.print("Total_time: ");
//    Serial.print(this_time);
//    Serial.print(",");  
//    Serial.print("Elapsed_time: ");
//    Serial.print(elapsed_time);
//    Serial.print(",");
    Serial.print("STATE:");
    Serial.print(STATE);
    Serial.print(",");
    Serial.print("Setpoint:");
    Serial.print(setpoint);
    Serial.print(",");
    Serial.print("Process_Value:");
    Serial.print(process_value);
    Serial.print(",");
    Serial.print("Error:");
    Serial.print(error);
    Serial.print(",");
    //Serial.print("Last_Error: ");
    //Serial.print(last_error);
    //Serial.print(",");
    Serial.print("Vout:");
    Serial.print(vout_in_volts);
    Serial.print(",");
    Serial.print("DFRout:");
    Serial.print(DFRout);
    Serial.print(",");
    Serial.print("Pcomponent:");
    Serial.print(Pcomponent,5);
    Serial.print(",");
    Serial.print("Icomponent:");
    Serial.print(Icomponent,5);
    Serial.print(",");
    Serial.print("Dcomponent:");
    Serial.print(Dcomponent,5);
    Serial.print(",");
    Serial.print("OZONE_ON:");
    Serial.print(OZONE_ON);
    Serial.print(",");
    Serial.print("BALLAST_MANUAL:");
    Serial.print(BALLAST_MANUAL);
    Serial.print(",");
    Serial.print("BALLAST_AUTO:");
    Serial.print(BALLAST_AUTO);
    Serial.print(",");
    Serial.print("BULB_MANUAL:");
    Serial.print(BULB_MANUAL);
    Serial.print(",");
    Serial.print("BULB_AUTO:");
    Serial.print(BULB_AUTO);
    Serial.print(",");
    Serial.print("Process_Value_alternate:");
    Serial.print(process_value_alternate);
    Serial.print(",");
    Serial.print("Door_Open:");
    Serial.print(door_sense);
    Serial.print(",");
    Serial.print("Ozonator_Temp:");
    Serial.print(ozonator_temp);
    Serial.print(",");
    Serial.print("Ozonator_Light_Intensity:");
    Serial.print(ozonator_light_intensity);    
    Serial.print(",");
    Serial.print("Kp:");
    Serial.print(kp,4);
    Serial.print(",");
    Serial.print("Ki:");
    Serial.print(ki*CYCLETIME,4);  
    Serial.print(",");
    Serial.print("Kd:");
    Serial.print(kd/CYCLETIME,4);      
    Serial.print(",");
    Serial.print("cycle time:");
    Serial.print(CYCLETIME);      
    Serial.println();
    last_error = error;
  }
}


void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte string_index = 0;
    char startMarker = '<';
    char endMarker = '>';
    char input_char;
 
    while (Serial.available() > 0 && newData == false) {
        input_char = Serial.read();

        if (recvInProgress == true) {
            if (input_char != endMarker) {
                receivedChars[string_index] = input_char;
                string_index++;
                if (string_index >= numChars) {
                    string_index = numChars - 1; // I should probably have this ignore the string or print an error
                }
            }
            else {
                receivedChars[string_index] = '\0'; // terminate the string
                recvInProgress = false;
                string_index = 0;
                newData = true;
            }
        }

        else if (input_char == startMarker) {
            recvInProgress = true;
        }
    }

if(newData){
  Serial.print(receivedChars);
  read_command_string();
  }

}

void read_command_string(){
    newData = false;
    const char *token;
    printf("%s \n",receivedChars);
    token = strtok(receivedChars,delim);
    Serial.println(token);

    switch(*token) // Read Command
    {

    case 68: //D Set Derivative Constant
        token = strtok(NULL,delim);
        if(atof(token) >= 0){
          kd = atof(token)*CYCLETIME;
          //Serial.print("Derivative Constant changed to: ");
          //Serial.println(token);
        }
        else {
           Serial.print("Derivative Constant invalid value: ");
           Serial.println(token);
        }
        break;

      
    case 73: //I Set Integral Constant
        token = strtok(NULL,delim);
        if(atof(token) >= 0){
          ki = atof(token)/CYCLETIME;
          //Serial.print("Integral Constant changed to: ");
          //Serial.println(token);
        }
        else {
           Serial.print("Integral Constant invalid value: ");
           Serial.println(token);
        }
        break;

      
    case 79: //O Set Ozone Control
        token = strtok(NULL,delim);
        if(atoi(token) == 0){
          OZONE_ON = 0;
          //Serial.print("OZONE_ON changed to: ");
          //Serial.println(token);
        }
        else if (atoi(token) == 1){
          OZONE_ON = 1;
          //Serial.print("OZONE_ON changed to: ");
          //Serial.println(token);
        }
        else {
           Serial.print("OZONE_ON invalid value: ");
           Serial.println(token);
        }
        break;

      case 80: //P Set Proportional Constant
        token = strtok(NULL,delim);
        if(atof(token) >= 0){
          kp = atof(token);
          //Serial.print("Proportional Constant changed to: ");
          //Serial.println(token);
        }
        else {
           Serial.print("Proportional Constant invalid value: ");
           Serial.println(atoi(token));
        }
        break;


    
    case 83: //S Set Setpoint
        token = strtok(NULL,delim);
        if(atoi(token) >= 0){
          setpoint = atoi(token);
          Serial.print("Setpoint changed to: ");
          Serial.println(token);
        } else {
          Serial.println("Invalid Setpoint");
        }
        break;
    
    default:
        Serial.print("Error: Invalid command: ");
        Serial.println(token);
    }
}
