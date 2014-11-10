
/*
******************************
*   Shrimp Monitor Prototype *
******************************
On Yun & Leonardo only these ports can be used for RX: 8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI).
*/

/*
***************************
* Constants
***************************
*/

#include <SoftwareSerial.h>
//#include <AltSoftSerial.h>

#include <Console.h>

//these no longer neeeded, were for temboo/yun
//#include <Bridge.h>
//#include <Temboo.h>

//updating for version to remove temboo dependency
//aws_keys.ino should contain aws public and private keys
#include <aws_keys.ino>

//tank identifier
const char site_tank_number[] = "nis01";

//serial chip enable ports
const byte enable_1 = 5;
const byte enable_2 = 4;

//serial channels
const char k1_sensor = '0';
const char orp_sensor = '1';
const char ph_sensor = '2';
const char k10_sensor = '3';
const char do_sensor = '4';
const char temp_sensor = '5';

const String comma = ",";

//serial chip ports
const byte s1_port = 7;
const byte s0_port = 6;
const byte serialrx = 10;
const byte serialtx = 11;


#define atlas_baud 38400
const char disable_led[] = "L0\r";
const char enable_led[] = "L1\r";
const char read_single_value[] = "R\r";
const char query_version[] = "I\r";
const char quiescent_mode[] = "E\r";
const char continuous_uncompensated[] = "C\r";

const String zero_temp = "00.00";
const String zero_conductivity = "00000";

String results_to_email = "";

//debug on or off
//note: make this controllable externally later (i.e. REST)
const boolean debug = false;
//const boolean debug = true;
const char debug_message[] = "*Debug*";


/*
***************************
* Globals
***************************
*/

SoftwareSerial sensorserial(serialrx, serialtx);

//setup array of serial connections to sensors
const char serial_sensors[] = {do_sensor, ph_sensor, orp_sensor, k1_sensor, k10_sensor, temp_sensor};
byte serial_sensors_size = sizeof(serial_sensors);

/*
********************************
* Setup the hardware
********************************
*/

void setup() {
  
  //removing yun specific functionality
  //Bridge.begin();                                                            //setup bridge to linux side of yun
  //Console.begin();                                                           //set up the console from yun linux

  pinMode(s1_port, OUTPUT);
  pinMode(s0_port, OUTPUT);
  pinMode(enable_1, OUTPUT);
  pinMode(enable_2, OUTPUT);

  sensorserial.begin(atlas_baud);

  //ensure all sensors in quiescent mode on power on
  for (byte i = 0; i < serial_sensors_size; i++) {
    open_channel(serial_sensors[i]);
    ensure_quiescent();
  }

  //if not in debug mode, disable the led's on the sensor circuits
  if (debug) {
    //wait for the console if in debug mode
    //20141110 changed to Serial from Console
    while (!Serial)

      //20141110 changed to Serial from Console
      Serial.println(debug_message);

    for (byte i = 0; i < serial_sensors_size; i++) {
      open_channel(serial_sensors[i]);
      debug_mode();
    }
  } else {
    //when not in debug, disable led's to conserve power
    for (byte i = 0; i < serial_sensors_size; i++) {
      open_channel(serial_sensors[i]);
      production_mode();
    }
  }

}


/*
************************************
* function: sensor_query()
* starts listening to sensor
* sends command, handles timing, and
* calls read_sensor_data() function to
* return input from serial
************************************
*/
String sensor_query(char channel, String label, byte iterate_count, String temp = zero_temp, String conductivity = zero_conductivity) {

  //open serial connection to k1 sensor
  open_channel(channel);
  //wait 1 sec to make sure connection established
  delay(1000);

  if (debug) {
    //20141110 changed to Serial from Console
    Serial.println("listen on " + label);
  }

  //remove carriage returns & other whitespace from results
  temp.trim();
  conductivity.trim();

  for (byte i = 0; i < iterate_count; i++) {
    ensure_quiescent();

    //do
    if (temp != zero_temp && conductivity != zero_conductivity) {
      if(debug){
        //20141110 changed to Serial from Console
        Serial.println(F("both set"));
        //2014-03-17 temporarily removed until conductivity sensors recalibrated
        //20141110 chaanged to Serial from Console
        Serial.println(temp + "," + conductivity + "\r");
        //Console.println(temp + ",B\r");
      }
      sensorserial.print(String(temp) + String(",") + String(conductivity) + String("\r"));
      sensorserial.print(read_single_value);
      //salinity & ph
    } else if (temp != zero_temp && conductivity == zero_conductivity) {
      if(debug){
        //20141110 changed to Serial from Console
        Serial.println(F("temp set"));
      }
      sensorserial.print(temp + "\r");
      //added this b/c ph sensor doesn't seem to return with just sending temp
      if (label == "ph") {
        sensorserial.print(read_single_value);
      }
      //others
    } else {
      if(debug){
        //20141110 changed to Serial from Console
        Serial.println(F("neither set"));
      }
      sensorserial.print(read_single_value);
    }

    //documentation says reading returned every 1000ms from atlas k1 -- seems to be the longest interval of the sensors
    delay(1050);
    String sensor_data = "";
    sensor_data = read_sensor_data();
    if (debug) {
      //20141110 changed to Serial from Console
        Serial.println(label + "," + sensor_data);
    }

    return (sensor_data);
  }
  //continuous done, back to quiescent
  ensure_quiescent();
}


/*
********************************
* loop runs continuously while powered up
* primary execution thread
********************************
*/

void loop() {

//sensor labels
const String k1_label = F("k1");
const String orp_label = F("orp");
const String ph_label = F("ph");
const String k10_label = F("k10");
const String do_label = F("do");
const String temp_label = F("t");

  //salinity says get 25 readings, documentation says not scientifically accurate without at least 25 readings
  //note: need clarification. Take last, take avg ? Currently taking last
  byte repeat_count =25;
  if (debug) {
    byte repeat_count = 3;
  }

  String temp_results = sensor_query(temp_sensor, temp_label, 1);
  String k1_results = sensor_query(k1_sensor, k1_label, repeat_count, temp_results);
  String k10_results = sensor_query(k10_sensor, k10_label, repeat_count, temp_results);
  String orp_results = sensor_query(orp_sensor, orp_label, 1);
  String ph_results = sensor_query(ph_sensor, ph_label, 1, temp_results);

  //conductivity sensor responds with three readings
  //need the first one EC in micro siemens

  char conductivity_result[16];
  k1_results.toCharArray(conductivity_result, 14);
  strtok(conductivity_result, ",");
  if(debug){
    //20141110 changed to Serial from Console
    Serial.print(F("k1 cond result"));
    Serial.println(conductivity_result);
  }
  String do_results = sensor_query(do_sensor, do_label, 1, temp_results, conductivity_result);

  //if k1 sensor did not return a value, get reading from K10 sensor for do compensated reading
  if (String(conductivity_result) == "--") {
    k10_results.toCharArray(conductivity_result, 14);
    //modifies it in place
    strtok(conductivity_result, ",");
    if(debug){
      //20141110 changed to Serial from Console
      Serial.print(F("k10 cond result"));
      Serial.println(conductivity_result);
    }
    String do_results = sensor_query(do_sensor, do_label, 1, temp_results, conductivity_result);
  }

  //convert tds to ppt
  //String ppt = strtok(NULL, ",");
  //int salinity = String(ppt).toInt()/1000;

  //results_to_email = k1_label + comma + k1_results + k10_label + comma + k10_results + String("ppt,") + String(salinity) + String("\r") + orp_label + comma + orp_results + ph_label + comma + ph_results + do_label + comma + do_results + temp_label + comma + temp_results;
  results_to_email = k1_label + comma + k1_results + orp_label + comma + orp_results + ph_label + comma + ph_results + do_label + comma + do_results + temp_label + comma + temp_results;
  //results_to_email = k10_results + orp_results + ph_results + do_results + temp_results;
  //results_to_email = k10_label + k10_results + orp_label + orp_results + ph_label + ph_results + do_label + do_results + temp_label + temp_results;


  if(debug){
    //20141110 changed to Serial from Console
    Serial.print(results_to_email);
  }

  //email all results in one email
  email_send(results_to_email);

  //make seperate sqs queue entry for each sensor

  sqs_send(k1_label + comma + k1_results);
  sqs_send(orp_label + comma + orp_results);
  sqs_send(ph_label + comma + ph_results);
  //sqs_send(k10_label + comma + k10_results);
  sqs_send(do_label + comma + do_results);
  sqs_send(temp_label + comma + temp_results);


  //delay between reading sets
  if (debug) {
    //30 sec
    delay(30000);
  } else {
    //1 hour
    delay(3600000);
  }

}


/*
**************************************
*  function: read_sensor_data(SoftwareSerial *sensorserial)
*  Takes a software serial object by reference
*  reads and prints to console
**************************************
*/
String read_sensor_data() {

  String sensorstring = "";                                                    //a string to hold the data from the Atlas Scientific product
  boolean sensor_stringcomplete = false;                                       //have we received all the data from the Atlas Scientific product
  sensorstring.reserve(30);                                                  //set aside some bytes for receiving data from Atlas Scientific product

  //added 20140310 trying to wait for serial data to be available before starting
  if (!sensorserial.available()) {
    delay(500);
  }

  while (sensorserial.available() && !sensor_stringcomplete) {                                               //while a char is holding in the serial buffer
    char inchar = (char)sensorserial.read();                                       //get the new char
    sensorstring += inchar;                                                         //add it to the sensorString
    if (inchar == '\r') {
      sensor_stringcomplete = true;                                                 //if the incoming character is a <CR>, set the flag
    }
  }

  if (sensor_stringcomplete) {                                                      //if a string from the Atlas Scientific product has been received in its entirety
    sensor_stringcomplete = false;                                                  //reset the flag used to tell if we have received a completed string from the Atlas Scientific product
    return sensorstring;
  } else {
    return sensorstring;
  }
}




/*
*****************************
* function: debug_mode(SoftwareSerial *sensorserial);
* if debug is true, turns on led's, checks version of each sensor, etc.
*****************************
*/
void debug_mode() {
  sensorserial.print(enable_led);
  sensorserial.print(enable_led);
  sensorserial.print(enable_led);
  //temporary
  /*
  //set k1 chip to k1 type sensor
  open_channel(k1_sensor);
  delay(1000);
  sensorserial.print(F("P,2\r"));
  //set k10 chip to k10 type sensor
  open_channel(k10_sensor);
  delay(1000);
  sensorserial.print(F("P,3\r"));
  */
  delay(500);
}


/*
*****************************
* function: production_mode(SoftwareSerial *sensorserial);
* if debug is true, turns on led's, checks version of each sensor, etc.
*****************************
*/
void production_mode() {
  sensorserial.print(disable_led);
  sensorserial.print(disable_led);
  sensorserial.print(disable_led);
  //temporary
  /*
  //set k1 chip to k1 type sensor
  open_channel(k1_sensor);
  delay(1000);
  sensorserial.print(F("P,2\r"));
  //set k10 chip to k10 type sensor
  open_channel(k10_sensor);
  delay(1000);
  sensorserial.print(F("P,3\r"));
  */
  delay(500);
}


/*
*****************************
* function: ensure_quiescent(SoftwareSerial *sensorserial);
* sets the sensor to quiescent mode
*****************************
*/
void ensure_quiescent() { //SoftwareSerial sensorserial) {
  sensorserial.print(quiescent_mode);
  sensorserial.print(quiescent_mode);
  sensorserial.print(quiescent_mode);
  delay(500);
}


/*
*****************************
* function: sqs_send(String sensor_result)
* send message via amazon sqs
*****************************
*/

void sqs_send(String sensor_result) {

  
  // Set Choreo inputs
  SendMessageChoreo.addInput(F("MessageBody"), String(sensor_result));
  SendMessageChoreo.addInput(F("AWSAccountId"), F("awsaccountid"));
  SendMessageChoreo.addInput(F("QueueName"), F("awsqueuename"));

  
}

/*
***************************
* freeRam function        *
***************************
*/
int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

/*
***************************
* open_channel()          *
***************************
*/
void open_channel(char channel) {

  switch (channel) {

    case '0':
      digitalWrite(enable_1, LOW);
      digitalWrite(enable_2, HIGH);
      digitalWrite(s0_port, LOW);
      digitalWrite(s1_port, LOW);
      break;

    case '1':
      digitalWrite(enable_1, LOW);
      digitalWrite(enable_2, HIGH);
      digitalWrite(s0_port, HIGH);
      digitalWrite(s1_port, LOW);
      break;

    case '2':
      digitalWrite(enable_1, LOW);
      digitalWrite(enable_2, HIGH);
      digitalWrite(s0_port, LOW);
      digitalWrite(s1_port, HIGH);
      break;

    case '3':
      digitalWrite(enable_1, LOW);
      digitalWrite(enable_2, HIGH);
      digitalWrite(s0_port, HIGH);
      digitalWrite(s1_port, HIGH);
      break;

    case '4':
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0_port, LOW);
      digitalWrite(s1_port, LOW);
      break;

    case '5':
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0_port, HIGH);
      digitalWrite(s1_port, LOW);
      break;

    case '6':
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0_port, LOW);
      digitalWrite(s1_port, HIGH);
      break;

    case '7':
      digitalWrite(enable_1, HIGH);
      digitalWrite(enable_2, LOW);
      digitalWrite(s0_port, HIGH);
      digitalWrite(s1_port, HIGH);
      break;

  }

}
