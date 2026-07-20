// ESP8266 PWM -> fake GPS (NMEA $GPRMC)
// Підключення:
// - PWM сигнал підключити до GPIO14 (D5)
// - UART вивід йде через Serial (TX GPIO1, RX GPIO3)
// - Для перегляду NMEA підключіть USB-TTL або інший UART-пристрій

#define PWM_PIN D5
#define LED1_PIN D2   // вільний пін Wemos D1 Mini (GPIO4)
#define LED2_PIN D1   // вільний пін Wemos D1 Mini (GPIO5)

String appendNmeaChecksum(String sentence) {
  byte checksum = 0;
  for (int i = 1; i < sentence.length(); i++) {
    checksum ^= sentence[i];
  }

  char csBuf[4];
  snprintf(csBuf, sizeof(csBuf), "%02X", checksum);
  sentence += "*" + String(csBuf);
  return sentence;
}

void formatNmeaCoordinate(float decimalDegrees, bool isLongitude, char *buffer, size_t bufferSize) {
  bool negative = decimalDegrees < 0.0f;
  if (negative) {
    decimalDegrees = -decimalDegrees;
  }

  int degrees = (int)decimalDegrees;
  float minutes = (decimalDegrees - degrees) * 60.0f;

  if (isLongitude) {
    snprintf(buffer, bufferSize, "%03d%05.3f", degrees, minutes);
  } else {
    snprintf(buffer, bufferSize, "%02d%05.3f", degrees, minutes);
  }
}

String buildNmeaGga(float latitudeDeg, float longitudeDeg, bool signalPresent) {
  String status = signalPresent ? "1" : "0";
  String satellites = signalPresent ? "04" : "00";
  String hdop = signalPresent ? "1.0" : "9.9";
  String altitude = signalPresent ? "000.00" : "000.00";
  String geoidalSeparation = signalPresent ? "-35.9" : "-35.9";

  char latBuf[16];
  char lonBuf[16];
  formatNmeaCoordinate(latitudeDeg, false, latBuf, sizeof(latBuf));
  formatNmeaCoordinate(longitudeDeg, true, lonBuf, sizeof(lonBuf));

  String sentence = "$GPGGA,094640.000," + String(latBuf) + ",N," + String(lonBuf) + ",E," + status + "," + satellites + "," + hdop + "," + altitude + ",M," + geoidalSeparation + ",M,,";
  return appendNmeaChecksum(sentence);
}

String buildNmeaRmc(float latitudeDeg, float longitudeDeg, bool signalPresent) {
  String status = signalPresent ? "A" : "V";
  char latBuf[16];
  char lonBuf[16];
  formatNmeaCoordinate(latitudeDeg, false, latBuf, sizeof(latBuf));
  formatNmeaCoordinate(longitudeDeg, true, lonBuf, sizeof(lonBuf));

  String sentence = "$GPRMC,094640.000," + status + "," + String(latBuf) + ",N," + String(lonBuf) + ",E,0.4,99.3,050119,,,A";
  return appendNmeaChecksum(sentence);
}

bool readPwm(float &frequencyHz, float &dutyPercent) {
  unsigned long highUs = pulseIn(PWM_PIN, HIGH, 500000);
  unsigned long lowUs = pulseIn(PWM_PIN, LOW, 500000);

  if (highUs == 0 || lowUs == 0) {
    return false;
  }

  unsigned long periodUs = highUs + lowUs;
  if (periodUs == 0) {
    return false;
  }

  frequencyHz = 1000000.0f / periodUs;
  dutyPercent = (highUs * 100.0f) / periodUs;
  return true;
}

void setLedStateFromPwm(float pwmValue) {
  int pwmValueInt = (int)round(pwmValue);

  bool led1Active = (pwmValueInt >= 400 && pwmValueInt <= 1100);
  bool led2Active = (pwmValueInt >= 1800 && pwmValueInt <= 2100);

  digitalWrite(LED1_PIN, led1Active ? HIGH : LOW);
  digitalWrite(LED2_PIN, led2Active ? HIGH : LOW);
}

void setup() {
  pinMode(PWM_PIN, INPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP8266 PWM -> NMEA GPRMC started");
}

void loop() {
  float freqHz = 0.0f;
  float duty = 0.0f;

  bool signalPresent = readPwm(freqHz, duty);

  if (signalPresent) {
    // Простий переклад PWM у «фейкові координати».
    // Лат/лон перетворюються у формат NMEA, сумісний з GPS-пристроями.
    float latitudeDeg = 30.0f + duty * 0.02f;          // приклад: 50% -> 30.50°
    float longitudeDeg = 0.0f + (freqHz / 10000.0f);   // приклад: 1000 Hz -> 0.10°

    Serial.print("PWM: freq=");
    Serial.print(freqHz, 1);
    Serial.print(" Hz, duty=");
    Serial.print(duty, 2);
    Serial.println(" %");

    setLedStateFromPwm(freqHz);

    String gga = buildNmeaGga(latitudeDeg, longitudeDeg, true);
    String rmc = buildNmeaRmc(latitudeDeg, longitudeDeg, true);

    Serial.println(gga);
    Serial.println(rmc);
  } else {
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);
    Serial.println("PWM signal not detected");
  }

  delay(1000);
}
