#include <SPI.h>

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

  // Read + multi-byte
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

  // Measurement mode
  writeRegister(
    bus,
    csPin,
    POWER_CTL,
    0x08
  );
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

  Serial.println(
    "Two Sensor Raw XYZ Test"
  );
}

void loop() {

  int16_t x0;
  int16_t y0;
  int16_t z0;

  int16_t x1;
  int16_t y1;
  int16_t z1;

  readXYZ(
    spiA,
    CS0,
    x0,
    y0,
    z0
  );

  readXYZ(
    spiB,
    CS1,
    x1,
    y1,
    z1
  );

  Serial.print("Floor 1 | X: ");
  Serial.print(x0);

  Serial.print(" Y: ");
  Serial.print(y0);

  Serial.print(" Z: ");
  Serial.println(z0);


  Serial.print("Floor 2 | X: ");
  Serial.print(x1);

  Serial.print(" Y: ");
  Serial.print(y1);

  Serial.print(" Z: ");
  Serial.println(z1);


  Serial.println(
    "----------------------------"
  );

  delay(250);
}
