#include <ESP8266WiFi.h>
#include <Wire.h>              //use bug free i2c driver https://github.com/enjoyneering/ESP8266-I2C-Driver
#include <HTU21D.h>
#include <BMP180.h>
#include <BH1750.h>

const char* ssid = "35246e";
const char* password = "242536845CAYBX";
const float STATION_ELEV = 100;
const long SLEEP_INTERVAL = 20;
const char IOT_SERVER[] = "192.168.0.15";
const char IOT_SERVER_PORT = 80; //22
const char table_name[] = "JKM";
const char write_api_key[] = "przecietniaka";

String unitStatus = "";
struct sensorData
{
  float stationPressure;          // measured station pressure in hPa
  float seaLevelPressure;         // calculated SLP
  float temperature;              // degrees Celsius
  float humidity;                 // relative humidity %
  unsigned int lightIntensity;    // lux
  float cellVoltage;              // volts
} rawData;              // declare struct variable

HTU21D  myHTU21D(HTU21D_RES_RH12_TEMP14);
BMP180  myBMP(BMP180_ULTRALOWPOWER);
BH1750  light_meter;
WiFiClient client;

// the setup function runs once when you press reset or power the board
void setup(){
  Serial.begin(115200);
  loguj_do_wifi();
  ogarnij_czujniki();
  rawData = odczyt_danych();
  printToSerialPort(rawData);
  postToRPi(rawData);
  enterSleep(SLEEP_INTERVAL);
}

// the loop function runs over and over again forever
void loop()
{}

void loguj_do_wifi() {
  WiFi.begin(ssid, password);
  Serial.println("");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
   }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void ogarnij_czujniki(){
  while (myHTU21D.begin() != true)
  {
    Serial.print(F("HTU21D error"));
    delay(5000);
  }
  Serial.println(F("HTU21D okay"));
  
  while (myBMP.begin(D2, D1) != true) //sda, scl
  {
    Serial.println(F("Bosch BMP180 error"));
    delay(5000);
  }
  Serial.println(F("Bosch BMP180 okay"));

  light_meter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
}

sensorData odczyt_danych(){

  sensorData tempData ={0};
  int samples = 3;
  float t = 0;          // temperature C
  float t_elev = 0;          // temperature C
  float h = 0;          // humidity %
  float sp = 0;         // station pressure mb or hPa
  float slp = 0;        // sea level pressure
  unsigned int li = 0;  // light intensity lux
  float cv = 0;         // cell voltage

  for (int i = 0; i < samples; i++)
  {
    t_elev = myBMP.getTemperature();
    t = myHTU21D.readTemperature();
    h = myHTU21D.readCompensatedHumidity();
    sp = binary_to_decimal(myBMP.getPressure());
    slp = calculateSeaLevelPressure(t_elev, sp, STATION_ELEV);
    li = light_meter.readLightLevel();
 
    // read analog voltage from the Analog to Digital Converter
    // on D1 Mini this is 0 - 1023 for voltages 0 to 3.2V
    // the D1M-WX1 has an external resistor to extend the range to 5.0 Volts
    cv = 5.0 * analogRead(A0) / 1023.0;
 
    // accumulate the values of each sensor
    tempData.temperature += t;
    tempData.humidity += h;
    tempData.stationPressure += sp;
    tempData.seaLevelPressure += slp;
    tempData.lightIntensity += li;
    tempData.cellVoltage += cv;
 
    delay(50);   // provide some delay to let sensors settle
  } // for()
 
  // divide the accumulated values by the number of samples
  // to get an average
  tempData.temperature /= (float)samples;
  tempData.humidity /= (float)samples;
  tempData.stationPressure /= (float)samples;
  tempData.seaLevelPressure /= (float)samples;
  tempData.lightIntensity /= (long)samples;
  tempData.cellVoltage /= (float)samples;
 
  return tempData;
}

void printToSerialPort(sensorData dataRaw)
{
  // '\t' is the C++ escape sequence for tab
  // header line
  Serial.println("\t°C\t%\thPa\tSLP hPa\tLux\tV");
  // data line
  Serial.print("Dane\t");
  Serial.print(dataRaw.temperature, 2);
  Serial.print("\t");
  Serial.print(dataRaw.humidity, 2);
  Serial.print("\t");
  Serial.print(dataRaw.stationPressure, 2);
  Serial.print("\t");
  Serial.print(dataRaw.seaLevelPressure, 2);
  Serial.print("\t");
  Serial.print(dataRaw.lightIntensity);
  Serial.print("\t");
  Serial.println(dataRaw.cellVoltage, 2);
  Serial.println("----------------------------------------------------");
}

void postToRPi(sensorData data)
{
  // assemble and post the data
  if ( client.connect(IOT_SERVER, IOT_SERVER_PORT) == true )
  {
    Serial.println("Połączono z serwerem RPi.");
 
    // get the data to RPi
    client.print( "GET /espdata.php?");
    client.print("api_key=");
    client.print( write_api_key );
    client.print("&&");
    client.print("station_id=");
    client.print( table_name );
    client.print("&&");
    client.print("t=");
    client.print( data.temperature );
    client.print("&&");
    client.print("h=");
    client.print( data.humidity );
    client.print("&&");
    client.print("ap=");
    client.print( data.stationPressure );
    client.print("&&");
    client.print("rp=");
    client.print( data.seaLevelPressure );
    client.print("&&");
    client.print("li=");
    client.print( data.lightIntensity );
    client.print("&&");
    client.print("vl=");
    client.print( data.cellVoltage );
    client.println( " HTTP/1.1");
    client.println( "Host: localhost" );
    client.println( "Content-Type: application/x-www-form-urlencoded" );
    client.println( "Connection: close" );
    client.println();
    client.println();
    Serial.println("Wysłano dane na serwer RPi.");
  }
  client.stop();
  Serial.println("Rozłączono z serwerem RPi.");
}

void enterSleep(long sleep)
{
  // sleep is in seconds
  Serial.print("Wejście w tryb głębokiego snu na: ");
  Serial.print(sleep);
  Serial.println(" sekund.");
  delay(200);                       // delay to let things settle
  // WAKE_RF_DEFAULT wakes the ESP8266 with WiFi enabled
  ESP.deepSleep(sleep * 1000000L, WAKE_RF_DEFAULT);
}

float calculateSeaLevelPressure(float celsius, float stationPressure, float elevation)
{
  float slP = stationPressure / pow(2.718281828, -(elevation / ((273.15 + celsius) * 29.263)));
  return slP;
}

float binary_to_decimal(int binary){
  int t = 0;
  int wynik = 0;
  int mask = 00000000000000001;
  for(int i=0; i<17; i++){
    wynik = binary & mask;
    t += wynik;
    mask <<= 1;
  }
  return t/100;
}
