#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <LedControl.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// Hardware SPI pins
#define CLK_PIN 15
#define DATA_PIN 4
#define CS_PIN 2

// Parola setup
#define MAX_DEVICES_PAROLA 4  // Number of modules for Parola
MD_Parola parola = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES_PAROLA);

// LedControl setup
#define MAX_DEVICES_LEDCONTROL 4  // Number of modules for LedControl
LedControl ledMatrix = LedControl(DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES_LEDCONTROL);

#define MAX_TOTAL_DEVICES 4
byte display[8][8 * MAX_TOTAL_DEVICES];  // Array for ON/OFF display state

// // Define the hardware configuration
// #define MAX_TOTAL_DEVICES 4  // Number of cascaded MAX7219 devices

#define BUZZER_PIN 23  //default 23

#define PLAYER1_PIN 34  //default 34
#define PLAYER2_PIN 35  //default 35

#define BUTTON1_PIN 12
#define BUTTON2_PIN 14

#define GAME_POINT 11  // Winning point
bool deuce = false;    // Flag for deuce condition

bool isPlayingSmashSound = false;  // Is the sound playing
bool smashTriggered = false;
int smashSoundFreq = 2000;        // Start frequency
unsigned long lastSmashTime = 0;  // Time of last sound step
const int smashStepDelay = 50;    // Time between steps

int scorePlayer1 = 0;
int scorePlayer2 = 0;

// Game variables
struct Vector2 {
  int x;
  int y;
};

Vector2 pongPosition;
Vector2 pongVelocity;

Vector2 player1Position;
Vector2 player2Position;

int pot1Val = 0;  // 0 Min | 4095 Max
int pot2Val = 0;

// Numeration
// 5x3 Numeric Digit Patterns for 0-9
const byte digitPatterns[10][3] = {
  { B11111, B10001, B11111 },  // 0
  { B01000, 
    B11111, 
    B00000 },  // 1
  { B10111, B10101, B11101 },  // 2
  { B10101, B10101, B11111 },  // 3
  { B11100, B00100, B11111 },  // 4
  { B11101, B10101, B10111 },  // 5
  { B11111, B10101, B10111 },  // 6
  { B10000, 
    B10000, 
    B11111 },  // 7
  { B11111, B10101, B11111 },  // 8
  { B11101, B10101, B11111 }   // 9
};
const byte digitPatternsFlipped[10][3] = {
  { B11111, B10001, B11111 },  // 0 flipped
  { B11111, B00010, B00000 },  // 1 flipped
  { B10111, B10101, B11101 },  // 2 flipped
  { B11111, B10101, B10101 },  // 3 flipped
  { B11111, B00100, B00111 },  // 4 flipped
  { B11101, B10101, B10111 },  // 5 flipped
  { B11101, B10101, B11111 },  // 6 flipped
  { B11111, B00001, B00001 },  // 7 flipped
  { B11111, B10101, B11111 },  // 8 flipped
  { B11111, B10101, B10111 }   // 9 flipped
};


char sliceText[50] = "Welcome";

void setup() {
  Serial.begin(115200);
  Serial.println("Game Starting...");

  pinMode(CS_PIN, OUTPUT);     // Set CS_PIN as output
  digitalWrite(CS_PIN, HIGH);  // Initially disable all devices

  activateParola();
  parola.begin();
  parola.setIntensity(5);              // Adjust brightness (0-15)
  parola.setTextAlignment(PA_CENTER);  // Center align text
  // parola.transform(MD_MAX72XX::TFR_ROT_180);
  deactivateParola();

  activateLedControl();
  // Initialize LedControl
  for (int i = 0; i < MAX_DEVICES_LEDCONTROL; i++) {
    ledMatrix.shutdown(i, false);   // Wake up MAX7219
    ledMatrix.setIntensity(i, 10);  // Brightness level (0-15)
    ledMatrix.clearDisplay(i);      // Clear LEDs
  }
  deactivateLedControl();

  // Game initialization
  pongPosition = { 8 * MAX_TOTAL_DEVICES / 2 - 1, 8 / 2 - 1 };
  pongVelocity = { 1, 1 };

  player1Position = { 7, 8 / 2 - 1 };
  player2Position = { 8 * MAX_TOTAL_DEVICES - 8, 8 / 2 - 1 };

  pinMode(BUTTON1_PIN, INPUT_PULLUP);  // Configure Button 1 with internal pull-up resistor
  pinMode(BUTTON2_PIN, INPUT_PULLUP);  // Configure Button 2 with internal pull-up resistor

  attachInterrupt(PLAYER1_PIN, MovePlayer1, CHANGE);
  attachInterrupt(PLAYER2_PIN, MovePlayer2, CHANGE);

  StartCountdown();
}

void loop() {
  if (scorePlayer1 >= GAME_POINT && scorePlayer2 >= GAME_POINT && abs(scorePlayer1 - scorePlayer2) < 2) {
    if (!deuce) {
        deuce = true;  // Enter Deuce condition
        DisplayDeuce();  // Announce Deuce only once
    }
  }

  // Check if Player 1 wins
  if (scorePlayer1 >= GAME_POINT && scorePlayer1 >= scorePlayer2 + 2) {
      deuce = false;  // Exit Deuce condition
      
      // Activate Parola
      activateParola();

      // Set text with scrolling animation
      parola.displayText("WIN >>>", PA_RIGHT, 50, 500, PA_MESH, PA_NO_EFFECT);

      // Animate text until the animation is complete
      while (!parola.displayAnimate()) {
        // Continue animating
      }
      PlayFinalVictory();
      // Deactivate Parola
      deactivateParola();
    
      AnnounceWinner("Right Win");
      return;  // End loop to reset the game
  }

  // Check if Player 2 wins
  if (scorePlayer2 >= GAME_POINT && scorePlayer2 >= scorePlayer1 + 2) {
      deuce = false;  // Exit Deuce condition
      
      // Activate Parola
      activateParola();

      // Set text with scrolling animation
      parola.displayText("WIN <<<", PA_LEFT, 50, 500, PA_MESH, PA_NO_EFFECT);

      // Animate text until the animation is complete
      while (!parola.displayAnimate()) {
        // Continue animating
      }
      PlayFinalVictory();
      // Deactivate Parola
      deactivateParola();

      AnnounceWinner("Left Win");
      return;  // End loop to reset the game
  }


  // Check if Player 1 wins
  if (scorePlayer1 >= GAME_POINT && scorePlayer1 >= scorePlayer2 + 2) {
    // Activate Parola
    activateParola();

    // Set text with scrolling animation
    parola.displayText("WIN >>>", PA_RIGHT, 50, 500, PA_MESH, PA_NO_EFFECT);

    // Animate text until the animation is complete
    while (!parola.displayAnimate()) {
      // Continue animating
    }
    PlayFinalVictory();
    // Deactivate Parola
    deactivateParola();
    AnnounceWinner("  Right Win");
  }

  // Check if Player 2 wins
  else if (scorePlayer2 >= GAME_POINT && scorePlayer2 >= scorePlayer1 + 2) {
    activateParola();

    // Set text with scrolling animation
    parola.displayText("<<< WIN", PA_LEFT, 50, 500, PA_MESH, PA_NO_EFFECT);

    // Animate text until the animation is complete
    while (!parola.displayAnimate()) {
      // Continue animating
    }
    PlayFinalVictory();
    // Deactivate Parola
    deactivateParola();
    AnnounceWinner("  Left Win");
  }

  MovePlayer1();
  MovePlayer2();
  UpdatePlayerPosition();
  UpdatePongPosition();
  DisplayScores();
  DisplayMatrix();

  delay(40);

  if (digitalRead(BUTTON1_PIN) == LOW) {
    Serial.println("Button 1 Pressed");
  }

  if (digitalRead(BUTTON2_PIN) == LOW) {
    Serial.println("Button 2 Pressed");
  }
}

// Display the current frame on the LED matrix
void DisplayMatrix() {
  activateLedControl();  // Activate LedControl

  for (int device = 0; device < MAX_TOTAL_DEVICES; device++) {
    for (int row = 0; row < 8; row++) {
      byte rowData = 0;  // Represents the 8 pixels in a row

      for (int col = 0; col < 8; col++) {
        // FC16_HW Mapping: Column mirroring for proper orientation
        int globalCol = device * 8 + col;
        if (display[row][globalCol] == 1) {
          rowData |= (1 << col);  // Column mirroring (bit order matches FC16_HW)
        }
      }

      // Update the row for the correct device
      ledMatrix.setRow(device, row, rowData);
    }
  }

  deactivateLedControl();  // Deactivate LedControl
}


void UpdatePongPosition() {
  // Reset the current pong position on the display
  Vector2 prevPosition = pongPosition;

  display[pongPosition.y][pongPosition.x] = 0;

  // Check collision with Player 1 paddle
  if (pongPosition.x == player1Position.x + 1 && (pongPosition.y == player1Position.y || pongPosition.y == player1Position.y - 1 || pongPosition.y == player1Position.y + 1)) {
    pongVelocity.x = 1;  // Reverse X direction
    PlayBeep();

    if (digitalRead(BUTTON1_PIN) == LOW) {
      if (!smashTriggered) {  // Trigger only once per smash
        Serial.println("Smash 1 activated!");
        pongVelocity.x = 3;     // Increase speed in the X direction
        PlaySmashSound();       // Play smash sound
        smashTriggered = true;  // Prevent re-trigger
      }
    } else {
      smashTriggered = false;  // Reset when button is released
    }

    // Randomize vertical direction
    if (pongPosition.y == player1Position.y) {
      pongVelocity.y = random(-1, 2);
    }
  }

  // Check collision with Player 2 paddle
  if (pongPosition.x == player2Position.x - 1 && (pongPosition.y == player2Position.y || pongPosition.y == player2Position.y - 1 || pongPosition.y == player2Position.y + 1)) {
    pongVelocity.x = -1;  // Reverse X direction
    PlayBeep();

    // Smash boost if BUTTON2_PIN is pressed
    if (digitalRead(BUTTON2_PIN) == LOW) {
      if (!smashTriggered) {  // Trigger only once per smash
        Serial.println("Smash 2 activated!");
        pongVelocity.x = -3;    // Increase speed in the X direction
        PlaySmashSound();       // Play smash sound
        smashTriggered = true;  // Prevent re-trigger
      }
    } else {
      smashTriggered = false;  // Reset when button is released
    }

    // Randomize vertical direction
    if (pongPosition.y == player2Position.y) {
      pongVelocity.y = random(-1, 2);
    }
  }

  // Check if ball goes past Player 1
  if (pongPosition.x <= 0) {
    scorePlayer2++;
    Serial.print("Player 2 (LEFT) Scored! Current Scores - Player 1(RIGHT): ");
    Serial.print(scorePlayer1);
    Serial.print(", Player 2 (LEFT): ");
    Serial.println(scorePlayer2);
    PlayVictorySound(2);
    ResetBall();
    return;
  }

  // Check if ball goes past Player 2
  if (pongPosition.x >= 8 * MAX_TOTAL_DEVICES - 1) {
    scorePlayer1++;
    Serial.print("Player 1(RIGHT) Scored! Current Scores - Player 1(RIGHT): ");
    Serial.print(scorePlayer1);
    Serial.print(", Player 2 (LEFT): ");
    Serial.println(scorePlayer2);
    PlayVictorySound(1);
    ResetBall();
    return;
  }

  // Update X boundaries
  if (pongPosition.x >= 8 * MAX_TOTAL_DEVICES - 1) {
    pongVelocity.x = -1;
    PlayBeep();
  } else if (pongPosition.x <= 0) {
    pongVelocity.x = 1;
    PlayBeep();
  }

  // Update Y boundaries
  if (pongPosition.y >= 8 - 1) {
    pongVelocity.y = -1;
    PlayBeep();
  } else if (pongPosition.y <= 0) {
    pongVelocity.y = 1;
    PlayBeep();
  }

  // Update position
  pongPosition.x += pongVelocity.x;
  pongPosition.y += pongVelocity.y;

  // Ensure Y velocity isn't zero after randomization
  if (pongVelocity.y == 0) {
    pongVelocity.y = random(-1, 2);
  }

  // Set new pong position on the display
  display[pongPosition.y][pongPosition.x] = 1;
}



void UpdatePlayerPosition() {
  // Update Player 1 paddle
  display[player1Position.y - 1][player1Position.x] = 1;
  display[player1Position.y][player1Position.x] = 1;
  display[player1Position.y + 1][player1Position.x] = 1;

  // Update Player 2 paddle
  display[player2Position.y - 1][player2Position.x] = 1;
  display[player2Position.y][player2Position.x] = 1;
  display[player2Position.y + 1][player2Position.x] = 1;
}

void MovePlayer1() {
  // Clear Player 1 paddle from display
  display[player1Position.y - 1][player1Position.x] = 0;
  display[player1Position.y][player1Position.x] = 0;
  display[player1Position.y + 1][player1Position.x] = 0;

  // Read paddle position from potentiometer
  pot1Val = analogRead(PLAYER1_PIN);
  player1Position.y = (int)(pot1Val / 4095.0 * 5.0) + 1;
}

void MovePlayer2() {
  // Clear Player 2 paddle from display
  display[player2Position.y - 1][player2Position.x] = 0;
  display[player2Position.y][player2Position.x] = 0;
  display[player2Position.y + 1][player2Position.x] = 0;

  // Read paddle position from potentiometer
  pot2Val = analogRead(PLAYER2_PIN);
  player2Position.y = (int)(pot2Val / 4095.0 * 5.0) + 1;
}

void ResetBall() {
  Serial.println("Resetting Ball...");
  pongPosition = { 8 * MAX_TOTAL_DEVICES / 2 - 1, 8 / 2 - 1 };  // Center the ball
  pongVelocity = { random(0, 2) ? 1 : -1, random(-1, 2) };      // Randomize direction
}

void DisplayScores() {
  // Clear the score areas
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 5; col++) {
      display[row][col] = 0;                              // Clear Player 1 score area (left)
      display[row][8 * MAX_TOTAL_DEVICES - 5 + col] = 0;  // Clear Player 2 score area (right)
    }
  }

  // Display Player 1 score (horizontal on left)
  DisplayDigitFlipped(scorePlayer1 / 10, 4, 0);  // Tens place
  DisplayDigitFlipped(scorePlayer1 % 10, 0, 0);  // Ones place
  
  // Display Player 2 score (horizontal on right)
  DisplayDigit(scorePlayer2 / 10, 0, 8 * MAX_TOTAL_DEVICES - 5);  // Tens place
  DisplayDigit(scorePlayer2 % 10, 4, 8 * MAX_TOTAL_DEVICES - 5);  // Ones place
}



void DisplayDigit(int digit, int startRow, int startCol) {
  if (digit < 0 || digit > 9) return;  // Ensure the digit is between 0-9

  for (int row = 0; row < 3; row++) {    // 3 rows for digit (rotated)
    for (int col = 0; col < 5; col++) {  // 5 columns for digit
      int globalRow = startRow + row;    // Adjusted for horizontal position
      int globalCol = startCol + col;    // Adjust column offset

      // Check if the pixel is ON in the rotated pattern
      if (digitPatterns[digit][row] & (1 << (4 - col))) {
        display[globalRow][globalCol] = 1;  // Set pixel ON
      } else {
        display[globalRow][globalCol] = 0;  // Set pixel OFF
      }
    }
  }
}

void DisplayDigitFlipped(int digit, int startRow, int startCol) {
  if (digit < 0 || digit > 9) return;  // Ensure the digit is between 0-9

  for (int row = 0; row < 3; row++) {    // 3 rows for digit (rotated)
    for (int col = 0; col < 5; col++) {  // 5 columns for digit
      int globalRow = startRow + row;    // Adjusted for horizontal position
      int globalCol = startCol + col;    // Adjust column offset

      // Check if the pixel is ON in the rotated pattern
      if (digitPatternsFlipped[digit][row] & (1 << (4 - col))) {
        display[globalRow][globalCol] = 1;  // Set pixel ON
      } else {
        display[globalRow][globalCol] = 0;  // Set pixel OFF
      }
    }
  }
}

void StartCountdown() {
  // Display "PONG!" with a melody
  parola.displayText("PONG!", PA_CENTER, 40, 50, PA_GROW_UP, PA_NO_EFFECT);
  while (!parola.displayAnimate()) {
  };  // Animate the text
  PlayWelcomeMelody();


  parola.displayText("Sidiq Bimo P. - 5024221002", PA_CENTER, 60, 1000, PA_SCROLL_LEFT, PA_NO_EFFECT);
  while (!parola.displayAnimate())
    ;  // Animate the text

  // Countdown 3...2...1 using Parola
  for (int i = 3; i > 0; i--) {
    char countdownText[2];
    sprintf(countdownText, "%d", i);  // Convert number to string
    PlayBeep();                       // Short beep
    parola.displayText(countdownText, PA_CENTER, 25, 1000, PA_SCROLL_UP, PA_NO_EFFECT);
    while (!parola.displayAnimate())
      ;  // Animate the countdown number
  }

  // Display "GO" with a high note
  tone(BUZZER_PIN, 1000, 500);  // High pitch sound for 500ms
  parola.displayText("GO", PA_CENTER, 15, 1000, PA_GROW_UP, PA_NO_EFFECT);
  while (!parola.displayAnimate())
    ;                  // Animate "GO"
  noTone(BUZZER_PIN);  // Stop sound
  delay(500);
}

void AnnounceWinner(const char* winnerText) {
  
  
  // Activate Parola
  activateParola();

  // Set text with scrolling animation
  parola.displayText(winnerText, PA_RIGHT, 50, 200, PA_SCROLL_LEFT, PA_GROW_DOWN);

  // Animate text until the animation is complete
  while (!parola.displayAnimate()) {
    // Continue animating
  }
  
  delay(500);
  // Deactivate Parola
  deactivateParola();

  ResetGame();
}




void ResetGame() {
  ledMatrix.clearDisplay(0);
  scorePlayer1 = 0;
  scorePlayer2 = 0;
  pongPosition = { 8 * MAX_TOTAL_DEVICES / 2 - 1, 8 / 2 - 1 };
  pongVelocity = { 1, 1 };
  deuce = false;
  StartCountdown();
}


void DisplayDeuce() {
    activateParola();
    parola.displayText("DEUCE", PA_CENTER, 40, 500, PA_SCROLL_LEFT, PA_NO_EFFECT);
    while (!parola.displayAnimate()) {
        // Non-blocking animation
    }
    for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 400, 150);
    delay(200);
    noTone(BUZZER_PIN);
    }
    deactivateParola();
}

void PlayBeep() {
  tone(BUZZER_PIN, 400, 50);  // Play a beep at 1000 Hz for 50 milliseconds
  delay(50);                  // Ensure the beep duration is noticeable
  noTone(BUZZER_PIN);         // Stop the buzzer after the beep
}

void PlayVictorySound(int player) {
  if (player == 1) {             // Player 1's victory sound
    tone(BUZZER_PIN, 660, 200);  // Note 1
    delay(200);
    tone(BUZZER_PIN, 880, 200);  // Note 2
    delay(200);
    tone(BUZZER_PIN, 990, 200);  // Note 3
    delay(200);
  } else if (player == 2) {      // Player 2's victory sound
    tone(BUZZER_PIN, 440, 200);  // Note 1
    delay(200);
    tone(BUZZER_PIN, 520, 200);  // Note 2
    delay(200);
    tone(BUZZER_PIN, 660, 200);  // Note 3
    delay(200);
  }
  noTone(BUZZER_PIN);  // Stop the buzzer after the sound
}

void DisplayStaticText(const char* text) {
  // Activate Parola
  activateParola();

  // Set text with scrolling animation
  parola.displayText(text, PA_RIGHT, 50, 1000, PA_FADE, PA_GROW_DOWN);

  // Animate text until the animation is complete
  while (!parola.displayAnimate()) {
    // Continue animating
  }

  // Deactivate Parola
  deactivateParola();
}


// Function to display a single character
void DisplayChar(char letter, int startCol) {
  static const byte font[][8] = {
    { B11111, B10001, B10001, B11111, B10000, B10000, B10000, B00000 },  // P
    { B01110, B10001, B10001, B10001, B10001, B10001, B01110, B00000 },  // O
    { B11111, B10001, B10001, B10001, B10001, B10001, B11111, B00000 },  // N
    { B01110, B10001, B10001, B10001, B10001, B10001, B01110, B00000 },  // G
    { B11111, B10000, B11110, B10000, B11111, B00000, B00000, B00000 },  // S
    { B10001, B11011, B10101, B10101, B10101, B10001, B10001, B00000 },  // W
    { B01110, B10001, B10001, B10001, B10001, B10001, B01110, B00000 }   // Default: O
  };

  int index = 6;  // Default to 'O' for unsupported letters
  switch (letter) {
    case 'P': index = 0; break;
    case 'O': index = 1; break;
    case 'N': index = 2; break;
    case 'G': index = 3; break;
    case 'S': index = 4; break;
    case 'W': index = 5; break;
  }

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int globalCol = startCol + col;  // Position across all devices
      int device = globalCol / 8;      // Which device
      int localCol = globalCol % 8;    // Column on the device

      if (device >= 0 && device < MAX_TOTAL_DEVICES) {
        ledMatrix.setLed(device, row, localCol, font[index][row] & (1 << (7 - col)));
      }
    }
  }
}



void DisplayNumber(int num) {
  static const byte numbers[][8] = {
    { B01110, B10001, B10001, B10001, B10001, B10001, B01110, B00000 },  // 0
    { B00100, B01100, B00100, B00100, B00100, B00100, B01110, B00000 },  // 1
    { B11111, B00001, B00010, B00100, B01000, B10000, B11111, B00000 },  // 2
    { B01110, B10001, B00001, B00110, B00001, B10001, B01110, B00000 }   // 3
  };

  int totalWidth = 8 * MAX_TOTAL_DEVICES;
  int startCol = (totalWidth - 8) / 2;

  // Clear all displays
  for (int i = 0; i < MAX_TOTAL_DEVICES; i++) {
    ledMatrix.clearDisplay(i);
  }

  // Draw the number
  for (int row = 0; row < 8; row++) {  // Keep the original row order
    for (int col = 0; col < 8; col++) {
      int globalCol = startCol + col;
      int device = globalCol / 8;
      int localCol = globalCol % 8;

      if (device >= 0 && device < MAX_TOTAL_DEVICES) {
        ledMatrix.setLed(device, 7 - row, localCol, (numbers[num][row] >> (7 - col)) & 1);
      }
    }
  }
}





void PlayWelcomeMelody() {
  int melody[] = { 262, 294, 330, 262, 330, 392, 440 };  // Melody notes
  int noteDurations[] = { 300, 300, 300, 300, 300, 400, 600 };

  for (int i = 0; i < 7; i++) {
    tone(BUZZER_PIN, melody[i], noteDurations[i]);
    delay(noteDurations[i] + 50);
    noTone(BUZZER_PIN);
  }
}

void PlayFinalVictory() {
  int melody[] = { 932, 932, 988, 988, 1047, 1047, 880, 784, 698 };  // Ais, B, C, A, G, F
  int durations[] = { 200, 200, 200, 200, 50, 50, 50, 50, 150 };     // Durations in milliseconds

  for (int i = 0; i < 9; i++) {
    tone(BUZZER_PIN, melody[i], durations[i]);
    delay(durations[i] + 50);  // Pause between notes
  }
  noTone(BUZZER_PIN);  // Turn off buzzer after melody
}

void PlaySmashSound() {
  // Stop any ongoing sound
  noTone(BUZZER_PIN);

  // Non-blocking smash sound effect: Quick high-to-low pitch sweep
  tone(BUZZER_PIN, 2000, 50);  // High pitch for 50ms
  tone(BUZZER_PIN, 1500, 50);  // Medium pitch for 50ms

  // Stop the buzzer after the sequence
  noTone(BUZZER_PIN);
}




void displayParolaText(const char* text) {
  parola.setTextBuffer(text);  // Set the text to display
  parola.displayReset();       // Prepare the display

  while (!parola.displayAnimate()) {
    // Keep animating text until complete
  }
}

void activateParola() {
  deactivateLedControl();     // Ensure LedControl is disabled
  digitalWrite(CS_PIN, LOW);  // Enable Parola (shared CS_PIN)
}

void deactivateParola() {
  digitalWrite(CS_PIN, HIGH);  // Disable Parola
}

void activateLedControl() {
  deactivateParola();         // Ensure Parola is disabled
  digitalWrite(CS_PIN, LOW);  // Enable LedControl (shared CS_PIN)
}

void deactivateLedControl() {
  digitalWrite(CS_PIN, HIGH);  // Disable LedControl
}
