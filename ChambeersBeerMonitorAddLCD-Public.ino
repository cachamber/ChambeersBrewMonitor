/*
Chambeers brew monitor
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>



// RGB Shield
#include <Adafruit_MCP23017.h>
#include <Adafruit_RGBLCDShield.h>
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
// These #defines make it easy to set the backlight color
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

// Temp BME280 for ambient
#include <SparkFunBME280.h>
BME280 mySensor;

// OneWire sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 13
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Phat library for posting data
#include <Phant.h>
const char PhantHost[] = "data.sparkfun.com";
const char PublicKey[] = "PUBLIC";
const char PrivateKey[] = "PRIVATE";
const unsigned long postRate = 60000;
unsigned long lastPost = 0;

// Wireless Information
const char *ssid = "SSID";
const char *password = "PASSWORD";

// Setup webserver on port 80
ESP8266WebServer server ( 80 );

const int LED_PIN = 5;
const int FAN_PIN = 15;

// Variables are here
double setpoint,fermtemp,fanout,cair;
double tempf = 60.0;
double gap = abs(setpoint - fermtemp);

// Convert double to string
char *dtostrf(double val, signed char width, unsigned char prec, char *s);


// PID Library
#include <PID_v1.h>
//Specify the links and initial tuning parameters
double aggKp=4, aggKi=5, aggKd=1;
double consKp=1, consKi=0.05, consKd=0.25;
//Specify the links and initial tuning parameters
PID myPID(&fermtemp, &fanout, &setpoint, aggKp, aggKi, aggKd, REVERSE);

// Start here
void loop ( void ) {
  
  // Handle web clients
  server.handleClient();
  // Handle phant posting
  if ((lastPost + postRate <= millis()) || lastPost == 0)
  {
    Serial.println("Posting to Phant!");
    if (postToPhant())
    {
      lastPost = millis();
      Serial.println("Post Suceeded!");
    }
    else // If the Phant post failed
    {
      delay(500); // Short delay, then try again
      Serial.println("Post failed, will try again.");
    }
  } // End phant if


  // DS18B20
  sensors.requestTemperatures(); // Send the command to get temperatures
  fermtemp = sensors.getTempFByIndex(0);
  cair = sensors.getTempFByIndex(1);
  
  tempf = mySensor.readTempF();
   
  // TESTING
  // fermtemp = tempf;
  
  // PID control here
  // TODO: Don't set tunings every time through the loop...
  gap = abs(setpoint - fermtemp);
 // if ( gap < 5 ) 
  //{
    // myPID.SetTunings(consKp, consKi, consKd);
 // }
 // else
 // {
 //    myPID.SetTunings(aggKp, aggKi, aggKd);
 // }
  myPID.Compute();
  // LCD Display
  lcd.clear();
  lcd.print("Temp:");
  lcd.print(fermtemp,2);
  lcd.print("->");
  lcd.print(setpoint,2);
  lcd.setCursor(0,1);
  lcd.print("CA:");
  lcd.print(cair,2);
  lcd.print(" F:");
  lcd.print((int)fanout);
  
    // Print our stuff out to serial
  Serial.print("Setpoint: ");
  Serial.print(setpoint,2);
  Serial.print(" Ferm Temp: ");
  Serial.print(fermtemp,2);
  Serial.print(" Air Temp: ");
  Serial.print(cair,2);
  Serial.print(" FAN: ");
  Serial.println((int)fanout);
  analogWrite(FAN_PIN,(int)fanout);
  delay(50);
}

void setup ( void ) {
  pinMode ( LED_PIN, OUTPUT );
  pinMode ( FAN_PIN, OUTPUT );
  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  lcd.setBacklight(RED);
  
  setpoint = 66.0;
  digitalWrite ( LED_PIN, 0 );
  Serial.begin ( 115200 );
  WiFi.begin ( ssid, password );
  Serial.println ( "" );

  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
    lcd.print(".");
  }
  lcd.setBacklight(GREEN);
  lcd.clear();
  Serial.println ( "" );
  Serial.print ( "Connected to " );
  lcd.print("SSID:");
  Serial.println ( ssid );
  lcd.println(ssid);
  Serial.print ( "IP address: " );
  lcd.print("IP:");
  Serial.println ( WiFi.localIP() );
  lcd.println(WiFi.localIP());

  if ( MDNS.begin ( "esp8266" ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.on ( "/", handleRoot );
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println ( "HTTP server started" );
  // Start BME280 
  startBME280();
  // Start up the OneWire library
  sensors.begin();

   //turn the PID on
  myPID.SetOutputLimits(0,1023);
  myPID.SetMode(AUTOMATIC);
}


void handleRoot() {
	digitalWrite ( LED_PIN, 1 );
	char temp[2500];
  char setpoint_temp[6];
  char ferm_temp[6];
  char cair_temp[6];

  if ( server.args() > 0 )
  {
     setpoint = server.arg("newsetpoint").toFloat();
  }
  // Convert temps to strings for display
  dtostrf(setpoint,4,2,setpoint_temp);
  dtostrf(fermtemp,4,2,ferm_temp);
  dtostrf(cair,4,2,cair_temp);
  
  
	snprintf ( temp, 2500,
"<html>\
  <head>\
    <meta http-equiv='refresh' content='300'/>\
    <script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js\"></script>\
    <script src=\"https://www.google.com/jsapi\"></script>\
    <script>\
      function drawChart() {\
        var public_key = 'PUBLIC';\
        var jsonData = $.ajax({\
          url: 'https://data.sparkfun.com/output/' + public_key + '.json',\
          data: { page: 1},\
          dataType: 'jsonp',\
        }).done(function (results) {\
          var data = new google.visualization.DataTable();\
          data.addColumn('datetime', 'Time');\
          data.addColumn('number', 'Temp');\
          $.each(results, function (i, row) {\
            data.addRow([\
              (new Date(row.timestamp)),\
              parseFloat(row.tempf)\
            ]);\
          });\
          var chart = new google.visualization.LineChart($('#chart').get(0));\
          chart.draw(data, {\
            title: 'Fermentation Chamber',\
            crosshair: { trigger: 'both' },\
            trendlines: { 0: { color: 'green',\
                               opacity: 0.2,\
                              showR2: true,\
                            visibleInLegend: true \
                              } },\
            explorer: {},\
          });\
        });\
      }\
      google.load('visualization', \'1\', {\
        packages: ['corechart']\
      });\
      google.setOnLoadCallback(drawChart);\
    </script>\
    <title>Chambeers Fermentation Monitor</title>\
    <style>\
      body { background-color: #ffffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Chambeers Status</h1>\
    <p>Current Temp: %s<p>\
    <p>Cold Air Temp: %s<p>\
    <p>Setpoint: %s<p>\
    <form action=\"\">\
       Update Setpoint: <input type=\"text\" name=\"newsetpoint\"><br>\
       <input type=\"submit\" value=\"Submit\">\
    </form>\
    <\div id=\"chart\" style=\"width:100\%\"></div>\
  </body>\
</html>", ferm_temp, cair_temp,setpoint_temp);
	server.send ( 2500, "text/html", temp );
	digitalWrite ( LED_PIN, 0 );
}

void handleNotFound() {
	digitalWrite ( LED_PIN, 1 );
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for ( uint8_t i = 0; i < server.args(); i++ ) {
		message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
	}

	server.send ( 404, "text/plain", message );
	digitalWrite ( LED_PIN, 0 );
}

// Utilities
void startBME280()
{
  
  mySensor.settings.commInterface = I2C_MODE;
  mySensor.settings.I2CAddress = 0x76;
  mySensor.settings.runMode = 3; //Normal mode
    //tStandby can be:
  //  0, 0.5ms
  //  1, 62.5ms
  //  2, 125ms
  //  3, 250ms
  //  4, 500ms
  //  5, 1000ms
  //  6, 10ms
  //  7, 20ms
  mySensor.settings.tStandby = 0;
  
  //filter can be off or number of FIR coefficients to use:
  //  0, filter off
  //  1, coefficients = 2
  //  2, coefficients = 4
  //  3, coefficients = 8
  //  4, coefficients = 16
  mySensor.settings.filter = 0;
  
  //tempOverSample can be:
  //  0, skipped
  //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
  mySensor.settings.tempOverSample = 3;

  //pressOverSample can be:
  //  0, skipped
  //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
    mySensor.settings.pressOverSample = 3;
  
  //humidOverSample can be:
  //  0, skipped
  //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
  mySensor.settings.humidOverSample = 3;
  Serial.print("Starting BME280... result of .begin(): 0x");
  //Calling .begin() causes the settings to be loaded
  delay(10);  //Make sure sensor had enough time to turn on. BME280 requires 2ms to start up.
  Serial.println(mySensor.begin(), HEX);
  Serial.print("ID(0xD0): 0x");
  Serial.println(mySensor.readRegister(BME280_CHIP_ID_REG), HEX);
}

int postToPhant()
{
  float pressure,myrh,alt;
  
  // LED turns on when we enter, it'll go off when we 
  // successfully post.
  digitalWrite(LED_PIN, LOW);
  lcd.setBacklight(YELLOW);

  // Declare an object from the Phant library - phant
  Phant phant(PhantHost, PublicKey, PrivateKey);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String postedID = "ccThingDev-" + macID;

  // Take temperature measurement
  tempf = mySensor.readTempF();
  // Take pressure measurement in kiloPascals
  pressure = mySensor.readFloatPressure();
  // Relative humidity in %
  myrh = mySensor.readFloatHumidity();
  // Altitude in Feet
  alt = mySensor.readFloatAltitudeFeet();

  sensors.requestTemperatures(); // Send the command to get temperatures
  fermtemp = sensors.getTempFByIndex(0);
  cair = sensors.getTempFByIndex(1);

  // TESTING
  //fermtemp = tempf;
  
  // Add the four field/value pairs defined by our stream:
  phant.add("id", postedID);
  phant.add("pressurekpa", pressure);
  phant.add("setpointf", setpoint);
  phant.add("tempf", tempf);
  phant.add("relhumid", myrh);
  phant.add("altitude", alt); 
  phant.add("fermtempf", fermtemp); 
  phant.add("fan", fanout); 
  phant.add("cair", cair);
  
  // Print our stuff out to serial
  Serial.print("Setpoint: ");
  Serial.print(setpoint,2);
  Serial.print(" Ferm Temp: ");
  Serial.print(fermtemp,2);
  Serial.print(" Amb Temp: ");
  Serial.print(tempf,2);
  Serial.print(" Pressure: ");
  Serial.print(pressure,2);
  Serial.print(" %RH: ");
  Serial.print(myrh,2);
  Serial.print(" Alt: ");
  Serial.println(alt,2);
 
  // Now connect to data.sparkfun.com, and post our data:
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(PhantHost, httpPort)) 
  {
    // If we fail to connect, return 0.
    return 0;
    lcd.setBacklight(RED);
  }
  // If we successfully connected, print our Phant post:
  client.print(phant.post());

  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line); // Trying to avoid using serial
  }

  // Before we exit, turn the LED off.
  digitalWrite(LED_PIN, HIGH);
  lcd.setBacklight(GREEN);

  return 1; // Return success
}
