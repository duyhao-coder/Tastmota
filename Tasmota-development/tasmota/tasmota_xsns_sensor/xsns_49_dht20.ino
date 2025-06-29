#define XSNS_49              49
#define XI2C_94              94
#define DHT20_ADDR           0x38

struct DHT20 {
  float   temperature;
  float   humidity;
  float   humOffset;
  float   tempOffset;
  uint8_t status;
  uint32_t lastRequest;
  uint32_t lastRead;
  char    name[6] = "DHT20";
} Dht20;

bool Dht20Read(void) {
  if (millis() - Dht20.lastRead < 1000) {
    return false; // avoid too frequent reads
  }

  Wire.beginTransmission(DHT20_ADDR);
  Wire.write(0xAC);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  Dht20.lastRequest = millis();

  delay(80);  // typical measurement time

  Wire.requestFrom(DHT20_ADDR, (uint8_t)7);
  if (Wire.available() < 7) return false;

  uint8_t data[7];
  for (uint8_t i = 0; i < 7; i++) {
    data[i] = Wire.read();
  }

  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < 6; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
  }
  if (crc != data[6]) return false;

  uint32_t raw_h = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);
  uint32_t raw_t = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];

  Dht20.humidity    = (raw_h * 100.0) / 1048576.0 + Dht20.humOffset;
  Dht20.temperature = (raw_t * 200.0) / 1048576.0 - 50 + Dht20.tempOffset;

  Dht20.lastRead = millis();
  Dht20.status = 0;
  return true;
}

void Dht20Detect(void) {
  Wire.beginTransmission(DHT20_ADDR);
  if (Wire.endTransmission() == 0) {
    if (Dht20Read()) {
      I2cSetActiveFound(DHT20_ADDR, Dht20.name);
    }
  }
}

void Dht20EverySecond(void) {
  if ((TasmotaGlobal.uptime & 1) && !Dht20Read()) {
    AddLogMissed(Dht20.name, 0);
  }
}

void Dht20Show(bool json) {
  TempHumDewShow(json, (0 == TasmotaGlobal.tele_period), Dht20.name, Dht20.temperature, Dht20.humidity);
}

bool Xsns49(uint32_t function) {
  if (!I2cEnabled(XI2C_94)) return false;
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      Dht20Detect();
      break;
    case FUNC_EVERY_SECOND:
      Dht20EverySecond();
      break;
    case FUNC_JSON_APPEND:
      Dht20Show(true);
      break;
#ifdef USE_WEBSERVER
    case FUNC_WEB_SENSOR:
      Dht20Show(false);
      break;
#endif
  }
  return result;
}

//#endif  // USE_DHT12
//#endif  // USE_I2C