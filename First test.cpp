#include <SPI.h>

#define CS0 10
#define CS1 15

#define BUS_A_SCLK 12
#define BUS_A_MISO 13
#define BUS_A_MOSI 11

#define BUS_B_SCLK 4
#define BUS_B_MISO 5
#define BUS_B_MOSI 6

#define DEVID 0x00

SPIClass spiA(FSPI);
SPIClass spiB(HSPI);

SPISettings adxlSPI(
  100000,
  MSBFIRST,
  SPI_MODE3
);

byte readRegister(
  SPIClass &bus,
  byte csPin,
  byte reg
) {

  bus.beginTransaction(adxlSPI);

  digitalWrite(csPin, LOW);

  bus.transfer(0x80 | reg);

  byte value =
    bus.transfer(0x00);

  digitalWrite(csPin, HIGH);

  bus.endTransaction();

  return value;
}

void printSensor(
  const char* name,
  byte id
) {

  Serial.print(name);
  Serial.print(" ID: 0x");

  if (id < 0x10) {
    Serial.print("0");
  }

  Serial.print(id, HEX);

  if (id == 0xE5) {
    Serial.println(" PASS");
  }

  else {
    Serial.println(" FAIL");
  }
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

  Serial.println();
  Serial.println("Two Sensor ID Test");
}

void loop() {

  byte id0 =
    readRegister(
      spiA,
      CS0,
      DEVID
    );

  byte id1 =
    readRegister(
      spiB,
      CS1,
      DEVID
    );

  printSensor(
    "Floor 1",
    id0
  );

  printSensor(
    "Floor 2",
    id1
  );

  Serial.println(
    "----------------"
  );

  delay(1000);
}
