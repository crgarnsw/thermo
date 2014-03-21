#include <Ethernet.h>
#include <TFT.h>
#include <SPI.h>
#include <DHT.h>
#include <SD.h>

/**
  HUDThermo
   Read local temp, set setpt.
   
    DHT22 pin 1 (left of sensor) connected to +5V
          pin 2 connected to DHTPIN
          pin 4 connected to ground
          connect 10K resistor from pin 2 to pin 1
          
    POT signal pin connected to POTPIN

    TFT Display connected to non-SPI pins, should
    try on the SPI pins later.

    All data is stored in SI, and will be converted
    for display.
*/

// For the breakout, you can use any (4 or) 5 pins
#define sclk 22
#define mosi 5
#define cs   6
#define dc   7
#define rst  8  // you can also connect this to the Arduino reset
#define sd_cs 4

#define DHTPIN 3
#define DHTTYPE DHT22
#define POTPIN 1
#define PIRPIN 2

#define HEATPIN 13
#define COOLPIN 11

#define C_TO_F(c) (c * 9 / 5 + 32)
// for display F
#define TO_LOCAL(c) C_TO_F(c)

DHT dht(DHTPIN, DHTTYPE);

// Option 1: use any pins but a little slower
Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, mosi, sclk, rst);

//global
float gSetpt, goSetpt;
float gHum, goHum, gTem, goTem, gOat = 35, goOat = 40;
int gOcc = 0, goOcc=0;
float gTemOff = 1; // temperature offset.

// for posting records
EthernetClient gClient;
// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte gMac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
//char gServer[] = "thermo-c9-crgarnsw.c9.io";
IPAddress gServer(10,28,22,121);

unsigned long gReadInterval  = 1000 * 5; // once every 5 seconds
unsigned long gWriteInterval = 1000 * 5; // once a minute
unsigned long gLastTimeRead = 0;
unsigned long gLastTimeWrite = 0;

// user record
int gUser = 1;

void setup() {
  Serial.begin(9600);
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab
  tft.background(ST7735_BLACK);
  dht.begin();
  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  
  pinMode(HEATPIN, OUTPUT);
  pinMode(COOLPIN, OUTPUT);

  Serial.println("Ethernet");
  if( Ethernet.begin(gMac) == 0 ) {
    Serial.println("Failed to start Ethernet");
  }
  printIP();
  
  pinMode(9, OUTPUT);
  analogWrite(9, 128);
  
  if(!SD.begin(sd_cs)) {
    Serial.println("Card failed, or not present");
  }
}

void loop() {
  String string;
  readPot();
  if(millis() > (gLastTimeRead + gReadInterval)) {
    readValues();
    update();
    gLastTimeRead = millis();
  }
  printValues();
  if(millis() > (gLastTimeWrite + gWriteInterval)) {
    string = makeString();
    record(string);
    postRecord(string);
    gLastTimeWrite = millis();
  }
}

// read the pot
void readPot() {
  gSetpt = analogRead(POTPIN);
  gSetpt = map(gSetpt, 0, 1024, 100, 322); // 50F to 90F
  gSetpt = gSetpt/10;
}

// Method to update the display on the screen.
void printValues() {
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
  if(gOat != goOat) {
    goOat = gOat;
    tft.setCursor(65,10);
    printNum(TO_LOCAL(gOat), 2); // outside air temp
  }
  
  if(gSetpt != goSetpt) {
    goSetpt = gSetpt;
    tft.setCursor(65,27);
    printNum(TO_LOCAL(gSetpt), 2); // stpt
  }
  
  if(gHum != goHum) {
    goHum = gHum;
    tft.setCursor(65, 44);
    printNum(gHum, 2); //hum
    tft.setTextSize(2);
    tft.print('%');
  }
  
  if(gTem != goTem) {
    goTem = gTem;
    tft.setCursor(0, 60);
    printNum(TO_LOCAL(gTem), 3); // temp
  }
  
  if(gOcc != goOcc) {
    goOcc = gOcc;
    tft.setCursor(10, 135);
    tft.print(gOcc ? 'o' : ' ');
  }
}

// read all the values from the sensors
void readValues() {
  gHum = dht.readHumidity();
  gTem = dht.readTemperature();
  gOcc = digitalRead(PIRPIN);
}

// update leds
void update() {
  if((gSetpt+gTemOff) < gTem) {
    digitalWrite(HEATPIN, HIGH);
  } else {
    digitalWrite(HEATPIN, LOW);
  }
  
  if((gSetpt-gTemOff) > gTem) {
    digitalWrite(COOLPIN, HIGH);
  } else {
    digitalWrite(COOLPIN, LOW);
  }
}

// record the data
void record(String s) {
  File dataFile = SD.open("dataLog.txt", FILE_WRITE);
  
  if(dataFile) {
    dataFile.println(s);
    dataFile.close();
  } else {
    Serial.println("Could not open sd card");
  }
  
  Serial.print("Writing to SD: ");
  Serial.println(s);
}

// write to server
void postRecord(String s) {
  Serial.println("Going to Ethernet");
  if(gClient.connect(gServer, 8080)) {
    Serial.print("connected to ");
    Serial.println(gServer);

    // send the HTTP PUT request:
    gClient.println("POST /dataLog HTTP/1.1");
    gClient.println("Host: localhost");
    gClient.println("User-Agent: arduino");
    gClient.print("Content-Length: ");
    gClient.println(s.length());

    // last pieces of the HTTP PUT request:
    gClient.println("Content-Type: text/json");
    gClient.println("Connection: close");
    gClient.println();

    // here's the actual content of the PUT request:
    gClient.println(s);

    // get reply
    while(gClient.connected()) {
      if(gClient.available()) {
        char c = gClient.read();
        Serial.print(c);
        //TODO getting setpt from server.
      }
    }
    
    gClient.stop();
  } else {
    Serial.println("Failed going to Ethernet");
  }
}

// print number with 3.1 decimals
void printNum(float f, int s) {
  int i = (int)f;
  
  tft.setTextSize(s);
  if(i < 10) {
    tft.print(' ');
  }
  
  if(i < 100) {
    tft.print(' ');
  }
  tft.print(String(i));
  
  tft.setTextSize(s-1);
  tft.print(String((int)(((f) - (int)f)*10)));
}

void printIP() {
    // print your local IP address:
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print("."); 
  }
  Serial.println();
}

// make xml string
String makeString() {
  String ret = "{hud:{t:";
  ret += (int)(100*gTem);
  ret += ",h:";
  ret += (int)(100*gHum);
  ret += ",st:";
  ret += (int)(10*gSetpt);
  ret += "}}";
  
  return ret;
}
