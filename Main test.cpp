#include <SPI.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// SAFETAG - FINAL
// Wireless Structural Search & Rescue Monitor
// =====================================================


// =====================================================
// SENSOR 0 = FLOOR 1
// =====================================================
#define CS0 10
#define BUS_A_SCLK 12
#define BUS_A_MISO 13
#define BUS_A_MOSI 11


// =====================================================
// SENSOR 1 = FLOOR 2
// =====================================================
#define CS1 15
#define BUS_B_SCLK 4
#define BUS_B_MISO 5
#define BUS_B_MOSI 6


SPIClass spiA(FSPI);
SPIClass spiB(HSPI);

SPISettings adxlSPI(
  100000,
  MSBFIRST,
  SPI_MODE3
);


// =====================================================
// ADXL345 REGISTERS
// =====================================================
#define DEVID       0x00
#define BW_RATE     0x2C
#define POWER_CTL   0x2D
#define DATA_FORMAT 0x31
#define DATAX0      0x32


// =====================================================
// WIFI
// =====================================================
const char* AP_SSID = "SafeTag-Rescue-01";
const char* AP_PASSWORD = "SafeTag123";

WebServer server(80);

bool webServerReady = false;


// =====================================================
// HEALTHY BASELINE
// =====================================================
float baseX0 = 0;
float baseY0 = 0;
float baseZ0 = 0;

float baseX1 = 0;
float baseY1 = 0;
float baseZ1 = 0;


// =====================================================
// EARTHQUAKE START DETECTION
// =====================================================

float SHAKE_START = 0.050;

// Must stay above threshold continuously
const unsigned long SHAKE_START_TIME = 180;

unsigned long shakeStartTimer = 0;

float candidateMaxMotion = 0.0;


// =====================================================
// EARTHQUAKE CLASSIFICATION
// =====================================================
float SHAKE_MODERATE = 0.060;
float SHAKE_STRONG   = 0.120;


// =====================================================
// EARTHQUAKE END
// =====================================================

float SHAKE_END = 0.040;

// Fast transition to post-earthquake
unsigned long SETTLE_TIME = 800;

unsigned long quietStart = 0;


// =====================================================
// STRUCTURAL THRESHOLDS
// =====================================================
float TILT_WARNING = 1.50;
float TILT_DANGER  = 3.00;

float WARNING_TO_SAFE = 1.00;
float DANGER_TO_WARNING = 2.40;


// =====================================================
// STRUCTURAL FILTER
// =====================================================
float filteredBasementTilt = 0.0;
float filteredFloorTilt = 0.0;

bool postFilterInitialized = false;

// 0 = SAFE
// 1 = WARNING
// 2 = DANGER
int basementState = 0;
int floorState = 0;

const float FILTER_ALPHA = 0.12;


// =====================================================
// STATE CONFIRMATION
// =====================================================
const int STATE_CONFIRM_COUNT = 5;

int basementCandidate = 0;
int basementCandidateCount = 0;

int floorCandidate = 0;
int floorCandidateCount = 0;


// =====================================================
// AUTO RESET
// =====================================================
const int SAFE_RESET_COUNT = 30;

int consecutiveSafeChecks = 0;


// =====================================================
// TIMING
// =====================================================
unsigned long lastPrint = 0;

const unsigned long PRINT_INTERVAL = 250;


// =====================================================
// SYSTEM STATE
// =====================================================
bool baselineReady = false;

bool earthquakeActive = false;
bool postEarthquakeMode = false;

float eventMaxMotion = 0.0;
float lastEventMaxMotion = 0.0;

float peakBasement = 0.0;
float peakFloor1 = 0.0;


// =====================================================
// LIVE PHONE VALUES
// =====================================================
float liveBasementMotion = 0.0;
float liveFloorMotion = 0.0;

float liveBasementTilt = 0.0;
float liveFloorTilt = 0.0;


// =====================================================
// DASHBOARD HTML
// =====================================================
const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>

<head>

<meta charset="UTF-8">

<meta
  name="viewport"
  content="width=device-width, initial-scale=1, maximum-scale=1"
>

<title>SafeTag Rescue</title>

<style>

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  padding: 0;

  background: #080a0d;
  color: #ffffff;

  font-family: Arial, Helvetica, sans-serif;
}

.container {
  max-width: 780px;
  margin: auto;
  padding: 16px;
}


/* HEADER */

.header {
  padding: 8px 2px 16px 2px;
}

.brand {
  font-size: 32px;
  font-weight: 900;
}

.subtitle {
  color: #8e99a7;
  margin-top: 3px;
  font-size: 14px;
}

.link {
  margin-top: 10px;
  font-size: 13px;
  font-weight: bold;
  color: #64df8c;
}

.dot {
  width: 9px;
  height: 9px;
  display: inline-block;
  border-radius: 50%;
  background: #64df8c;
  margin-right: 6px;
}


/* RESCUE COMMAND */

.command {
  background: #15191f;
  border: 2px solid #343c46;
  border-radius: 18px;
  padding: 20px;
  margin-bottom: 14px;
}

.commandLabel {
  color: #8e99a7;
  font-size: 12px;
  letter-spacing: 1.4px;
  font-weight: bold;
}

.commandText {
  font-size: 30px;
  line-height: 1.08;
  font-weight: 900;
  margin-top: 8px;
}


/* PRIORITY */

.priorityBox {
  border-radius: 18px;
  padding: 18px;
  margin-bottom: 14px;
  text-align: center;
  background: #15191f;
  border: 1px solid #343c46;
}

.priorityLabel {
  font-size: 12px;
  color: #8e99a7;
  letter-spacing: 1.4px;
  font-weight: bold;
}

.priorityValue {
  font-size: 40px;
  margin-top: 5px;
  font-weight: 900;
}


/* SYSTEM */

.system {
  background: #15191f;
  border: 1px solid #343c46;
  border-radius: 16px;
  padding: 16px;
  margin-bottom: 14px;
}

.systemLabel {
  color: #8e99a7;
  font-size: 12px;
  letter-spacing: 1.2px;
  font-weight: bold;
}

.systemValue {
  margin-top: 6px;
  font-size: 25px;
  font-weight: 800;
}


/* CARDS */

.grid {
  display: grid;

  grid-template-columns:
    repeat(
      auto-fit,
      minmax(220px, 1fr)
    );

  gap: 12px;
}

.card {
  background: #15191f;
  border: 1px solid #343c46;
  border-radius: 16px;
  padding: 17px;
}

.cardTitle {
  color: #8e99a7;
  font-size: 12px;
  letter-spacing: 1.2px;
  font-weight: bold;
}

.floorName {
  font-size: 20px;
  font-weight: 800;
  margin-top: 7px;
}

.floorState {
  font-size: 31px;
  font-weight: 900;
  margin-top: 8px;
}

.measure {
  margin-top: 10px;
  font-size: 16px;
  color: #d5d8dd;
}

.number {
  font-weight: bold;
  color: white;
}


/* EVENT */

.eventLevel {
  font-size: 30px;
  font-weight: 900;
  margin-top: 8px;
}

.eventMotion {
  font-size: 17px;
  margin-top: 9px;
}


/* SAFE COUNTER */

.counter {
  font-size: 25px;
  font-weight: 800;
  margin-top: 8px;
}

.progressOuter {
  width: 100%;
  height: 11px;
  background: #292f37;
  margin-top: 12px;
  border-radius: 20px;
  overflow: hidden;
}

.progressInner {
  width: 0%;
  height: 100%;
  background: #64df8c;
  transition: width 0.2s linear;
}


/* COLORS */

.safe {
  color: #64df8c;
}

.warning {
  color: #ffd166;
}

.danger {
  color: #ff5656;
}

.neutral {
  color: #64b5f6;
}


/* INFO */

.info {
  background: #101318;
  border: 1px solid #292f37;
  border-radius: 14px;
  padding: 14px;
  margin-top: 14px;
  color: #929ba7;
  font-size: 13px;
  line-height: 1.4;
}

.footer {
  color: #555e68;
  font-size: 11px;
  text-align: center;
  margin-top: 18px;
  padding-bottom: 20px;
}

.offline {
  color: #ff5656;
}

</style>

</head>


<body>

<div class="container">


<!-- HEADER -->

<div class="header">

  <div class="brand">
    SafeTag
  </div>

  <div class="subtitle">
    Structural Search &amp; Rescue Monitor
  </div>

  <div
    id="connection"
    class="link">

    <span class="dot"></span>
    CONNECTING

  </div>

</div>


<!-- RESCUE COMMAND -->

<div class="command">

  <div class="commandLabel">
    RESCUE RECOMMENDATION
  </div>

  <div
    id="commandText"
    class="commandText neutral">

    MONITORING BUILDING

  </div>

</div>


<!-- PRIORITY -->

<div class="priorityBox">

  <div class="priorityLabel">
    STRUCTURAL INSPECTION PRIORITY
  </div>

  <div
    id="priority"
    class="priorityValue neutral">

    NORMAL

  </div>

</div>


<!-- SYSTEM -->

<div class="system">

  <div class="systemLabel">
    SYSTEM STATE
  </div>

  <div
    id="phase"
    class="systemValue neutral">

    CONNECTING...

  </div>

</div>


<div class="grid">


<!-- FLOOR 1 -->

<div class="card">

  <div class="cardTitle">
    STRUCTURAL ZONE
  </div>

  <div class="floorName">
    FLOOR 1
  </div>

  <div
    id="basementState"
    class="floorState neutral">

    READY

  </div>

  <div class="measure">

    Tilt:

    <span
      id="basementTilt"
      class="number">

      0.00

    </span> deg

  </div>

  <div class="measure">

    Motion:

    <span
      id="basementMotion"
      class="number">

      0.000

    </span> g

  </div>

</div>


<!-- FLOOR 2 -->

<div class="card">

  <div class="cardTitle">
    STRUCTURAL ZONE
  </div>

  <div class="floorName">
    FLOOR 2
  </div>

  <div
    id="floorState"
    class="floorState neutral">

    READY

  </div>

  <div class="measure">

    Tilt:

    <span
      id="floorTilt"
      class="number">

      0.00

    </span> deg

  </div>

  <div class="measure">

    Motion:

    <span
      id="floorMotion"
      class="number">

      0.000

    </span> g

  </div>

</div>


<!-- EARTHQUAKE -->

<div class="card">

  <div class="cardTitle">
    EARTHQUAKE EVENT
  </div>

  <div
    id="eventLevel"
    class="eventLevel neutral">

    NONE

  </div>

  <div class="eventMotion">

    Maximum motion:

    <span
      id="eventMax"
      class="number">

      0.000 g

    </span>

  </div>

</div>


<!-- RECOVERY -->

<div class="card">

  <div class="cardTitle">
    RECOVERY CONFIRMATION
  </div>

  <div
    id="safeCount"
    class="counter">

    0 / 30

  </div>

  <div class="progressOuter">

    <div
      id="safeProgress"
      class="progressInner">
    </div>

  </div>

  <div
    class="measure"
    style="margin-top:12px;">

    Consecutive safe structural checks

  </div>

</div>


</div>


<div class="info">

  <b>Search &amp; Rescue Prototype</b><br><br>

  SafeTag identifies structural zones that may require
  priority inspection following a seismic event.

  It does not detect or confirm the presence of survivors.

</div>


<div class="footer">

SafeTag | Local Direct Wi-Fi | No Internet Required

</div>


</div>


<script>


// =====================================================
// COLORS
// =====================================================

function colorClass(value) {

  value =
    String(value)
    .toUpperCase();


  if (
    value.includes("DANGER") ||
    value.includes("HIGH") ||
    value.includes("STRONG") ||
    value.includes("EARTHQUAKE ACTIVE")
  ) {

    return "danger";
  }


  if (
    value.includes("WARNING") ||
    value.includes("MEDIUM") ||
    value.includes("MODERATE") ||
    value.includes("VERIFYING")
  ) {

    return "warning";
  }


  if (
    value.includes("SAFE") ||
    value.includes("LOW")
  ) {

    return "safe";
  }


  return "neutral";
}


// =====================================================
// CONNECTION
// =====================================================

function markConnectionLive() {

  const connection =
    document.getElementById(
      "connection"
    );


  connection.innerHTML =
    '<span class="dot"></span> LIVE DIRECT LINK';


  connection.className =
    "link";
}


function markConnectionLost() {

  const connection =
    document.getElementById(
      "connection"
    );


  connection.innerText =
    "LINK LOST - RETRYING";


  connection.className =
    "link offline";
}


// =====================================================
// RESCUE RECOMMENDATION
// =====================================================

function updateRecommendation(d) {

  const command =
    document.getElementById(
      "commandText"
    );


  const floor1 =
    String(
      d.basementState
    ).toUpperCase();


  const floor2 =
    String(
      d.floorState
    ).toUpperCase();


  const phase =
    String(
      d.phase
    ).toUpperCase();


  // ACTIVE EARTHQUAKE

  if (
    phase.includes(
      "EARTHQUAKE ACTIVE"
    )
  ) {

    command.innerText =
      "ACTIVE SHAKING - WAIT FOR STRUCTURAL ASSESSMENT";

    command.className =
      "commandText danger";

    return;
  }


  // VERIFYING

  if (
    phase.includes(
      "VERIFYING"
    )
  ) {

    command.innerText =
      "VERIFYING SEISMIC MOTION";

    command.className =
      "commandText warning";

    return;
  }


  // BOTH DANGER

  if (
    floor1 === "DANGER" &&
    floor2 === "DANGER"
  ) {

    command.innerText =
      "HIGH RISK - INSPECT BOTH FLOORS";

    command.className =
      "commandText danger";

    return;
  }


  // FLOOR 1 DANGER

  if (
    floor1 === "DANGER"
  ) {

    command.innerText =
      "INSPECT FLOOR 1 FIRST";

    command.className =
      "commandText danger";

    return;
  }


  // FLOOR 2 DANGER

  if (
    floor2 === "DANGER"
  ) {

    command.innerText =
      "INSPECT FLOOR 2 FIRST";

    command.className =
      "commandText danger";

    return;
  }


  // BOTH WARNING

  if (
    floor1 === "WARNING" &&
    floor2 === "WARNING"
  ) {

    command.innerText =
      "CAUTION - INSPECT BOTH FLOORS";

    command.className =
      "commandText warning";

    return;
  }


  // FLOOR 1 WARNING

  if (
    floor1 === "WARNING"
  ) {

    command.innerText =
      "CHECK FLOOR 1";

    command.className =
      "commandText warning";

    return;
  }


  // FLOOR 2 WARNING

  if (
    floor2 === "WARNING"
  ) {

    command.innerText =
      "CHECK FLOOR 2";

    command.className =
      "commandText warning";

    return;
  }


  // POST-EARTHQUAKE SAFE

  if (
    phase.includes(
      "POST-EARTHQUAKE"
    ) &&
    floor1 === "SAFE" &&
    floor2 === "SAFE"
  ) {

    command.innerText =
      "NO STRUCTURAL PRIORITY DETECTED";

    command.className =
      "commandText safe";

    return;
  }


  command.innerText =
    "MONITORING BUILDING";

  command.className =
    "commandText neutral";
}


// =====================================================
// DASHBOARD UPDATE
// =====================================================

async function updateData() {

  const controller =
    new AbortController();


  const timeoutID =
    setTimeout(
      function() {

        controller.abort();

      },
      1500
    );


  try {

    const response =
      await fetch(

        "/data?t=" +
        Date.now(),

        {
          cache: "no-store",
          signal: controller.signal
        }

      );


    if (
      !response.ok
    ) {

      throw new Error(
        "HTTP error"
      );
    }


    const d =
      await response.json();


    clearTimeout(
      timeoutID
    );


    markConnectionLive();


    // SYSTEM

    const phase =
      document.getElementById(
        "phase"
      );


    phase.innerText =
      d.phase;


    phase.className =
      "systemValue " +
      colorClass(
        d.phase
      );


    // PRIORITY

    const priority =
      document.getElementById(
        "priority"
      );


    priority.innerText =
      d.priority;


    priority.className =
      "priorityValue " +
      colorClass(
        d.priority
      );


    // FLOOR 1

    const floor1 =
      document.getElementById(
        "basementState"
      );


    floor1.innerText =
      d.basementState;


    floor1.className =
      "floorState " +
      colorClass(
        d.basementState
      );


    document
      .getElementById(
        "basementTilt"
      )
      .innerText =

      d.basementTilt
      .toFixed(2);


    document
      .getElementById(
        "basementMotion"
      )
      .innerText =

      d.basementMotion
      .toFixed(3);


    // FLOOR 2

    const floor2 =
      document.getElementById(
        "floorState"
      );


    floor2.innerText =
      d.floorState;


    floor2.className =
      "floorState " +
      colorClass(
        d.floorState
      );


    document
      .getElementById(
        "floorTilt"
      )
      .innerText =

      d.floorTilt
      .toFixed(2);


    document
      .getElementById(
        "floorMotion"
      )
      .innerText =

      d.floorMotion
      .toFixed(3);


    // EARTHQUAKE EVENT

    const event =
      document.getElementById(
        "eventLevel"
      );


    event.innerText =
      d.eventLevel;


    event.className =
      "eventLevel " +
      colorClass(
        d.eventLevel
      );


    document
      .getElementById(
        "eventMax"
      )
      .innerText =

      d.eventMax
      .toFixed(3) +
      " g";


    // SAFE COUNTER

    document
      .getElementById(
        "safeCount"
      )
      .innerText =

      d.safeCount +
      " / " +
      d.safeTarget;


    let percent =

      (
        d.safeCount /
        d.safeTarget
      ) *
      100;


    if (
      percent > 100
    ) {

      percent = 100;
    }


    document
      .getElementById(
        "safeProgress"
      )
      .style.width =

      percent +
      "%";


    updateRecommendation(d);

  }

  catch (error) {

    clearTimeout(
      timeoutID
    );


    markConnectionLost();

  }

  finally {

    // Sequential polling
    // Roughly 2 updates per second
    setTimeout(
      updateData,
      500
    );
  }
}


updateData();

</script>

</body>
</html>
)HTML";


// =====================================================
// RESPONSIVE DELAY
// =====================================================

void responsiveDelay(
  unsigned long duration
) {

  unsigned long start =
    millis();


  while (
    millis() - start <
    duration
  ) {

    if (
      webServerReady
    ) {

      server.handleClient();
    }


    delay(5);
  }
}


// =====================================================
// WRITE REGISTER
// =====================================================

void writeRegister(
  SPIClass &bus,
  byte csPin,
  byte reg,
  byte value
) {

  bus.beginTransaction(
    adxlSPI
  );


  digitalWrite(
    csPin,
    LOW
  );


  delayMicroseconds(10);


  bus.transfer(reg);
  bus.transfer(value);


  delayMicroseconds(10);


  digitalWrite(
    csPin,
    HIGH
  );


  bus.endTransaction();
}


// =====================================================
// READ REGISTER
// =====================================================

byte readRegister(
  SPIClass &bus,
  byte csPin,
  byte reg
) {

  bus.beginTransaction(
    adxlSPI
  );


  digitalWrite(
    csPin,
    LOW
  );


  delayMicroseconds(10);


  bus.transfer(
    0x80 |
    reg
  );


  byte value =
    bus.transfer(
      0x00
    );


  delayMicroseconds(10);


  digitalWrite(
    csPin,
    HIGH
  );


  bus.endTransaction();


  return value;
}


// =====================================================
// READ RAW XYZ
// =====================================================

void readXYZ(
  SPIClass &bus,
  byte csPin,
  int16_t &x,
  int16_t &y,
  int16_t &z
) {

  bus.beginTransaction(
    adxlSPI
  );


  digitalWrite(
    csPin,
    LOW
  );


  delayMicroseconds(10);


  bus.transfer(
    0x80 |
    0x40 |
    DATAX0
  );


  byte x0 =
    bus.transfer(0x00);

  byte x1 =
    bus.transfer(0x00);

  byte y0 =
    bus.transfer(0x00);

  byte y1 =
    bus.transfer(0x00);

  byte z0 =
    bus.transfer(0x00);

  byte z1 =
    bus.transfer(0x00);


  delayMicroseconds(10);


  digitalWrite(
    csPin,
    HIGH
  );


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


// =====================================================
// READ g
// =====================================================

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


  gx =
    x * 0.0039;

  gy =
    y * 0.0039;

  gz =
    z * 0.0039;
}


// =====================================================
// AVERAGE SENSOR
// =====================================================

void readAverageG(
  SPIClass &bus,
  byte csPin,
  float &gx,
  float &gy,
  float &gz,
  int samples
) {

  gx = 0;
  gy = 0;
  gz = 0;


  for (
    int i = 0;
    i < samples;
    i++
  ) {

    float x;
    float y;
    float z;


    readG(
      bus,
      csPin,
      x,
      y,
      z
    );


    gx += x;
    gy += y;
    gz += z;


    if (
      webServerReady &&
      i % 4 == 0
    ) {

      server.handleClient();
    }


    delay(2);
  }


  gx /= samples;
  gy /= samples;
  gz /= samples;
}


// =====================================================
// MOTION FROM BASELINE
// =====================================================

float motionFromBaseline(
  float x,
  float y,
  float z,
  float bx,
  float by,
  float bz
) {

  float dx =
    x - bx;

  float dy =
    y - by;

  float dz =
    z - bz;


  return sqrt(

    dx * dx +

    dy * dy +

    dz * dz

  );
}


// =====================================================
// VECTOR ANGLE
// =====================================================

float vectorAngle(
  float x0,
  float y0,
  float z0,
  float x1,
  float y1,
  float z1
) {

  float dot =

    x0 * x1 +

    y0 * y1 +

    z0 * z1;


  float mag0 =

    sqrt(

      x0 * x0 +

      y0 * y0 +

      z0 * z0

    );


  float mag1 =

    sqrt(

      x1 * x1 +

      y1 * y1 +

      z1 * z1

    );


  if (
    mag0 == 0 ||
    mag1 == 0
  ) {

    return 0;
  }


  float ratio =

    dot /

    (
      mag0 *
      mag1
    );


  if (
    ratio > 1.0
  ) {

    ratio = 1.0;
  }


  if (
    ratio < -1.0
  ) {

    ratio = -1.0;
  }


  return

    acos(ratio) *

    180.0 /

    PI;
}


// =====================================================
// ADXL345 SETUP
// =====================================================

void setupADXL345(
  SPIClass &bus,
  byte csPin
) {

  writeRegister(
    bus,
    csPin,
    DATA_FORMAT,
    0x08
  );


  writeRegister(
    bus,
    csPin,
    BW_RATE,
    0x0A
  );


  writeRegister(
    bus,
    csPin,
    POWER_CTL,
    0x08
  );


  delay(100);
}


// =====================================================
// EARTHQUAKE LEVEL
// =====================================================

const char* earthquakeLevel(
  float motion
) {

  if (
    motion >=
    SHAKE_STRONG
  ) {

    return "STRONG";
  }


  if (
    motion >=
    SHAKE_MODERATE
  ) {

    return "MODERATE";
  }


  return "SMALL";
}


// =====================================================
// STRUCTURAL STATE
// =====================================================

int desiredStructuralState(
  float tilt,
  int currentState
) {

  // SAFE
  if (
    currentState == 0
  ) {

    if (
      tilt >=
      TILT_DANGER
    ) {

      return 2;
    }


    if (
      tilt >=
      TILT_WARNING
    ) {

      return 1;
    }


    return 0;
  }


  // WARNING
  if (
    currentState == 1
  ) {

    if (
      tilt >=
      TILT_DANGER
    ) {

      return 2;
    }


    if (
      tilt <
      WARNING_TO_SAFE
    ) {

      return 0;
    }


    return 1;
  }


  // DANGER
  if (
    currentState == 2
  ) {

    if (
      tilt <
      WARNING_TO_SAFE
    ) {

      return 0;
    }


    if (
      tilt <
      DANGER_TO_WARNING
    ) {

      return 1;
    }


    return 2;
  }


  return 0;
}


// =====================================================
// CONFIRM STRUCTURAL STATE
// =====================================================

void updateConfirmedState(
  int desiredState,
  int &candidateState,
  int &candidateCount,
  int &confirmedState
) {

  if (
    desiredState ==
    confirmedState
  ) {

    candidateState =
      desiredState;


    candidateCount =
      0;


    return;
  }


  if (
    desiredState ==
    candidateState
  ) {

    candidateCount++;

  }

  else {

    candidateState =
      desiredState;


    candidateCount =
      1;
  }


  if (
    candidateCount >=
    STATE_CONFIRM_COUNT
  ) {

    confirmedState =
      candidateState;


    candidateCount =
      0;
  }
}


// =====================================================
// STATE TEXT
// =====================================================

const char* stateText(
  int state
) {

  if (
    state == 2
  ) {

    return "DANGER";
  }


  if (
    state == 1
  ) {

    return "WARNING";
  }


  return "SAFE";
}


// =====================================================
// SYSTEM PHASE
// =====================================================

String currentPhaseText() {

  if (
    !baselineReady
  ) {

    return "CALIBRATING";
  }


  if (
    earthquakeActive
  ) {

    return "EARTHQUAKE ACTIVE";
  }


  if (
    postEarthquakeMode
  ) {

    return "POST-EARTHQUAKE";
  }


  if (
    shakeStartTimer != 0
  ) {

    return "VERIFYING MOTION";
  }


  return "STABLE / NORMAL";
}


// =====================================================
// PRIORITY
// =====================================================

String currentPriorityText() {

  if (
    postEarthquakeMode
  ) {

    if (
      basementState == 2 ||
      floorState == 2
    ) {

      return "HIGH";
    }


    if (
      basementState == 1 ||
      floorState == 1
    ) {

      return "MEDIUM";
    }


    return "LOW";
  }


  if (
    earthquakeActive
  ) {

    if (
      eventMaxMotion >=
      SHAKE_STRONG
    ) {

      return "HIGH";
    }


    if (
      eventMaxMotion >=
      SHAKE_MODERATE
    ) {

      return "MEDIUM";
    }


    return "LOW";
  }


  return "NORMAL";
}


// =====================================================
// WEB STATES
// =====================================================

String webBasementState() {

  if (
    postEarthquakeMode
  ) {

    return String(
      stateText(
        basementState
      )
    );
  }


  if (
    earthquakeActive
  ) {

    return "SHAKING";
  }


  if (
    !baselineReady
  ) {

    return "CALIBRATING";
  }


  return "READY";
}


String webFloorState() {

  if (
    postEarthquakeMode
  ) {

    return String(
      stateText(
        floorState
      )
    );
  }


  if (
    earthquakeActive
  ) {

    return "SHAKING";
  }


  if (
    !baselineReady
  ) {

    return "CALIBRATING";
  }


  return "READY";
}


// =====================================================
// DASHBOARD ROOT
// =====================================================

void handleDashboard() {

  server.send_P(

    200,

    "text/html; charset=utf-8",

    DASHBOARD_HTML

  );
}


// =====================================================
// LIVE JSON
// =====================================================

void handleDashboardData() {

  float displayedEventMax =
    0;


  if (
    earthquakeActive
  ) {

    displayedEventMax =
      eventMaxMotion;

  }

  else {

    displayedEventMax =
      lastEventMaxMotion;
  }


  String eventLevel =
    "NONE";


  if (
    displayedEventMax >=
    SHAKE_START
  ) {

    eventLevel =
      earthquakeLevel(
        displayedEventMax
      );
  }


  String json;

  json.reserve(
    400
  );


  json =
    "{";


  json +=
    "\"phase\":\"";

  json +=
    currentPhaseText();

  json +=
    "\",";


  json +=
    "\"eventLevel\":\"";

  json +=
    eventLevel;

  json +=
    "\",";


  json +=
    "\"eventMax\":";

  json +=
    String(
      displayedEventMax,
      3
    );

  json +=
    ",";


  json +=
    "\"priority\":\"";

  json +=
    currentPriorityText();

  json +=
    "\",";


  json +=
    "\"basementState\":\"";

  json +=
    webBasementState();

  json +=
    "\",";


  json +=
    "\"floorState\":\"";

  json +=
    webFloorState();

  json +=
    "\",";


  json +=
    "\"basementTilt\":";

  json +=
    String(
      liveBasementTilt,
      2
    );

  json +=
    ",";


  json +=
    "\"floorTilt\":";

  json +=
    String(
      liveFloorTilt,
      2
    );

  json +=
    ",";


  json +=
    "\"basementMotion\":";

  json +=
    String(
      liveBasementMotion,
      3
    );

  json +=
    ",";


  json +=
    "\"floorMotion\":";

  json +=
    String(
      liveFloorMotion,
      3
    );

  json +=
    ",";


  json +=
    "\"safeCount\":";

  json +=
    String(
      consecutiveSafeChecks
    );

  json +=
    ",";


  json +=
    "\"safeTarget\":";

  json +=
    String(
      SAFE_RESET_COUNT
    );

  json +=
    ",";


  json +=
    "\"clients\":";

  json +=
    String(
      WiFi.softAPgetStationNum()
    );


  json +=
    "}";


  server.sendHeader(
    "Cache-Control",
    "no-store, no-cache, must-revalidate, max-age=0"
  );


  server.sendHeader(
    "Pragma",
    "no-cache"
  );


  server.send(

    200,

    "application/json; charset=utf-8",

    json

  );
}


// =====================================================
// WIFI
// Known-working simple AP setup
// =====================================================

void setupWiFiDashboard() {

  Serial.println();

  Serial.println(
    "================================="
  );

  Serial.println(
    "STARTING SAFETAG WIRELESS LINK"
  );

  Serial.println(
    "================================="
  );


  WiFi.mode(
    WIFI_AP
  );


  delay(
    300
  );


  bool started =

    WiFi.softAP(

      AP_SSID,

      AP_PASSWORD

    );


  if (
    !started
  ) {

    Serial.println(
      "WIFI ACCESS POINT FAILED"
    );


    return;
  }


  delay(
    500
  );


  WiFi.setSleep(
    false
  );


  IPAddress ip =
    WiFi.softAPIP();


  server.on(

    "/",

    HTTP_GET,

    handleDashboard

  );


  server.on(

    "/data",

    HTTP_GET,

    handleDashboardData

  );


  server.onNotFound(

    []() {

      server.sendHeader(
        "Location",
        "/"
      );


      server.send(
        302,
        "text/plain",
        ""
      );

    }

  );


  server.begin();


  webServerReady =
    true;


  Serial.println(
    "WIRELESS LINK READY"
  );


  Serial.print(
    "WiFi: "
  );

  Serial.println(
    AP_SSID
  );


  Serial.print(
    "Password: "
  );

  Serial.println(
    AP_PASSWORD
  );


  Serial.print(
    "Dashboard: http://"
  );

  Serial.println(
    ip
  );


  Serial.println(
    "================================="
  );

  Serial.println();
}


// =====================================================
// BASELINE
// =====================================================

void createBaseline() {

  baselineReady =
    false;


  shakeStartTimer =
    0;


  candidateMaxMotion =
    0;


  Serial.println();

  Serial.println(
    "================================="
  );

  Serial.println(
    "CALIBRATING HEALTHY BUILDING"
  );

  Serial.println(
    "DO NOT MOVE MODEL"
  );

  Serial.println(
    "================================="
  );


  responsiveDelay(
    2000
  );


  readAverageG(

    spiA,

    CS0,

    baseX0,

    baseY0,

    baseZ0,

    60

  );


  readAverageG(

    spiB,

    CS1,

    baseX1,

    baseY1,

    baseZ1,

    60

  );


  baselineReady =
    true;


  liveBasementMotion =
    0;


  liveFloorMotion =
    0;


  liveBasementTilt =
    0;


  liveFloorTilt =
    0;


  Serial.println();

  Serial.println(
    "BASELINE COMPLETE"
  );

  Serial.println(
    "SAFETAG READY"
  );

  Serial.println();
}


// =====================================================
// RESET
// =====================================================

void resetSystem() {

  earthquakeActive =
    false;


  postEarthquakeMode =
    false;


  shakeStartTimer =
    0;


  candidateMaxMotion =
    0;


  quietStart =
    0;


  eventMaxMotion =
    0;


  lastEventMaxMotion =
    0;


  peakBasement =
    0;


  peakFloor1 =
    0;


  consecutiveSafeChecks =
    0;


  postFilterInitialized =
    false;


  filteredBasementTilt =
    0;


  filteredFloorTilt =
    0;


  basementState =
    0;


  floorState =
    0;


  basementCandidate =
    0;


  basementCandidateCount =
    0;


  floorCandidate =
    0;


  floorCandidateCount =
    0;


  Serial.println();

  Serial.println(
    "================================="
  );

  Serial.println(
    "30 CONSECUTIVE SAFE CHECKS"
  );

  Serial.println(
    "BUILDING RETURNED TO SAFE STATE"
  );

  Serial.println(
    "RESETTING SAFETAG"
  );

  Serial.println(
    "================================="
  );

  Serial.println();


  createBaseline();
}


// =====================================================
// POST-EARTHQUAKE CHECK
// =====================================================

bool printPostEarthquakeCondition() {

  float x0;
  float y0;
  float z0;

  float x1;
  float y1;
  float z1;


  // 20-sample average

  readAverageG(

    spiA,

    CS0,

    x0,

    y0,

    z0,

    20

  );


  readAverageG(

    spiB,

    CS1,

    x1,

    y1,

    z1,

    20

  );


  // ===================================================
  // TILT
  // ===================================================

  float rawBasementTilt =

    vectorAngle(

      baseX0,
      baseY0,
      baseZ0,

      x0,
      y0,
      z0

    );


  float rawFloorTilt =

    vectorAngle(

      baseX1,
      baseY1,
      baseZ1,

      x1,
      y1,
      z1

    );


  // ===================================================
  // FILTER
  // ===================================================

  if (
    !postFilterInitialized
  ) {

    filteredBasementTilt =
      rawBasementTilt;


    filteredFloorTilt =
      rawFloorTilt;


    postFilterInitialized =
      true;

  }

  else {

    filteredBasementTilt =

      FILTER_ALPHA *
      rawBasementTilt +

      (
        1.0 -
        FILTER_ALPHA
      ) *
      filteredBasementTilt;


    filteredFloorTilt =

      FILTER_ALPHA *
      rawFloorTilt +

      (
        1.0 -
        FILTER_ALPHA
      ) *
      filteredFloorTilt;
  }


  // ===================================================
  // PHONE VALUES
  // ===================================================

  liveBasementTilt =
    filteredBasementTilt;


  liveFloorTilt =
    filteredFloorTilt;


  liveBasementMotion =

    motionFromBaseline(

      x0,
      y0,
      z0,

      baseX0,
      baseY0,
      baseZ0

    );


  liveFloorMotion =

    motionFromBaseline(

      x1,
      y1,
      z1,

      baseX1,
      baseY1,
      baseZ1

    );


  // ===================================================
  // STRUCTURAL STATE
  // ===================================================

  int desiredBasement =

    desiredStructuralState(

      filteredBasementTilt,

      basementState

    );


  int desiredFloor =

    desiredStructuralState(

      filteredFloorTilt,

      floorState

    );


  updateConfirmedState(

    desiredBasement,

    basementCandidate,

    basementCandidateCount,

    basementState

  );


  updateConfirmedState(

    desiredFloor,

    floorCandidate,

    floorCandidateCount,

    floorState

  );


  bool bothSafe =

    (
      basementState == 0 &&
      floorState == 0
    );


  // ===================================================
  // SERIAL
  // ===================================================

  Serial.println();

  Serial.println(
    "================================="
  );

  Serial.println(
    "POST-EARTHQUAKE BUILDING CHECK"
  );

  Serial.println(
    "================================="
  );


  Serial.print(
    "FLOOR 1: "
  );


  Serial.print(

    stateText(
      basementState
    )

  );


  Serial.print(
    " | Tilt: "
  );


  Serial.print(
    filteredBasementTilt,
    2
  );


  Serial.println(
    " deg"
  );


  Serial.print(
    "FLOOR 2: "
  );


  Serial.print(

    stateText(
      floorState
    )

  );


  Serial.print(
    " | Tilt: "
  );


  Serial.print(
    filteredFloorTilt,
    2
  );


  Serial.println(
    " deg"
  );


  Serial.println();


  if (

    basementState == 2 ||

    floorState == 2

  ) {

    Serial.println(
      "BUILDING STATUS: DANGER"
    );

  }

  else if (

    basementState == 1 ||

    floorState == 1

  ) {

    Serial.println(
      "BUILDING STATUS: WARNING"
    );

  }

  else {

    Serial.println(
      "BUILDING STATUS: SAFE"
    );
  }


  Serial.println(
    "================================="
  );


  return bothSafe;
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    115200
  );


  delay(
    3000
  );


  pinMode(
    CS0,
    OUTPUT
  );


  pinMode(
    CS1,
    OUTPUT
  );


  digitalWrite(
    CS0,
    HIGH
  );


  digitalWrite(
    CS1,
    HIGH
  );


  // ===================================================
  // TWO SPI BUSES
  // ===================================================

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


  delay(
    1000
  );


  Serial.println();

  Serial.println(
    "==============================="
  );

  Serial.println(
    "SAFETAG"
  );

  Serial.println(
    "==============================="
  );


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


  Serial.print(
    "Floor 1 Sensor: 0x"
  );


  Serial.println(
    id0,
    HEX
  );


  Serial.print(
    "Floor 2 Sensor: 0x"
  );


  Serial.println(
    id1,
    HEX
  );


  if (

    id0 != 0xE5 ||

    id1 != 0xE5

  ) {

    Serial.println(
      "SENSOR ERROR"
    );


    while (
      true
    ) {

      delay(
        1000
      );
    }
  }


  Serial.println(
    "BOTH SENSORS PASS"
  );


  setupADXL345(

    spiA,

    CS0

  );


  setupADXL345(

    spiB,

    CS1

  );


  // ===================================================
  // WIFI
  // ===================================================

  setupWiFiDashboard();


  // ===================================================
  // BASELINE
  // ===================================================

  createBaseline();
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // Keep server responsive

  if (
    webServerReady
  ) {

    server.handleClient();
  }


  float x0;
  float y0;
  float z0;

  float x1;
  float y1;
  float z1;


  // ===================================================
  // POST-EARTHQUAKE
  // ===================================================

  if (
    postEarthquakeMode
  ) {

    if (

      millis() -
      lastPrint >=
      PRINT_INTERVAL

    ) {

      lastPrint =
        millis();


      bool bothSafe =

        printPostEarthquakeCondition();


      // =================================================
      // SAFE COUNTER
      // =================================================

      if (
        bothSafe
      ) {

        consecutiveSafeChecks++;

      }

      else {

        consecutiveSafeChecks =
          0;
      }


      Serial.print(
        "SAFE CONFIRMATION: "
      );


      Serial.print(
        consecutiveSafeChecks
      );


      Serial.print(
        "/"
      );


      Serial.println(
        SAFE_RESET_COUNT
      );


      // =================================================
      // AUTO RESET
      // =================================================

      if (

        consecutiveSafeChecks >=
        SAFE_RESET_COUNT

      ) {

        resetSystem();

        return;
      }
    }


    delay(
      5
    );


    return;
  }


  // ===================================================
  // LIVE EARTHQUAKE READINGS
  // ===================================================

  readAverageG(

    spiA,

    CS0,

    x0,

    y0,

    z0,

    2

  );


  readAverageG(

    spiB,

    CS1,

    x1,

    y1,

    z1,

    2

  );


  // ===================================================
  // MOTION
  // ===================================================

  float basementMotion =

    motionFromBaseline(

      x0,
      y0,
      z0,

      baseX0,
      baseY0,
      baseZ0

    );


  float floorMotion =

    motionFromBaseline(

      x1,
      y1,
      z1,

      baseX1,
      baseY1,
      baseZ1

    );


  float overallMotion =

    max(

      basementMotion,

      floorMotion

    );


  // ===================================================
  // PHONE VALUES
  // ===================================================

  liveBasementMotion =
    basementMotion;


  liveFloorMotion =
    floorMotion;


  // ===================================================
  // NORMAL STATE
  // ===================================================

  if (
    !earthquakeActive
  ) {

    // =================================================
    // POSSIBLE EARTHQUAKE
    // Must stay above 0.050g for 180 ms
    // =================================================

    if (

      overallMotion >=
      SHAKE_START

    ) {

      if (
        shakeStartTimer == 0
      ) {

        shakeStartTimer =
          millis();


        candidateMaxMotion =
          overallMotion;
      }


      if (

        overallMotion >
        candidateMaxMotion

      ) {

        candidateMaxMotion =
          overallMotion;
      }

    }

    else {

      // Quick bump / impact
      shakeStartTimer =
        0;


      candidateMaxMotion =
        0;
    }


    // =================================================
    // CONFIRMED EARTHQUAKE
    // =================================================

    if (

      shakeStartTimer != 0 &&

      millis() -
      shakeStartTimer >=
      SHAKE_START_TIME

    ) {

      earthquakeActive =
        true;


      eventMaxMotion =

        max(

          candidateMaxMotion,

          overallMotion

        );


      candidateMaxMotion =
        0;


      shakeStartTimer =
        0;


      peakBasement =
        basementMotion;


      peakFloor1 =
        floorMotion;


      quietStart =
        0;


      lastEventMaxMotion =
        0;


      Serial.println();

      Serial.println(
        "================================="
      );

      Serial.println(
        "EARTHQUAKE EVENT STARTED"
      );

      Serial.println(
        "================================="
      );


      Serial.print(
        "Event Level: "
      );


      Serial.println(

        earthquakeLevel(
          eventMaxMotion
        )

      );


      Serial.println();


      return;
    }


    // NORMAL OUTPUT

    if (

      millis() -
      lastPrint >=
      PRINT_INTERVAL

    ) {

      lastPrint =
        millis();


      Serial.println(
        "STATUS: STABLE / NORMAL"
      );
    }


    return;
  }


  // ===================================================
  // EARTHQUAKE ACTIVE
  // ===================================================

  if (

    overallMotion >
    eventMaxMotion

  ) {

    eventMaxMotion =
      overallMotion;
  }


  if (

    basementMotion >
    peakBasement

  ) {

    peakBasement =
      basementMotion;
  }


  if (

    floorMotion >
    peakFloor1

  ) {

    peakFloor1 =
      floorMotion;
  }


  // ===================================================
  // STILL SHAKING
  // ===================================================

  if (

    overallMotion >=
    SHAKE_END

  ) {

    quietStart =
      0;


    if (

      millis() -
      lastPrint >=
      PRINT_INTERVAL

    ) {

      lastPrint =
        millis();


      Serial.println();

      Serial.println(
        "------ SHAKING ------"
      );


      Serial.print(
        "Current Level: "
      );


      Serial.println(

        earthquakeLevel(
          overallMotion
        )

      );


      Serial.print(
        "Event Level: "
      );


      Serial.println(

        earthquakeLevel(
          eventMaxMotion
        )

      );


      Serial.print(
        "Floor 1: "
      );


      Serial.print(
        basementMotion,
        3
      );


      Serial.println(
        " g"
      );


      Serial.print(
        "Floor 2: "
      );


      Serial.print(
        floorMotion,
        3
      );


      Serial.println(
        " g"
      );


      Serial.println(
        "---------------------"
      );
    }


    return;
  }


  // ===================================================
  // SHAKING STOPPED
  // ===================================================

  if (
    quietStart == 0
  ) {

    quietStart =
      millis();


    Serial.println();

    Serial.println(
      "Shaking stopped."
    );


    Serial.println(
      "Confirming structure is stable..."
    );


    return;
  }


  // ===================================================
  // WAIT 800 ms
  // ===================================================

  if (

    millis() -
    quietStart <
    SETTLE_TIME

  ) {

    return;
  }


  // ===================================================
  // EVENT ENDED
  // ===================================================

  Serial.println();

  Serial.println(
    "================================="
  );

  Serial.println(
    "EARTHQUAKE EVENT ENDED"
  );

  Serial.println(
    "================================="
  );


  Serial.print(
    "Event Classification: "
  );


  Serial.println(

    earthquakeLevel(
      eventMaxMotion
    )

  );


  Serial.print(
    "Maximum Event Motion: "
  );


  Serial.print(
    eventMaxMotion,
    3
  );


  Serial.println(
    " g"
  );


  lastEventMaxMotion =
    eventMaxMotion;


  Serial.println();

  Serial.println(
    "Entering post-earthquake structural monitoring..."
  );


  // ===================================================
  // ENTER POST-EARTHQUAKE
  // ===================================================

  earthquakeActive =
    false;


  postEarthquakeMode =
    true;


  shakeStartTimer =
    0;


  candidateMaxMotion =
    0;


  quietStart =
    0;


  eventMaxMotion =
    0;


  peakBasement =
    0;


  peakFloor1 =
    0;


  consecutiveSafeChecks =
    0;


  postFilterInitialized =
    false;


  filteredBasementTilt =
    0;


  filteredFloorTilt =
    0;


  liveBasementTilt =
    0;


  liveFloorTilt =
    0;


  basementState =
    0;


  floorState =
    0;


  basementCandidate =
    0;


  basementCandidateCount =
    0;


  floorCandidate =
    0;


  floorCandidateCount =
    0;


  Serial.println();

  Serial.println(
    "LIVE POST-EARTHQUAKE MONITOR ACTIVE"
  );


  Serial.println(
    "Watching Floor 1 and Floor 2 continuously."
  );


  Serial.println(
    "30 consecutive SAFE checks will automatically re-arm SafeTag."
  );


  Serial.println();


  delay(
    100
  );
}
