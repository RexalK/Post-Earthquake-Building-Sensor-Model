#include <SPI.h>
#include <math.h>

#define CS0 10
#define CS1 15

#define BUS_A_SCLK 12
#define BUS_A_MISO 13
#define BUS_A_MOSI 11

#define BUS_B_SCLK 4
#define BUS_B_MISO 5
#define BUS_B_MOSI 6

#define POWER_CTL   0x2D
#define DATA_FORMAT 0x31
#define BW_RATE     0x2C
#define DATAX0      0x32

SPIClass spiA(FSPI);
SPIClass spiB(HSPI);

SPISettings adxlSPI(
  100000,
  MSBFIRST,
  SPI_MODE3
);

void writeRegister(
  SPIClass &bus,
  byte csPin,
  byte reg,
  byte value
) {

  bus.beginTransaction(adxlSPI);

  digitalWrite(csPin, LOW);

  bus.transfer(reg);
  bus.transfer(value);

  digitalWrite(csPin, HIGH);

  bus.endTransaction();
}

void readXYZ(
  SPIClass &bus,
  byte csPin,
  int16_t &x,
  int16_t &y,
  int16_t &z
) {

  bus.beginTransaction(adxlSPI);

  digitalWrite(csPin, LOW);

  bus.transfer(
    0x80 |
    0x40 |
    DATAX0
  );

  byte x0 = bus.transfer(0x00);
  byte x1 = bus.transfer(0x00);

  byte y0 = bus.transfer(0x00);
  byte y1 = bus.transfer(0x00);

  byte z0 = bus.transfer(0x00);
  byte z1 = bus.transfer(0x00);

  digitalWrite(csPin, HIGH);

  bus.endTransaction();

  x =
    (int16_t)(
      (x1 << 8) |
      x0
    );

  y =
    (int16_t)(
      (y1 << 8) |
      y0
    );

  z =
    (int16_t)(
      (z1 << 8) |
      z0
    );
}

void readG(
  SPIClass &bus,
  byte csPin,
  float &gx,
  float &gy,
  float &gz
) {

  int16_t x;
  int16_t y;
  int16_t z;

  readXYZ(
    bus,
    csPin,
    x,
    y,
    z
  );

  gx = x * 0.0039;
  gy = y * 0.0039;
  gz = z * 0.0039;
}

void setupSensor(
  SPIClass &bus,
  byte csPin
) {

  // Full resolution +/-2g
  writeRegister(
    bus,
    csPin,
    DATA_FORMAT,
    0x08
  );

  // 100 Hz
  writeRegister(
    bus,
    csPin,
    BW_RATE,
    0x0A
  );

  // Measurement mode
  writeRegister(
    bus,
    csPin,
    POWER_CTL,
    0x08
  );
}

void printSensor(
  const char* name,
  float x,
  float y,
  float z
) {

  float total =
    sqrt(
      x * x +
      y * y +
      z * z
    );

  Serial.print(name);

  Serial.print(" | X: ");
  Serial.print(x, 3);

  Serial.print(" g | Y: ");
  Serial.print(y, 3);

  Serial.print(" g | Z: ");
  Serial.print(z, 3);

  Serial.print(" g | Total: ");
  Serial.print(total, 3);

  Serial.println(" g");
}

void setup() {

  Serial.begin(115200);

  delay(2000);

  pinMode(CS0, OUTPUT);
  pinMode(CS1, OUTPUT);

  digitalWrite(CS0, HIGH);
  digitalWrite(CS1, HIGH);

  spiA.begin(
    BUS_A_SCLK,
    BUS_A_MISO,
    BUS_A_MOSI,
    CS0
  );

  spiB.begin(
    BUS_B_SCLK,
    BUS_B_MISO,
    BUS_B_MOSI,
    CS1
  );

  setupSensor(
    spiA,
    CS0
  );

  setupSensor(
    spiB,
    CS1
  );

  Serial.println();
  Serial.println(
    "SafeTag Two Sensor g Test"
  );
}

void loop() {

  float x0;
  float y0;
  float z0;

  float x1;
  float y1;
  float z1;

  readG(
    spiA,
    CS0,
    x0,
    y0,
    z0
  );

  readG(
    spiB,
    CS1,
    x1,
    y1,
    z1
  );

  printSensor(
    "Floor 1",
    x0,
    y0,
    z0
  );

  printSensor(
    "Floor 2",
    x1,
    y1,
    z1
  );

  Serial.println(
    "--------------------------------"
  );

  delay(250);
}
