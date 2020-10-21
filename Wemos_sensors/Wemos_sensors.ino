#include <ESP8266WiFi.h>
#include <Wire.h>
#include <HTU21D.h>
#include <BMP180.h>
#include <BH1750.h>

const char* SSID = "35246e";				//nazwa sieci Wi-Fi
const char* PASSWORD = "242536845CAYBX";	//hasło do Wi-Fi
const float ELEVATION = 100;				//wysokość stacji nad poziomem morza
const long  SLEEP_DURATION = 303;			//czas uśpienia w sekundach
const char  IP_ADDRESS[] = "192.168.0.15";	//adres IP serwera z bazą danych
const char  PORT = 80;						//port serwera
const char  SQL_TABLE[] = "JKM";			//nazwa tabeli w bazie danych
const char  SQL_PASS[] = "przecietniaka";	//hasło do bazy danych
float 		batteryVoltage = 0;         	//napięcie baterii

struct weatherStationData					//struktura przechowująca dane odczytane
{											//z czujników
  float pressure;          					//ciśnienie atmosferyczne w hPa
  float pressureOnTheSeaLevel;         		//ciśnienie na poziomie morza w hPa
  float temp;              					//temperatura w °C
  float humi;                 				//wilgotność w %
  unsigned int sunIntensity;    			//natężenie oświetlenia w luxach
  float accumulatorVoltage;              	//napięcie akumulatora
} measuredData;              				

HTU21D  myHTU21D(HTU21D_RES_RH12_TEMP14);	//utworzenie obiektu HTU21D
BMP180  myBMP180(BMP180_ULTRALOWPOWER);		//utworzenie obiektu BMP180
BH1750  light_meter;						//utworzenie obiektu BH1750
WiFiClient raspberry;						//inicjalizacja klienta WiFi

void setup(){								//program główny
  Serial.begin(115200);
  WiFi.begin(ssid, password);				//logowanie do sieci WiFi
  sensorsSetUp();							//konfiguracja połączenia z czujnikami
  measuredData = readData();				//odczyt danych z czujników
  //printToSerialPort(measuredData);
  sendDataToRPi(measuredData);				//wyślij dane do RaspberryPi
  goToSleep(SLEEP_DURATION);				//przejdź w stan uśpienia
}

void loop() {}								//nieużywane, bo mikrokontroler za każdym razem jest usypiany
/*void wifiLogin() {
	Serial.println("");
	while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
	}
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}*/

void sensorsSetUp(){						//inicjalizacja czujników

  myHTU21D.begin();							
  myBMP180.begin(D2, D1);					//SDA,SCL
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

weatherStationData readData(){

  weatherStationData tempData ={0};
  int samples = 6;
  float t = 0;          // temperatura w °C
  float h = 0;          // wilgotność w %
  float sp = 0;         // ciśnienie atmosferyczne w hPa
  float slp = 0;        // ciśnienie na poziomie morza w hPa
  unsigned int li = 0;  // natężenie oświetlenia w luxach

  for (int i = 0; i < samples; i++)
  {
    t = myHTU21D.readTemperature();
    h = myHTU21D.readCompensatedHumidity();
    sp = binaryToDecimal(myBMP180.getPressure());
    slp = calculatePressureOnTheSeaLevel(sp, ELEVATION);
    li = light_meter.readLightLevel();
    
    tempData.temp += t;
    tempData.humi += h;
    tempData.pressure += sp;
    tempData.pressureOnTheSeaLevel += slp;
    tempData.sunIntensity += li;
 
    delay(30);
  }
  tempData.temp /= (float)samples;
  tempData.humi /= (float)samples;
  tempData.pressure /= (float)samples;
  tempData.pressureOnTheSeaLevel /= (float)samples;
  tempData.sunIntensity /= (long)samples;
  batteryVoltage = 5.0 * analogRead(A0) / 1023.0;
  tempData.accumulatorVoltage = batteryVoltage;
 
  return tempData;
}

void sendDataToRPi(weatherStationData data)
{
  if ( raspberry.connect(IP_ADDRESS, PORT) == true )
  {
 
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
    raspberry.print( data.accumulatorVoltage );
    raspberry.println( " HTTP/1.1");
    raspberry.println( "Host: localhost" );
    raspberry.println( "Content-Type: application/x-www-form-urlencoded" );
    raspberry.println( "Connection: close" );
    raspberry.println();
    raspberry.println();
  }
  raspberry.stop();
}

void goToSleep(long sleep)
{
  /*Serial.print("Wejście w tryb głębokiego snu na: ");
  Serial.print(sleep);
  Serial.println(" sekund.");*/
  delay(200);                       					// delay to let things settle
  if(batteryVoltage > 3.0)
	ESP.deepSleep(sleep * 1000000L, WAKE_RF_DEFAULT);		// WAKE_RF_DEFAULT wakes the ESP8266 with WiFi enabled
  else if (batteryVoltage < 3.0 and batteryVoltage > 2.9)
	ESP.deepSleep(606000000L, WAKE_RF_DEFAULT);
  else if (batteryVoltage < 2.9 and batteryVoltage > 2.8)
	ESP.deepSleep(909000000L, WAKE_RF_DEFAULT);
  else if (batteryVoltage < 2.8)
	ESP.deepSleep(1818000000L, WAKE_RF_DEFAULT);
}

float calculatePressureOnTheSeaLevel(float pressure, float elevation)
{
  float slP = pressure / (pow((1-elevation/44330),5.255));	//wzór pozwalający na obliczenie ciśnienia na poziomie morza
  return slP;
}

float binaryToDecimal(int binary){
  int wynik = 0;
  int mask = 00000000000000001;				//maska, która pozwala na konwersję
  for(int i=0; i<17; i++){					//iloczyn logiczny pozwala na wydobycie wartości kolejnego
    wynik += (binary & mask);				//bitu, który jest sumowany w każdej iteracji
    mask <<= 1;								//przesunięcie maski, aby uzyskać wartość kolejengo bitu
  }
  return (float)wynik/100.0;				//wynik dzielony przez 100, aby uzyskać ciśnienie w hPa
}

void printToSerialPort(weatherStationData dataRaw)
{
  Serial.println("\t°C\t%\thPa\tSLP hPa\tLux\tV");
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
  Serial.println(dataRaw.accumulatorVoltage, 2);
  Serial.println("----------------------------------------------------");
}
