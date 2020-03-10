// Libraries
#include <Wire.h>
#include <SPI.h>
#include <SD.h> // Library for SD
#include "Seeed_BME280.h" // Library for BME280
#include <DS3231.h> // Library for clock module
#include <Elegoo_GFX.h>    // LCD graphics library
#include <Elegoo_TFTLCD.h> // LCD screen library
#include <TouchScreen.h> // LCD Touch library

// Pin Allocation
int sdPin = 53;
int dustPin = 48;
#define purityPin A10

// LCD Allocation
#define LCD_CS A3 // chip select
#define LCD_CD A2 
#define LCD_WR A1 // write
#define LCD_RD A0 //read
#define LCD_RESET A4 // reset
#define YP A2  // touch control
#define XM A3  // touch control
#define YM 8   // touch control
#define XP 9   // touch control

// Storage Variables
bool storageInit=true;
File dataFile;
Sd2Card card;
SdVolume volume;
SdFile root;
String filename;
String monthFolder;
bool createFile=false;
bool logging=false;
unsigned long starttime;
unsigned long sampleRate = 1000; 

// BME280 Variables
BME280 bme280;
float pressure;
  
// Clock Variables
DS3231 clock;
RTCDateTime dt;

// dust sensor variables
unsigned long duration;
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
float concentration = 0;

// LCD Variables
//UI
#define Cyan  0x07FF  
#define btnW 100 
#define btnH 40 
//touch variables
#define TS_MINX 70
#define TS_MAXX 920
#define TS_MINY 120
#define TS_MAXY 900

Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
Elegoo_GFX_Button mainBtns[2];
bool mainBtnsActive[2] = {}; 

void setup()   { 
  // Serial initialisation               
  Serial.begin(9600);
  while (!Serial){}

  pinMode(purityPin, INPUT);
  pinMode(dustPin, INPUT);
  
  // SD initialisation
  Serial.print("Initializing SD card...");
  if (!SD.begin(sdPin)) {
    Serial.println("initialization failed!");
    storageInit=false;
  }
  if (!card.init(SPI_HALF_SPEED, sdPin)){storageInit=false;}
  if (!volume.init(card)){storageInit=false;}

  // Initialisation of Sensors and other Modules
  // temperature, humidity, pressure
  if(!bme280.init()){
    Serial.println("BME280 Sensor Error");
  }
  
  // clock
  clock.begin(); 
  clock.setDateTime(__DATE__, __TIME__); // Syncs with compliation time
  Serial.println("initialization done.");

  // LCD Screen
  tft.begin(0x9341);
  tft.fillScreen(0x0000);
  tft.setCursor(0, 0);
  tft.setTextColor(0xFFFF);  tft.setTextSize(2); tft.setRotation(1);  

  //Create Buttons
  // ref, x, y, w, h, outline, fill, text colour, text, text size
  mainBtns[0].initButton(&tft, tft.width()/4, 200, 100, 40, Cyan, 0x0000, Cyan, "Scan", 3); 
  mainBtns[1].initButton(&tft, tft.width()*0.75, 200, 100, 40, Cyan, 0x0000, Cyan, "Log", 3); 
  mainBtns[0].drawButton();
  mainBtns[1].drawButton();  
}
 
void loop() {
  LCD();
  if(createFile){
    CreateFile();  
  }
  if(logging){
    LogData();  
  }
}

void LCD(){
  // lcd pressure, touch co-ordinates
  TSPoint p = ts.getPoint();
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  if (p.z > 100 && p.z < 1000) { // press if pressure between 10 - 1000
  p.x = (tft.width() - map(p.x, 70, 920, tft.width(), 0));
  p.y = (tft.height()-map(p.y, 120, 900, tft.height(), 0));
  }
  for (uint8_t b=0; b<2; b++) {
    if (mainBtns[b].contains(p.x, p.y)) {
      mainBtns[b].press(true);  // tell the button it is pressed
  } 
  else {
    mainBtns[b].press(false);  // tell the button it is NOT pressed
  }
  if (mainBtns[b].justReleased()) {
    if (mainBtnsActive[b] == false){ mainBtns[b].drawButton();}// draw normal
    else if (mainBtnsActive[b] == true){
      mainBtns[b].drawButton(true);
    }
  }  
  // on click button events
  if (mainBtns[b].justPressed()) { 
    switch(b){
      //scan button
      case 0:
              if(mainBtnsActive[b] == false){
                mainBtnsActive[1] = false;
                mainBtnsActive[b] = true;            
                Scan();
                mainBtnsActive[b] = false;
                mainBtns[1].drawButton();
              }
              else{
                mainBtnsActive[b] = false;
              }
              break;
    // log button
    case 1:
              if(mainBtnsActive[b] == false){
                mainBtnsActive[b] = true; 
                createFile=true;
                starttime = millis();
                tft.fillRect(0, 0, tft.width(), 150, 0x0000);
                tft.setCursor(100,0); tft.setTextSize(2);
                tft.print("Sample Rate: "); tft.print(sampleRate/1000); tft.println("s");
                tft.setCursor(0, 50); tft.setTextSize(4);
                tft.println("   Logging");
                tft.println("   Data...");    
                tft.setTextSize(2);
              }
              else{
                logging = false;
                mainBtnsActive[b] = false;
                tft.fillRect(0, 0, tft.width(), 150, 0x0000);
              }
              break;
    }
delay(200); // UI debouncing
}
}
}

//main buttons
void DataScreen(){
  mainBtns[0].drawButton();
  mainBtns[1].drawButton();
}

// instant scan procedure
void Scan(){
  tft.setCursor(0, 0);
  tft.fillRect(0, 0, tft.width(), 150, 0x0000);
  tft.fillRect(0, 0, tft.width(), 20, 0x0AAA); // horizontal divider fill
  tft.drawLine(0, 20, tft.width(), 20, Cyan); // horizontal divider 
  tft.setTextSize(3); tft.setTextColor(Cyan); 
  tft.println(Time());
  tft.setTextSize(2); tft.setTextColor(0xFFFF);
  tft.print(bme280.getTemperature());  tft.println("C");
  tft.print(bme280.getHumidity());  tft.println("%");
  tft.print(pressure = bme280.getPressure());  tft.println("Pa");
  tft.print(bme280.calcAltitude(pressure));  tft.println("m");  
  tft.setTextColor(0xF99F);
  tft.println("Air Purity");
  tft.setTextColor(0xFFFF);
  tft.println(AirPurity());
  tft.print(DustCon()); tft.println(" pcs/0.01cf");
}

// updates time
String Time(){
  dt = clock.getDateTime();
  return (String)dt.hour+":"+(String)dt.minute+"  "+(String)dt.day+"/"+(String)dt.month+"/"+(String)dt.year;  
}

// air purity 
int AirPurity(){
  return analogRead(purityPin);
}

// calculates concentration
float DustCon(){
    duration = pulseIn(dustPin, LOW);
    lowpulseoccupancy = lowpulseoccupancy+duration;
    ratio = lowpulseoccupancy/(sampleRate*10.0);  
    concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; 
    lowpulseoccupancy = 0;
    Serial.println(concentration);
    return concentration;
  }

// reads info on sd card
void ReadSD(){
  root.openRoot(volume);
  root.ls(LS_R | LS_DATE | LS_SIZE);
  }

// Logs data to SD file
void LogData(){   
  int sampleCount;
  if ((millis()-starttime) >= sampleRate) //sample at sampleRate
  {    
    sampleCount += sampleRate;
    if(storageInit){
      dataFile = SD.open("DataLogs/"+ monthFolder + filename, FILE_WRITE);
      dataFile.println(sampleCount);
      dataFile.print(bme280.getTemperature()); dataFile.println(" C");
      dataFile.print(bme280.getHumidity()); dataFile.println(" %");
      dataFile.print(bme280.getPressure()); dataFile.println(" Pa");
      dataFile.print(bme280.calcAltitude(pressure)); dataFile.println(" m");
      dataFile.println(AirPurity());
      dataFile.print(DustCon()); dataFile.println(" pcs/0.01cf");
      dataFile.close();
      starttime = millis();
    }
  }
}

// Creates folder and file system
void CreateFile(){
  createFile=false;
  Time(); // Update time
  if(storageInit){
    monthFolder = String(dt.month); // Current month
    filename = "/" + String(dt.day)+ "-"+ String(dt.hour) + "-" + String(dt.minute) + ".TXT"; // Data Log file name
  
    Serial.println(filename);
    Serial.println(monthFolder);

    // Create Data Log folder
    if (!SD.exists("DataLogs")){
      SD.mkdir("DataLogs");
      Serial.println("DataLogs created");
    }
    
    // Month folder
    if (!SD.exists("DataLogs/" + monthFolder)){
      SD.mkdir("DataLogs/" + monthFolder);
      Serial.println("MonthFolder created");
    }  
  
    // Create Log
    if (!SD.exists("DataLogs/" + monthFolder + filename)) {
      dataFile = SD.open("DataLogs/"+ monthFolder + filename, FILE_WRITE);
      dataFile.close();
    }
    ReadSD();
    logging=true; // activates logging
  }
}
 
