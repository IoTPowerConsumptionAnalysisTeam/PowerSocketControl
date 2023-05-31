#include  <Filters.h>                      //This library does a massive work check it's .cpp   file
#include  <WiFiNINA.h>
#include  <SPI.h>
#include  "time.h"
#define V_Pin A0            //Sensor data pin on A0 analog   input
#define ACS_Pin A1                        

// network connection
// wifi name
char ssid[] = "Sycamore";
// wifi password
char pass[] = "iotpower";

char serverIP[]= "1.171.164.243";
int status = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiClient client;
String readString;
char URL[]="192.168.203.154";
char API[]="/api/user/6475eed5d83292c1741a4752/power_socket/6475eef5d83292c1741a4771/consumption";

// time
unsigned long start_time;
unsigned long end_time;

// ----- 電流
float ACS_Value;                              //Here we keep the raw   data valuess
float testFrequency = 50;                    // test signal frequency   (Hz)
float windowLength = 40.0/testFrequency;     // how long to average the   signal, for statistist

float intercept = 0; // to be adjusted based   on calibration testing
float slope = 0.0752; // to be adjusted based on calibration   testing
                      //Please check the ACS712 Tutorial video by SurtrTech   to see how to get them because it depends on your sensor, or look below

float   Amps_TRMS; // estimated actual current in amps

unsigned long printPeriod   = 100; // in milliseconds
// Track time in milliseconds since last reading 
unsigned   long previousMillis = 0;
// =====

// -----電壓
double sensorValue=0;
double sensorValue1=0;
int crosscount=0;
int climbhill=0;
double VmaxD=0;
double VeffD;
double Veff;
// =====

// -----每分鐘統計
unsigned long postPeriod = 1000 * 5;
unsigned long maxPreviousMillis;
double maxA;
double maxV;
// =====

void setup() {
  Serial.begin( 9600 );    //   Start the serial port
  pinMode(ACS_Pin,INPUT);  //Define the pin mode

  maxPreviousMillis = millis();
  maxA = 0;
  maxV = 0;

  // connect to wifi
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network");
    Serial.println();
    // Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    /* status value: meaning
    0: WL_IDLE_STATUS
    1: WL_NO_SSID_AVAIL
    2: WL_SCAN_COMPLETED
    3: WL_CONNECTED
    4: WL_CONNECT_FAILED
    5: WL_CONNECTION_LOST
    6: WL_DISCONNECFTED
    7: WL_AP_LISTENING
    8: WL_AP_CONNECTED*/
    delay(1000);
  }
  server.begin();
  Serial.println("wifi connected");
  start_time = WiFi.getTime();
}

void loop() {
  //Serial.println("start of loop: "+String(WiFi.getTime()));
  RunningStatistics inputStats;                 // create statistics   to look at the raw test signal
  inputStats.setWindowSecs( windowLength );     //Set   the window length
  while( true ) {
    // ----- 電流
    ACS_Value = analogRead(ACS_Pin);   // read the analog in value:
    inputStats.input(ACS_Value);  // log to Stats   function
    // =====

    if((unsigned long)(millis() - previousMillis) >= printPeriod)   { //every second we do the calculation
      previousMillis = millis();   //   update time
      // ----- 電流
      Amps_TRMS = intercept + slope * inputStats.sigma();
      if (Amps_TRMS > maxA) maxA = Amps_TRMS;
      // ----- 電壓
      sensorValue = analogRead(V_Pin);
      if (sensorValue>sensorValue1 && sensorValue>511){
        climbhill=1;
        VmaxD=sensorValue;
      }
      if (sensorValue<sensorValue1 && climbhill==1){
        climbhill=0;
        VmaxD=sensorValue1;
        VeffD=VmaxD/sqrt(2);
        Veff=(((VeffD-420.76)/-90.24)*-210.2)+210.2;
        if (Veff > maxV) maxV = Veff;
        // Serial.print(Veff);
        VmaxD=0;
      }
      sensorValue1=sensorValue;
    }
    // -----post
    if((unsigned long)(millis() - maxPreviousMillis) >= postPeriod){
      end_time = WiFi.getTime();
      // WiFi.getTime() return 0 at first n times, therefore we wait until the real unix time to post data
      if(!(start_time==0 || end_time==0)){
        Serial.print("電流: ");
        Serial.print(maxA);
        Serial.print(", ");
        Serial.print("電壓: ");
        Serial.print(maxV);
        Serial.println();
        
        double kilowattHour = (maxA * maxV / 1000) / 3600;
        String jsonStTime = "{\"start_time\":"+String(start_time);
        String jsonEnTime = ",\"end_time\":"+String(end_time);
        String jsonConsum = ",\"consumption\":"+String(kilowattHour, 10)+"}";  
        String jsonStr = jsonStTime+jsonEnTime+jsonConsum;
        Serial.println(jsonStr);
        if(WiFi.status()== WL_CONNECTED){
          if (client.connect(URL, 3000)){
            client.print("POST ");
            client.print(API);
            client.println(" HTTP/1.1"); 
            client.print("Host: ");
            client.print(URL);
            client.println(":3000");
            client.println("Content-type: application/json");
            client.print("Content-Length: ");
            client.println(jsonStr.length());
            client.println();
            client.println(jsonStr);
          }else{
            Serial.println("Error connecting to server");
          }
        }
      }
      maxPreviousMillis = millis();
      maxA = 0;
      maxV = 0;
      start_time = end_time;
    }
  }
}

/* About the slope and intercept
 * First you need to   know that all the TRMS calucations are done by functions from the library, it's   the "inputStats.sigma()" value
 * At first you can display that "inputStats.sigma()"   as your TRMS value, then try to measure using it when the input is 0.00A
 * If   the measured value is 0 like I got you can keep the intercept as 0, otherwise you'll   need to add or substract to make that value equal to 0
 * In other words " remove   the offset"
 * Then turn on the power to a known value, for example use a bulb   or a led that ou know its power and you already know your voltage, so a little math   you'll get the theoritical amps
 * you divide that theory value by the measured   value and here you got the slope, now place them or modify them
 */
