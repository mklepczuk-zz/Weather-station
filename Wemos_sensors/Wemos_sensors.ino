#include <ESP8266WiFi.h>
#include <Wire.h>
#include <HTU21D.h>
#include <BMP180.h>
#include <BH1750.h>

const char* ssid = "35246e";				//nazwa sieci Wi-Fi
const char* password = "242536845CAYBX";	//hasło do Wi-Fi
const float ELEVATION = 100;				//wysokość stacji nad poziomem morza
const long  SLEEP_DURATION = 305;			//czas uśpienia w sekundach
const char  IP_ADDRESS[] = "192.168.0.15";	//adres IP serwera z bazą danych
const char  PORT = 80;						//port serwera
const char  SQL_table[] = "JKM";			//nazwa tabeli w bazie danych
const char  SQL_pass[] = "przecietniaka";	//hasło do bazy danych

struct weatherStationData					//struktura przechowująca dane odczytane ze stacji
{
  float pressure;          					//ciśnienie atmosferyczne w hPa
  float pressureOnTheSeaLevel;         		//ciśnienie na poziomie morza w hPa
  float temp;              					//temperatura w °C
  float humi;                 				//wilgotność w %
  unsigned int sunIntensity;    			//natężenie oświetlenia w luxach
  float accumulatorVoltage;              	//napięcie akumulatora
} measuredData;              				

HTU21D  myHTU21D(HTU21D_RES_RH12_TEMP14);	//inicjalizacja czujnika HTU21D
BMP180  myBMP(BMP180_ULTRALOWPOWER);		//inicjalizacja czujnika BMP180
BH1750  light_meter;						//inicjalizacja czujnika BH1750
WiFiClient raspberry;						//inicjalizacja klienta WiFi

// the setup function runs once when you press reset or power the board
void setup(){
  Serial.begin(115200);
  wifi_login();
  sensors_set_up();
  measuredData = read_data();
  //printToSerialPort(measuredData);
  postToRPi(measuredData);
  enterSleep(SLEEP_DURATION);
}

// the loop function runs over and over again forever
void loop()
{}

void wifi_login() {
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

void sensors_set_up(){

  myHTU21D.begin();
  myBMP.begin(D2, D1);
  light_meter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);
  
  /*while ( != true)
  {
    Serial.print(F("HTU21D error"));
    delay(5000);
  }
  Serial.println(F("HTU21D okay"));
  
  while ( != true) //sda, scl
  {
    Serial.println(F("Bosch BMP180 error"));
    delay(5000);
  }
  Serial.println(F("Bosch BMP180 okay"));*/
}

weatherStationData read_data(){

  weatherStationData tempData ={0};
  int samples = 3;
  float t = 0;          // temperature C
  float h = 0;          // humidity %
  float sp = 0;         // station pressure mb or hPa
  float slp = 0;        // sea level pressure
  unsigned int li = 0;  // light intensity lux
  float cv = 0;         // cell voltage

  for (int i = 0; i < samples; i++)
  {
    t = myHTU21D.readTemperature();
    h = myHTU21D.readCompensatedHumidity();
    sp = binary_to_decimal(myBMP.getPressure());
    slp = calculatePressureOnTheSeaLevel(sp, ELEVATION);
    li = light_meter.readLightLevel();
    cv = 5.0 * analogRead(A0) / 1023.0;
    
    tempData.temp += t;
    tempData.humi += h;
    tempData.pressure += sp;
    tempData.pressureOnTheSeaLevel += slp;
    tempData.sunIntensity += li;
    tempData.cellVoltage += cv;
 
    delay(30);   // provide some delay to let sensors settle
  }
  tempData.temp /= (float)samples;
  tempData.humi /= (float)samples;
  tempData.pressure /= (float)samples;
  tempData.pressureOnTheSeaLevel /= (float)samples;
  tempData.sunIntensity /= (long)samples;
  tempData.cellVoltage /= (float)samples;
 
  return tempData;
}

void postToRPi(weatherStationData data)
{
  // assemble and post the data
  if ( raspberry.connect(IP_ADDRESS, PORT) == true )
  {
    Serial.println("Połączono z serwerem RPi.");
 
    raspberry.print( "GET /espdata.php?");
    raspberry.print("api_key=");
    raspberry.print( SQL_pass );
    raspberry.print("&&");
    raspberry.print("station_id=");
    raspberry.print( SQL_table );
    raspberry.print("&&");
    raspberry.print("t=");
    raspberry.print( data.temp );
    raspberry.print("&&");
    raspberry.print("h=");
    raspberry.print( data.humi );
    raspberry.print("&&");
    raspberry.print("ap=");
    raspberry.print( data.pressure );
    raspberry.print("&&");
    raspberry.print("rp=");
    raspberry.print( data.pressureOnTheSeaLevel );
    raspberry.print("&&");
    raspberry.print("li=");
    raspberry.print( data.sunIntensity );
    raspberry.print("&&");
    raspberry.print("vl=");
    raspberry.print( data.cellVoltage );
    raspberry.println( " HTTP/1.1");
    raspberry.println( "Host: localhost" );
    raspberry.println( "Content-Type: application/x-www-form-urlencoded" );
    raspberry.println( "Connection: close" );
    raspberry.println();
    raspberry.println();
    Serial.println("Wysłano dane na serwer RPi.");
  }
  raspberry.stop();
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

float calculatePressureOnTheSeaLevel(float pressure, float elevation)
{
  float slP = pressure / (pow((1-elevation/44330),5.255));
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
  return (float)t/100.0;
}

void printToSerialPort(weatherStationData dataRaw)
{
  // '\t' is the C++ escape sequence for tab
  // header line
  Serial.println("\t°C\t%\thPa\tSLP hPa\tLux\tV");
  // data line
  Serial.print("Dane\t");
  Serial.print(dataRaw.temp, 2);
  Serial.print("\t");
  Serial.print(dataRaw.humi, 2);
  Serial.print("\t");
  Serial.print(dataRaw.pressure, 2);
  Serial.print("\t");
  Serial.print(dataRaw.pressureOnTheSeaLevel, 2);
  Serial.print("\t");
  Serial.print(dataRaw.sunIntensity);
  Serial.print("\t");
  Serial.println(dataRaw.cellVoltage, 2);
  Serial.println("----------------------------------------------------");
}
