#include "DFRobot_GP8403.h"
DFRobot_GP8403 dac(&Wire,0x5F);

String firmware_version_number = "firmware version 0002"; // this will need to change with certain hardware changes. I'm not sure if I'll track it closely with most software changes
const int OZONE_PIN = A0;
const int OZONE_PIN_ALTERNATE = A3;
const int BALLAST_POWER_RELAY_PIN = 12;
const int BALLAST_MANUAL_ON_SENSE_PIN = 4;
const int BALLAST_AUTO_SENSE_PIN = 2;
const int BULB_INTENSITY_MANUAL_SENSE_PIN = 7;
const int BULB_INTENSITY_AUTO_SENSE_PIN = 8;
//const int BulbPin = 9;
const int CYCLETIME = 4000;
float setpoint = 150;
float process_value;
float process_value_alternate;
float error = 0;
float last_error = 0;
float ozonator_temp = 0;
float ozonator_light_intensity = 0;
int door_sense = 0; // this is the one of the physical door sensor

//Find how to store these in non-volatile memory

//door code
const int DOOR_SENSOR_PIN 3; // we can set this when we put it all together
bool door_interlock_is_bypassed = false; // this is the one that ignores door sensor


float kp = 0.0600;
float ki = 0.0002/CYCLETIME;
float kd = 0.0899*CYCLETIME;


float Pcomponent = 0;
float Icomponent = 1.32; // Set the initial I term to help things stabilize sooner.
float Dcomponent = 0;
float vout_in_volts;

int ozone_on = 0;
int relay_state = 0;
int ballast_manual = 0;
int ballast_auto = 0;
int bulb_manual = 0;
int bulb_auto = 0;
int state = 0;


int DFRout = 0;
int ozone_gain = 250;
int CO2_gain = 2000;
bool first_cycle = true;


float last_time = millis();
float this_time;
float elapsed_time;

//COMMUNICATION GLOBALS
const byte NUM_CHARS = 32;
char receivedChars[NUM_CHARS];
const char *delim = " ,:/"; 
boolean newData = false;

void setup() {
  pinMode(BALLAST_POWER_RELAY_PIN,OUTPUT);
  pinMode(BALLAST_MANUAL_ON_SENSE_PIN, INPUT_PULLUP);
  pinMode(BALLAST_AUTO_SENSE_PIN, INPUT_PULLUP);
  pinMode(BULB_INTENSITY_MANUAL_SENSE_PIN, INPUT_PULLUP);
  pinMode(BULB_INTENSITY_AUTO_SENSE_PIN, INPUT_PULLUP);
  pinMode(DOOR_SENSOR_PIN, INPUT_PULLUP);
  digitalWrite(BALLAST_POWER_RELAY_PIN,0);
  Serial.begin(9600);
  //Serial.println("<Arduino is ready>");
  while(dac.begin()!=0){
    Serial.println("init error");
    delay(1000);
  }
  Serial.println("<Arduino is ready>");
  //Set DAC output range
  dac.setDACOutRange(dac.eOutputRange10V);
  //Set DFR output pin 0 to 0 volts. This is for automated control of the UV bulb and makes sure it's off when starting
  dac.setDACOutVoltage(0,0);
  //Set DFR output pin 1 to 10 volts. This is for manual control of the UV bulb and provides the excitation voltage for the manual potentiometer
  dac.setDACOutVoltage(10000,1);
}

void loop() {
control_loop();
recvWithStartEndMarkers();
}



bool IsOzoneOn() {
  door_sense = digitalRead(DOOR_SENSOR_PIN);
  return (door_sense == 0) || door_interlock_is_bypassed;
}


void control_loop(){
  this_time = millis();
  elapsed_time = this_time - last_time;
  ozone_on = IsOzoneOn()
  if(first_cycle & elapsed_time >= CYCLETIME){
    dac.setDACOutVoltage(0,0);
    ballast_manual = !digitalRead(BALLAST_MANUAL_ON_SENSE_PIN);
    ballast_auto = !digitalRead(BALLAST_AUTO_SENSE_PIN);
    bulb_manual = !digitalRead(BULB_INTENSITY_MANUAL_SENSE_PIN);
    bulb_auto = !digitalRead(BULB_INTENSITY_AUTO_SENSE_PIN);
    last_time = this_time;
    process_value = analogRead(OZONE_PIN);
    process_value = process_value/1023*ozone_gain;
    process_value_alternate = analogRead(OZONE_PIN_ALTERNATE);
    process_value_alternate = process_value_alternate/1023*CO2_gain;
    error = setpoint - process_value;
    Pcomponent = error * kp * abs(error/setpoint);
    Icomponent += error * elapsed_time * ki * ozone_on;
    Dcomponent = (error - last_error) / elapsed_time * kd; // is there a way to make this hold the last value if the process value hasn't updated? Maybe averaging the current and last value would be easier. Matching the 4 second cycle of the monitor is probably easiest.
    last_error = error;
    first_cycle = false;
    
    
  }
  
  if(!first_cycle & elapsed_time >= CYCLETIME){
    last_time = this_time;
    process_value = analogRead(OZONE_PIN);
    process_value = process_value/1023*ozone_gain;
    process_value_alternate = analogRead(OZONE_PIN_ALTERNATE);
    process_value_alternate = process_value_alternate/1023*CO2_gain;
    
    error = setpoint - process_value;
    
    Pcomponent = error * kp * abs(error/setpoint);
    Icomponent += error * elapsed_time * ki * ozone_on;
    if(Icomponent >3.5){Icomponent = 3.5;}
    if(Icomponent < -.5){Icomponent = -0.5;}
    Dcomponent = (error - last_error) / elapsed_time * kd; // is there a way to make this hold the last value if the process value hasn't updated? Maybe averaging the current and last value would be easier. Matching the 4 second cycle of the monitor is probably easiest.
  
    
    vout_in_volts = Pcomponent + Icomponent + Dcomponent;
    if(vout_in_volts < 0){vout_in_volts = 0;}
    if(vout_in_volts > 10){vout_in_volts = 10;}
  

    DFRout = floor(vout_in_volts * 1000);
    if(DFRout <= 0){DFRout = 0;} //This and the line below are to help prevent instability when the DACout is near the threshhold to keep the bulb on.
    if(DFRout > 0 & DFRout < 138){DFRout = 138;} // This is the minimum DAC level that will consistently activate the UV bulb. Determined by eye.
    if(DFRout > 10000){DFRout = 10000;} // Prevent sending a value greater than 10000 (the maximum value) to the DFR.
    digitalWrite(BALLAST_POWER_RELAY_PIN, ozone_on);
    DFRout = DFRout*ozone_on;
    dac.setDACOutVoltage(DFRout,0);
    ballast_manual = !digitalRead(BALLAST_MANUAL_ON_SENSE_PIN);
    ballast_auto = !digitalRead(BALLAST_AUTO_SENSE_PIN);
    bulb_manual = !digitalRead(BULB_INTENSITY_MANUAL_SENSE_PIN);
    bulb_auto = !digitalRead(BULB_INTENSITY_AUTO_SENSE_PIN);


    
//    Serial.print("Total_time: ");
//    Serial.print(this_time);
//    Serial.print(",");  
//    Serial.print("Elapsed_time: ");
//    Serial.print(elapsed_time);
//    Serial.print(",");
    Serial.print("state:");
    Serial.print(state);
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
    Serial.print("ozone_on:");
    Serial.print(ozone_on);
    Serial.print(",");
    Serial.print("ballast_manual:");
    Serial.print(ballast_manual);
    Serial.print(",");
    Serial.print("ballast_auto:");
    Serial.print(ballast_auto);
    Serial.print(",");
    Serial.print("bulb_manual:");
    Serial.print(bulb_manual);
    Serial.print(",");
    Serial.print("bulb_auto:");
    Serial.print(bulb_auto);
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
    Serial.print(",");
    Serial.print("firmware version number");
    Serial.print(firmware_version_number);
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
                if (string_index >= NUM_CHARS) {
                    string_index = NUM_CHARS - 1; // I should probably have this ignore the string or print an error
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
          ozone_on = 0;
          //Serial.print("ozone_on changed to: ");
          //Serial.println(token);
        }
        else if (atoi(token) == 1){
          ozone_on = 1;
          //Serial.print("ozone_on changed to: ");
          //Serial.println(token);
        }
        else {
           Serial.print("ozone_on invalid value: ");
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
