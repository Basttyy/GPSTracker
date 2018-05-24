#include <EEPROM.h>
#include <AESLib.h> 
#include <SoftwareSerial.h>
#include <time.h>
#include <SD.h>
#include <SPI.h>

#define PASSWORDLEN     16
#define CHUNKSIZE       16
//#define DEBUG


class Logger {
  public:
    Logger();
    void AddData(uint8_t *msg, int msglen);
    void savetime(time_t t);
    void LogGPSPosition(void);
  private:
    void updatetime(void);
    void settime(void);
    void loadpassword(void);
    bool askuser(const char *msg);
    void serialFlush(void);
    void InitTime(void);
    void InitCrypto(void);
    uint8_t password[PASSWORDLEN];
    aes_context ctx;
    File dataFile;
    String streambuffer;
};

Logger *mylogger;
SoftwareSerial GPSInput(2, 3);
time_t time_offset = 0;

void setup()
{
  Serial.begin(9600);
  GPSInput.begin(9600);
  pinMode(15, OUTPUT);
  digitalWrite(15, LOW);
  mylogger = new Logger();
}

void loop()
{
    Serial.println("looping");
    //Save current date to EEPROM
    time_t t = millis() / 1000 + time_offset;
    mylogger->savetime(t);
    const char *strtime = ctime(&t);
    Serial.println(strtime);
    mylogger->AddData((uint8_t *)strtime, strlen(strtime));
    mylogger->AddData((uint8_t *)";", 1);
    //Add position data
    #ifdef DEBUG
    Serial.println("powering up");
    #endif
    digitalWrite(15, HIGH);
    delay(300000);//let the GPS 5 min to sync
    mylogger->LogGPSPosition();
    mylogger->AddData((uint8_t *)"\n", 1);
    #ifdef DEBUG
    Serial.println("powering down");
    #endif
    digitalWrite(15, LOW);
    delay(600000);//Loop time: 10min
}

void Logger::updatetime(void)
{
  char timebuffer[sizeof(time_t)];
  for(int i=0; i<sizeof(time_t); i++)
    timebuffer[i] = (char)EEPROM.read(i + PASSWORDLEN + 1);
  memcpy(&time_offset, timebuffer, sizeof(time_t));
}

void Logger::savetime(time_t t)
{
  char timebuffer[sizeof(time_t)];
  memcpy(timebuffer, &t, sizeof(time_t));
  for(int i=0; i<sizeof(time_t); i++)
    EEPROM.update(i + PASSWORDLEN + 1, timebuffer[i]);
}

void Logger::LogGPSPosition(void)
{
  int c;
  while(1) {
    String pos;
    while(c != 36) {
      c = GPSInput.read();
    }
    //read a line
    while(c != 0xa && pos.length() < 100) {
        if(c > 0)
          pos += (char)c;
        c = GPSInput.read();
    }
    if(strncmp(pos.c_str(), "$GPGLL", 6) == 0) {
      Serial.println(pos);
      AddData((uint8_t *)pos.c_str(), pos.length());
      break;
    }
  }
}

void Logger::loadpassword(void)
{
    for(int i=0; i<PASSWORDLEN; i++)
      password[i] = (uint8_t)EEPROM.read(i);
    password[PASSWORDLEN] = '\x00';
    #ifdef DEBUG
      Serial.print("password=");
      Serial.println((char *)password);
    #endif
}

void Logger::AddData(uint8_t *msg, int msglen)
{
  uint8_t cryptobuf[CHUNKSIZE];
  //populate the string
  for(unsigned int i=0; i<msglen; i++)
    streambuffer += (char)msg[i];
  if(streambuffer.length() > CHUNKSIZE) {
    memcpy(cryptobuf, streambuffer.c_str(), CHUNKSIZE);
    aes192_cbc_enc_continue(ctx, cryptobuf, CHUNKSIZE);
    dataFile.write(cryptobuf, CHUNKSIZE);
    dataFile.flush();
    streambuffer = streambuffer.substring(CHUNKSIZE);
    AddData(NULL, 0);
  }
}

bool Logger::askuser(const char * msg)
{
  serialFlush();
  Serial.print(msg);
  Serial.println("[y/N]");
  for(int i=10; i>0; i--) { 
    int val = Serial.read();
    if(val > 0) {
      Serial.println("");
      if((char)val == 'y')
        return true;
      return false;
    }
    delay(1000);
    Serial.print(i);
    Serial.print(" ");
  }
  Serial.println();
  return false;
}

void Logger::serialFlush(void)
{
  while(Serial.available() > 0)
    Serial.read();
}

void Logger::settime(void)
{
   Serial.println("enter epoch, end with #");
   char timebuf[256] = {0};
   for(unsigned int counter=0; counter<255; counter++) {
    int c = 0;
    while(c <= 0)
      c = Serial.read();
    if((char)c == '#')
      break;
    timebuf[counter] = (char)c;
   }
   #ifdef DEBUG
    Serial.print("# reached=");
    Serial.println(timebuf);
   #endif
   char * pEnd;
   time_offset = strtol(timebuf, &pEnd, 10) - 946684800;
   savetime(time_offset);
}


void Logger::InitTime(void)
{
  if(askuser("set time?"))
    settime();
  else
    updatetime();
}

void Logger::InitCrypto(void)
{
  if(askuser("set new 16 bits pwd?")) {
    int counter = 0;
    while(counter < PASSWORDLEN)
    {
      int val = Serial.read();
      if(val > 0) {
        password[counter] = (uint8_t)val;
        counter++;
      }
    }
    password[counter] = '\x00';
    if(askuser("commit password?")) {
      for(int i=0; i<PASSWORDLEN; i++) {
        EEPROM.update(i, password[i]);
      }
    }
  } else {
    loadpassword();
  }
  ctx = aes128_cbc_enc_start(password,password);
}

Logger::Logger()
{
  InitTime();
  InitCrypto();
  char filename[10];
  snprintf(filename, sizeof(filename), "%x.bin", millis() / 1000 + time_offset);
  while(!SD.begin(4)) {
    Serial.println("SD card not found");
    delay(1000);
  }
  while(!dataFile) {
    dataFile = SD.open(filename, FILE_WRITE);
    Serial.println(filename);
    delay(1000);
  }
}

