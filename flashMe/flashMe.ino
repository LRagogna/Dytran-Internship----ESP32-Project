#include <LittleFS.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// ======================================================
// PCB BUTTON PINS
// ======================================================

const int TOP_PIN = 32;   // B1
const int MID_PIN = 33;   // B2
const int BOT_PIN = 27;   // B3


// ======================================================
// PCB TFT PINS
// J5:
// 1 = +5V
// 2 = GND
// 3 = CS
// 4 = RST
// 5 = DC
// 6 = MOSI
// 7 = SCK
// 8 = LED +5V
// ======================================================

const int TFT_CS   = 26;
const int TFT_RST  = 25;
const int TFT_DC   = 23;
const int TFT_MOSI = 21;
const int TFT_SCK  = 18;


// ======================================================
// TFT
// ======================================================

Adafruit_ILI9341 tft =
  Adafruit_ILI9341(
    &SPI,
    TFT_DC,
    TFT_CS,
    TFT_RST
  );


// ======================================================
// STUDY STATE
// ======================================================

String currentStudy = "";

int currentStepIndex = 0;

bool timerRunning = false;

unsigned long timerStartMs = 0;

bool homeMenuActive = true;

int selectedStudyIndex = 0;


// ======================================================
// BUTTON DEBOUNCE
// ======================================================

bool lastTopState = HIGH;
bool lastMidState = HIGH;
bool lastBotState = HIGH;

unsigned long lastTopPress = 0;
unsigned long lastMidPress = 0;
unsigned long lastBotPress = 0;

const unsigned long DEBOUNCE_MS = 200;


// ======================================================
// TFT REFRESH
// ======================================================

unsigned long lastScreenUpdate = 0;

const unsigned long SCREEN_UPDATE_MS = 250;


// ======================================================
// HELPERS
// ======================================================

String normalizeFilename(String filename) {
  filename.trim();

  if (!filename.endsWith(".csv")) {
    filename += ".csv";
  }

  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }

  return filename;
}


String displayFilename(String filename) {
  if (filename.startsWith("/")) {
    filename =
      filename.substring(1);
  }

  if (filename.endsWith(".csv")) {
    filename =
      filename.substring(
        0,
        filename.length() - 4
      );
  }

  return filename;
}


String csvEscape(String value) {
  value.replace("\r", " ");
  value.replace("\n", " ");

  bool needsQuotes =
    value.indexOf(',') >= 0 ||
    value.indexOf('"') >= 0;

  value.replace("\"", "\"\"");

  if (needsQuotes) {
    value =
      "\"" + value + "\"";
  }

  return value;
}


// ======================================================
// HOME MENU HELPERS
// ======================================================

bool isStudyFile(String filename) {
  return
    filename.endsWith(".csv") &&
    filename != "/temp_study.csv" &&
    filename != "temp_study.csv";
}


int getStudyCount() {
  File root = LittleFS.open("/");

  if (!root) return 0;

  int count = 0;
  File file = root.openNextFile();

  while (file) {
    if (!file.isDirectory() && isStudyFile(String(file.name()))) count++;
    file.close();
    file = root.openNextFile();
  }

  root.close();
  return count;
}


String getStudyFilename(int targetIndex) {
  File root = LittleFS.open("/");

  if (!root) return "";

  int studyIndex = 0;
  File file = root.openNextFile();

  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());

      if (isStudyFile(name)) {
        if (studyIndex == targetIndex) {
          file.close();
          root.close();
          return normalizeFilename(name);
        }

        studyIndex++;
      }
    }

    file.close();
    file = root.openNextFile();
  }

  root.close();
  return "";
}


void drawHomeMenu() {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 240, 36, ILI9341_DARKCYAN);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Choose a Study");

  int studyCount = getStudyCount();

  if (studyCount <= 0) {
    selectedStudyIndex = 0;
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(18, 72);
    tft.print("No studies found");
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_WHITE);
    tft.setCursor(18, 108);
    tft.print("Create one through USB first.");
    return;
  }

  if (selectedStudyIndex >= studyCount) selectedStudyIndex = studyCount - 1;

  const int visibleRows = 7;
  int firstVisible = selectedStudyIndex - (visibleRows / 2);

  if (firstVisible < 0) firstVisible = 0;
  if (firstVisible > studyCount - visibleRows) {
    firstVisible = max(0, studyCount - visibleRows);
  }

  for (int row = 0; row < visibleRows; row++) {
    int studyIndex = firstVisible + row;
    if (studyIndex >= studyCount) break;

    int y = 48 + row * 30;
    bool selected = studyIndex == selectedStudyIndex;

    if (selected) tft.fillRect(8, y - 4, 224, 26, ILI9341_BLUE);

    String title = displayFilename(getStudyFilename(studyIndex));
    if (title.length() > 15) title = title.substring(0, 15);

    tft.setTextColor(selected ? ILI9341_WHITE : ILI9341_LIGHTGREY);
    tft.setTextSize(2);
    tft.setCursor(16, y);
    tft.print(selected ? "> " : "  ");
    tft.print(title);
  }

  tft.fillRect(0, 282, 240, 38, ILI9341_DARKCYAN);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_DARKCYAN);
  tft.setCursor(12, 298);
  tft.print("TOP: open  MID: down  BOT: up");
}


// ======================================================
// TIME HELPERS
// ======================================================

unsigned long parseTimeToSeconds(
  String timeValue
) {
  timeValue.trim();

  if (timeValue.length() == 0) {
    return 0;
  }

  int colon =
    timeValue.indexOf(':');

  if (colon < 0) {
    return 0;
  }

  unsigned long minutes =
    timeValue
      .substring(0, colon)
      .toInt();

  unsigned long seconds =
    timeValue
      .substring(colon + 1)
      .toInt();

  return
    minutes * 60 +
    seconds;
}


String formatSeconds(
  unsigned long totalSeconds
) {
  unsigned long minutes =
    totalSeconds / 60;

  unsigned long seconds =
    totalSeconds % 60;

  char buffer[16];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02lu:%02lu",
    minutes,
    seconds
  );

  return String(buffer);
}


// ======================================================
// SIMPLE CSV FIELD PARSER
//
// Handles:
// 1,"description, with comma",00:30
// ======================================================

String getCSVField(
  String line,
  int targetField
) {
  bool inQuotes = false;

  int fieldIndex = 0;
  int startIndex = 0;

  for (
    int i = 0;
    i <= line.length();
    i++
  ) {
    char c =
      i < line.length()
      ? line[i]
      : ',';

    if (c == '"') {
      if (
        inQuotes &&
        i + 1 < line.length() &&
        line[i + 1] == '"'
      ) {
        i++;
      }
      else {
        inQuotes = !inQuotes;
      }
    }

    if (
      c == ',' &&
      !inQuotes
    ) {
      if (
        fieldIndex ==
        targetField
      ) {
        String field =
          line.substring(
            startIndex,
            i
          );

        field.trim();

        if (
          field.startsWith("\"") &&
          field.endsWith("\"") &&
          field.length() >= 2
        ) {
          field =
            field.substring(
              1,
              field.length() - 1
            );

          field.replace(
            "\"\"",
            "\""
          );
        }

        return field;
      }

      fieldIndex++;

      startIndex =
        i + 1;
    }
  }

  return "";
}


// ======================================================
// STEP COUNT
// ======================================================

int getStepCount() {
  if (
    currentStudy.length() == 0
  ) {
    return 0;
  }

  File file =
    LittleFS.open(
      currentStudy,
      "r"
    );

  if (!file) {
    return 0;
  }

  int count = 0;

  bool firstLine = true;

  while (file.available()) {
    String line =
      file.readStringUntil('\n');

    line.trim();

    if (
      firstLine
    ) {
      firstLine = false;
      continue;
    }

    if (
      line.length() > 0
    ) {
      count++;
    }
  }

  file.close();

  return count;
}


// ======================================================
// GET CURRENT STEP DATA
// ======================================================

bool getCurrentStepData(
  String &stepNumber,
  String &description,
  String &storedTime
) {
  if (
    currentStudy.length() == 0
  ) {
    return false;
  }

  File file =
    LittleFS.open(
      currentStudy,
      "r"
    );

  if (!file) {
    return false;
  }

  int dataIndex = -1;

  while (file.available()) {
    String line =
      file.readStringUntil('\n');

    line.replace(
      "\r",
      ""
    );

    if (
      dataIndex == -1
    ) {
      dataIndex++;
      continue;
    }

    if (
      dataIndex ==
      currentStepIndex
    ) {
      stepNumber =
        getCSVField(
          line,
          0
        );

      description =
        getCSVField(
          line,
          1
        );

      storedTime =
        getCSVField(
          line,
          2
        );

      file.close();

      return true;
    }

    dataIndex++;
  }

  file.close();

  return false;
}


// ======================================================
// CURRENT DISPLAY TIME
//
// Existing stored time + currently running segment
// ======================================================

String getCurrentDisplayTime() {
  String stepNumber;
  String description;
  String storedTime;

  if (
    !getCurrentStepData(
      stepNumber,
      description,
      storedTime
    )
  ) {
    return "00:00";
  }

  unsigned long totalSeconds =
    parseTimeToSeconds(
      storedTime
    );

  if (timerRunning) {
    unsigned long elapsedMs =
      millis() -
      timerStartMs;

    totalSeconds +=
      elapsedMs / 1000;
  }

  return
    formatSeconds(
      totalSeconds
    );
}


// ======================================================
// TFT TEXT WRAPPING
// ======================================================

void drawWrappedText(
  String text,
  int x,
  int y,
  int maxWidth,
  int maxLines,
  uint16_t color,
  uint8_t textSize
) {
  tft.setTextColor(
    color,
    ILI9341_BLACK
  );

  tft.setTextSize(
    textSize
  );

  int charWidth =
    6 * textSize;

  int charsPerLine =
    maxWidth /
    charWidth;

  if (
    charsPerLine < 1
  ) {
    charsPerLine = 1;
  }

  String remaining =
    text;

  for (
    int line = 0;
    line < maxLines &&
    remaining.length() > 0;
    line++
  ) {
    int breakPos =
      min(
        charsPerLine,
        (int)remaining.length()
      );

    if (
      breakPos <
      remaining.length()
    ) {
      int space =
        remaining.lastIndexOf(
          ' ',
          breakPos
        );

      if (space > 0) {
        breakPos =
          space;
      }
    }

    String piece =
      remaining.substring(
        0,
        breakPos
      );

    piece.trim();

    tft.setCursor(
      x,
      y +
      line *
      (8 * textSize)
    );

    tft.print(
      piece
    );

    remaining =
      remaining.substring(
        breakPos
      );

    remaining.trim();
  }
}


// ======================================================
// TFT SCREEN
// ======================================================

void drawScreen() {
  tft.fillScreen(
    ILI9341_BLACK
  );

  // ----------------------------------
  // No study selected
  // ----------------------------------

  if (
    currentStudy.length() == 0
  ) {
    tft.setTextColor(
      ILI9341_WHITE
    );

    tft.setTextSize(2);

    tft.setCursor(
      20,
      40
    );

    tft.println(
      "No Study Selected"
    );

    tft.setTextSize(1);

    tft.setCursor(
      20,
      80
    );

    tft.println(
      "Use SELECT through USB"
    );

    return;
  }


  String stepNumber;
  String description;
  String storedTime;

  if (
    !getCurrentStepData(
      stepNumber,
      description,
      storedTime
    )
  ) {
    tft.setTextColor(
      ILI9341_WHITE
    );

    tft.setTextSize(2);

    tft.setCursor(
      20,
      40
    );

    tft.println(
      "No Steps"
    );

    return;
  }


  int stepCount =
    getStepCount();


  // ==================================================
  // HEADER
  // ==================================================

  tft.fillRect(
    0,
    0,
    240,
    34,
    ILI9341_DARKCYAN
  );

  tft.setTextColor(
    ILI9341_WHITE
  );

  tft.setTextSize(2);

  tft.setCursor(
    10,
    9
  );

  String title =
    displayFilename(
      currentStudy
    );

  if (
    title.length() > 17
  ) {
    title =
      title.substring(
        0,
        17
      );
  }

  tft.print(
    title
  );


  // ==================================================
  // STEP NUMBER
  // ==================================================

  tft.setTextColor(
    ILI9341_CYAN
  );

  tft.setTextSize(2);

  tft.setCursor(
    12,
    48
  );

  tft.print(
    "Step "
  );

  tft.print(
    currentStepIndex + 1
  );

  tft.print(
    " / "
  );

  tft.println(
    stepCount
  );


  // ==================================================
  // DESCRIPTION
  // ==================================================

  tft.drawRect(
    10,
    76,
    220,
    130,
    ILI9341_DARKGREY
  );

  drawWrappedText(
    description,
    18,
    86,
    204,
    13,
    ILI9341_WHITE,
    1
  );


  // ==================================================
  // TIME
  // ==================================================

  tft.setTextColor(
    ILI9341_WHITE
  );

  tft.setTextSize(1);

  tft.setCursor(
    12,
    222
  );

  tft.print(
    "TIME"
  );


  tft.setTextSize(4);

  tft.setCursor(
    12,
    240
  );

  if (timerRunning) {
    tft.setTextColor(
      ILI9341_GREEN
    );
  }
  else {
    tft.setTextColor(
      ILI9341_WHITE
    );
  }

  tft.print(
    getCurrentDisplayTime()
  );


  // ==================================================
  // STATUS
  // ==================================================

  tft.setTextSize(2);

  if (timerRunning) {
    tft.setTextColor(
      ILI9341_GREEN
    );

    tft.setCursor(
      170,
      220
    );

    tft.print(
      "REC"
    );
  }

  else {
    tft.setTextColor(
      ILI9341_YELLOW
    );

    tft.setCursor(
      170,
      220
    );

    tft.print(
      "STOP"
    );
  }


  // ==================================================
  // BUTTON HELP
  // ==================================================

  tft.fillRect(0, 282, 240, 38, ILI9341_DARKCYAN);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_DARKCYAN);
  tft.setCursor(8, 286);
  tft.print("TOP: Record  MID: Next");
  tft.setCursor(8, 300);
  tft.print("BOT: Previous");
}


// ======================================================
// UPDATE ONLY TIME AREA
//
// Avoids full-screen flashing while recording.
// ======================================================

void updateRunningTimeDisplay() {
  if (!timerRunning) {
    return;
  }

  if (
    millis() -
    lastScreenUpdate <
    SCREEN_UPDATE_MS
  ) {
    return;
  }

  lastScreenUpdate =
    millis();

  tft.fillRect(
    10,
    237,
    150,
    45,
    ILI9341_BLACK
  );

  tft.setTextColor(
    ILI9341_GREEN,
    ILI9341_BLACK
  );

  tft.setTextSize(4);

  tft.setCursor(
    12,
    240
  );

  tft.print(
    getCurrentDisplayTime()
  );
}


void openSelectedStudy() {
  int studyCount = getStudyCount();

  if (studyCount <= 0) {
    Serial.println("ERROR:NO_STUDIES");
    drawHomeMenu();
    return;
  }

  String filename = getStudyFilename(selectedStudyIndex);

  if (filename.length() == 0) {
    Serial.println("ERROR:FILE_NOT_FOUND");
    drawHomeMenu();
    return;
  }

  currentStudy = filename;
  currentStepIndex = 0;
  timerRunning = false;
  homeMenuActive = false;

  Serial.print("OK:SELECTED:");
  Serial.println(currentStudy);
  drawScreen();
}


// ======================================================
// ADD TIME TO CURRENT STEP
// ======================================================

bool addTimeToCurrentStep(
  unsigned long newElapsedMs
) {
  if (
    currentStudy.length() == 0
  ) {
    Serial.println(
      "ERROR:NO_STUDY_SELECTED"
    );

    return false;
  }

  File source =
    LittleFS.open(
      currentStudy,
      "r"
    );

  if (!source) {
    Serial.println(
      "ERROR:OPEN_FAILED"
    );

    return false;
  }

  String tempFile =
    "/temp_study.csv";

  File destination =
    LittleFS.open(
      tempFile,
      "w"
    );

  if (!destination) {
    source.close();

    Serial.println(
      "ERROR:TEMP_FILE_FAILED"
    );

    return false;
  }

  int dataRow = -1;

  String finalTime = "";

  while (
    source.available()
  ) {
    String line =
      source.readStringUntil(
        '\n'
      );

    line.replace(
      "\r",
      ""
    );


    // Header

    if (
      dataRow == -1
    ) {
      destination.println(
        line
      );

      dataRow++;

      continue;
    }


    // Current row

    if (
      dataRow ==
      currentStepIndex
    ) {
      String existingTime =
        getCSVField(
          line,
          2
        );

      unsigned long existingSeconds =
        parseTimeToSeconds(
          existingTime
        );


      unsigned long addedSeconds =
        (
          newElapsedMs +
          500
        ) /
        1000;


      unsigned long totalSeconds =
        existingSeconds +
        addedSeconds;


      finalTime =
        formatSeconds(
          totalSeconds
        );


      String stepNumber =
        getCSVField(
          line,
          0
        );

      String description =
        getCSVField(
          line,
          1
        );


      destination.print(
        stepNumber
      );

      destination.print(
        ","
      );

      destination.print(
        csvEscape(
          description
        )
      );

      destination.print(
        ","
      );

      destination.println(
        finalTime
      );
    }

    else {
      destination.println(
        line
      );
    }

    dataRow++;
  }

  source.close();

  destination.close();


  LittleFS.remove(
    currentStudy
  );


  if (
    !LittleFS.rename(
      tempFile,
      currentStudy
    )
  ) {
    Serial.println(
      "ERROR:RENAME_FAILED"
    );

    return false;
  }


  Serial.print(
    "TOTAL_TIME:"
  );

  Serial.println(
    finalTime
  );


  return true;
}


// ======================================================
// TOP BUTTON
// START / STOP
// ======================================================

void handleTop() {
  if (homeMenuActive) {
    openSelectedStudy();
    return;
  }

  if (
    currentStudy.length() == 0
  ) {
    Serial.println(
      "ERROR:NO_STUDY_SELECTED"
    );

    return;
  }


  if (
    getStepCount() <= 0
  ) {
    Serial.println(
      "ERROR:NO_STEPS"
    );

    return;
  }


  // START

  if (!timerRunning) {
    timerStartMs =
      millis();

    timerRunning =
      true;


    Serial.print(
      "TIMER_STARTED:STEP:"
    );

    Serial.println(
      currentStepIndex + 1
    );


    drawScreen();
  }


  // STOP

  else {
    unsigned long elapsed =
      millis() -
      timerStartMs;


    timerRunning =
      false;


    if (
      addTimeToCurrentStep(
        elapsed
      )
    ) {
      Serial.print(
        "TIMER_ADDED:STEP:"
      );

      Serial.print(
        currentStepIndex + 1
      );

      Serial.print(
        ":ADDED:"
      );

      Serial.println(
        formatSeconds(
          (
            elapsed +
            500
          ) /
          1000
        )
      );
    }


    drawScreen();
  }
}


// ======================================================
// MID BUTTON
// NEXT STEP
// ======================================================

void handleMid() {
  if (homeMenuActive) {
    int studyCount = getStudyCount();

    if (studyCount > 0) {
      selectedStudyIndex = (selectedStudyIndex + 1) % studyCount;
    }

    drawHomeMenu();
    return;
  }

  if (timerRunning) {
    Serial.println(
      "ERROR:TIMER_RUNNING"
    );

    return;
  }


  int stepCount =
    getStepCount();


  if (
    stepCount <= 0
  ) {
    Serial.println(
      "ERROR:NO_STUDY_SELECTED"
    );

    return;
  }


  if (
    currentStepIndex <
    stepCount - 1
  ) {
    currentStepIndex++;
  }


  Serial.print(
    "STEP:"
  );

  Serial.print(
    currentStepIndex + 1
  );

  Serial.print(
    "/"
  );

  Serial.println(
    stepCount
  );


  drawScreen();
}


// ======================================================
// BOT BUTTON
// PREVIOUS STEP
// ======================================================

void handleBot() {
  if (homeMenuActive) {
    int studyCount = getStudyCount();

    if (studyCount > 0) {
      selectedStudyIndex--;
      if (selectedStudyIndex < 0) selectedStudyIndex = studyCount - 1;
    }

    drawHomeMenu();
    return;
  }

  if (timerRunning) {
    Serial.println(
      "ERROR:TIMER_RUNNING"
    );

    return;
  }


  if (
    currentStudy.length() == 0
  ) {
    Serial.println(
      "ERROR:NO_STUDY_SELECTED"
    );

    return;
  }


  // Previous from step 1 returns to the study home menu.
  if (
    currentStepIndex == 0
  ) {
    homeMenuActive = true;
    selectedStudyIndex = 0;

    Serial.println(
      "OK:HOME"
    );

    drawHomeMenu();
    return;
  }


  if (
    currentStepIndex > 0
  ) {
    currentStepIndex--;
  }


  Serial.print(
    "STEP:"
  );

  Serial.print(
    currentStepIndex + 1
  );

  Serial.print(
    "/"
  );

  Serial.println(
    getStepCount()
  );


  drawScreen();
}


// ======================================================
// CREATE STUDY
// ======================================================

void createStudy(
  String filename
) {
  filename =
    normalizeFilename(
      filename
    );


  if (
    LittleFS.exists(
      filename
    )
  ) {
    Serial.println(
      "ERROR:FILE_ALREADY_EXISTS"
    );

    return;
  }


  File file =
    LittleFS.open(
      filename,
      "w"
    );


  if (!file) {
    Serial.println(
      "ERROR:CREATE_FAILED"
    );

    return;
  }


  file.println(
    "Step Number,"
    "Step Description,"
    "Time (mm:ss)"
  );


  file.close();


  Serial.println(
    "OK:STUDY_CREATED"
  );

  if (homeMenuActive) {
    drawHomeMenu();
  }
}


// ======================================================
// SELECT STUDY
// ======================================================

void selectStudy(
  String filename
) {
  filename =
    normalizeFilename(
      filename
    );


  if (
    !LittleFS.exists(
      filename
    )
  ) {
    Serial.println(
      "ERROR:FILE_NOT_FOUND"
    );

    return;
  }


  currentStudy =
    filename;

  currentStepIndex = 0;

  timerRunning = false;

  homeMenuActive = false;


  Serial.print(
    "OK:SELECTED:"
  );

  Serial.println(
    currentStudy
  );


  drawScreen();
}


// ======================================================
// CURRENT STUDY
// ======================================================

void showCurrentStudy() {
  if (
    currentStudy.length() == 0
  ) {
    Serial.println(
      "NO_STUDY_SELECTED"
    );

    return;
  }


  Serial.print(
    "CURRENT:"
  );

  Serial.println(
    currentStudy
  );


  Serial.print(
    "STEP:"
  );

  Serial.print(
    currentStepIndex + 1
  );

  Serial.print(
    "/"
  );

  Serial.println(
    getStepCount()
  );
}


// ======================================================
// ADD JT STEP
// ======================================================

void addStep(
  String command
) {
  int firstSeparator =
    command.indexOf(
      '|'
    );

  int secondSeparator =
    command.indexOf(
      '|',
      firstSeparator + 1
    );


  if (
    firstSeparator < 0 ||
    secondSeparator < 0
  ) {
    Serial.println(
      "ERROR:BAD_STEP"
    );

    return;
  }


  String filename =
    command.substring(
      0,
      firstSeparator
    );


  String stepNumber =
    command.substring(
      firstSeparator + 1,
      secondSeparator
    );


  String description =
    command.substring(
      secondSeparator + 1
    );


  filename =
    normalizeFilename(
      filename
    );


  if (
    !LittleFS.exists(
      filename
    )
  ) {
    Serial.println(
      "ERROR:FILE_NOT_FOUND"
    );

    return;
  }


  File file =
    LittleFS.open(
      filename,
      "a"
    );


  if (!file) {
    Serial.println(
      "ERROR:OPEN_FAILED"
    );

    return;
  }


  file.print(
    stepNumber
  );

  file.print(
    ","
  );

  file.print(
    csvEscape(
      description
    )
  );

  file.print(
    ","
  );

  file.println(
    ""
  );


  file.close();


  Serial.println(
    "OK:STEP_ADDED"
  );
}


// ======================================================
// LIST CSV FILES
// ======================================================

void listCSVs() {
  Serial.println(
    "LIST_START"
  );


  File root =
    LittleFS.open(
      "/"
    );


  if (!root) {
    Serial.println(
      "LIST_END"
    );

    return;
  }


  File file =
    root.openNextFile();


  while (file) {
    if (
      !file.isDirectory()
    ) {
      String name =
        file.name();


      if (
        name.endsWith(
          ".csv"
        )
      ) {
        Serial.println(
          name
        );
      }
    }


    file.close();


    file =
      root.openNextFile();
  }


  root.close();


  Serial.println(
    "LIST_END"
  );
}


// ======================================================
// SEND CSV
// ======================================================

void sendCSV(
  String filename
) {
  filename =
    normalizeFilename(
      filename
    );


  if (
    !LittleFS.exists(
      filename
    )
  ) {
    Serial.println(
      "ERROR:FILE_NOT_FOUND"
    );

    return;
  }


  File file =
    LittleFS.open(
      filename,
      "r"
    );


  if (!file) {
    Serial.println(
      "ERROR:OPEN_FAILED"
    );

    return;
  }


  Serial.println(
    "FILE_START"
  );


  while (
    file.available()
  ) {
    Serial.write(
      file.read()
    );
  }


  file.close();


  Serial.println(
    "FILE_END"
  );
}


// ======================================================
// DELETE CSV
// ======================================================

void deleteCSV(
  String filename
) {
  filename =
    normalizeFilename(
      filename
    );


  if (
    !LittleFS.exists(
      filename
    )
  ) {
    Serial.println(
      "ERROR:FILE_NOT_FOUND"
    );

    return;
  }


  if (
    currentStudy ==
    filename
  ) {
    currentStudy = "";

    currentStepIndex = 0;

    timerRunning = false;

    homeMenuActive = true;
  }


  if (
    LittleFS.remove(
      filename
    )
  ) {
    Serial.print(
      "OK:DELETED:"
    );

    Serial.println(
      filename
    );
  }

  else {
    Serial.println(
      "ERROR:DELETE_FAILED"
    );
  }


  if (homeMenuActive) {
    int studyCount = getStudyCount();
    if (selectedStudyIndex >= studyCount) selectedStudyIndex = max(0, studyCount - 1);
    drawHomeMenu();
  }
  else {
    drawScreen();
  }
}


// ======================================================
// PHYSICAL BUTTONS
// ======================================================

void checkButtons() {
  bool topState =
    digitalRead(
      TOP_PIN
    );


  bool midState =
    digitalRead(
      MID_PIN
    );


  bool botState =
    digitalRead(
      BOT_PIN
    );


  unsigned long now =
    millis();


  if (
    lastTopState == HIGH &&
    topState == LOW &&
    now -
    lastTopPress >
    DEBOUNCE_MS
  ) {
    lastTopPress =
      now;

    handleTop();
  }


  if (
    lastMidState == HIGH &&
    midState == LOW &&
    now -
    lastMidPress >
    DEBOUNCE_MS
  ) {
    lastMidPress =
      now;

    handleMid();
  }


  if (
    lastBotState == HIGH &&
    botState == LOW &&
    now -
    lastBotPress >
    DEBOUNCE_MS
  ) {
    lastBotPress =
      now;

    handleBot();
  }


  lastTopState =
    topState;

  lastMidState =
    midState;

  lastBotState =
    botState;
}


// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(
    115200
  );


  pinMode(
    TOP_PIN,
    INPUT_PULLUP
  );


  pinMode(
    MID_PIN,
    INPUT_PULLUP
  );


  pinMode(
    BOT_PIN,
    INPUT_PULLUP
  );


  // Custom SPI pins from PCB

  SPI.begin(
    TFT_SCK,
    -1,
    TFT_MOSI,
    TFT_CS
  );


  tft.begin();

  // Orientation 0: portrait, 240 x 320

  tft.setRotation(
    0
  );


  tft.fillScreen(
    ILI9341_BLACK
  );


  delay(
    300
  );


  if (
    !LittleFS.begin(
      true
    )
  ) {
    Serial.println(
      "ERROR:LITTLEFS_FAILED"
    );


    tft.setTextColor(
      ILI9341_RED
    );


    tft.setTextSize(
      2
    );


    tft.setCursor(
      20,
      30
    );


    tft.println(
      "LittleFS Error"
    );


    return;
  }


  Serial.println(
    "ESP32_STUDY_MANAGER_READY"
  );


  homeMenuActive = true;
  selectedStudyIndex = 0;
  drawHomeMenu();
}


// ======================================================
// LOOP
// ======================================================

void loop() {
  checkButtons();


  updateRunningTimeDisplay();


  if (
    !Serial.available()
  ) {
    return;
  }


  String input =
    Serial.readStringUntil(
      '\n'
    );


  input.trim();


  if (
    input.length() == 0
  ) {
    return;
  }


  // ==================================================
  // SIMULATED BUTTONS
  // ==================================================

  if (
    input == "TOP"
  ) {
    handleTop();
  }


  else if (
    input == "MID"
  ) {
    handleMid();
  }


  else if (
    input == "BOT"
  ) {
    handleBot();
  }


  // ==================================================
  // CREATE STUDY
  // ==================================================

  else if (
    input.startsWith(
      "NEWSTUDY "
    )
  ) {
    createStudy(
      input.substring(
        9
      )
    );
  }


  // ==================================================
  // SELECT STUDY
  // ==================================================

  else if (
    input.startsWith(
      "SELECT "
    )
  ) {
    selectStudy(
      input.substring(
        7
      )
    );
  }


  // ==================================================
  // CURRENT
  // ==================================================

  else if (
    input == "CURRENT"
  ) {
    showCurrentStudy();
  }


  // ==================================================
  // HOME MENU
  // ==================================================

  else if (input == "HOME") {
    if (timerRunning) {
      Serial.println("ERROR:TIMER_RUNNING");
    }
    else {
      homeMenuActive = true;
      selectedStudyIndex = 0;
      Serial.println("OK:HOME");
      drawHomeMenu();
    }
  }


  // ==================================================
  // ADD JT STEP
  // ==================================================

  else if (
    input.startsWith(
      "STEP "
    )
  ) {
    addStep(
      input.substring(
        5
      )
    );
  }


  // ==================================================
  // LIST
  // ==================================================

  else if (
    input == "LIST"
  ) {
    listCSVs();
  }


  // ==================================================
  // GET
  // ==================================================

  else if (
    input.startsWith(
      "GET "
    )
  ) {
    sendCSV(
      input.substring(
        4
      )
    );
  }


  // ==================================================
  // DELETE
  // ==================================================

  else if (
    input.startsWith(
      "DELETE "
    )
  ) {
    deleteCSV(
      input.substring(
        7
      )
    );
  }


  // ==================================================
  // PING
  // ==================================================

  else if (
    input == "PING"
  ) {
    Serial.println(
      "ESP32_STUDY_MANAGER"
    );
  }


  else {
    Serial.println(
      "ERROR:UNKNOWN_COMMAND"
    );
  }
}