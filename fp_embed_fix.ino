#include <LedControl.h>

// Define the hardware configuration
#define MAX_DEVICES 4  // Number of cascaded MAX7219 devices
#define CLK_PIN 15     // CLK or SCK pin ; default 15
#define CS_PIN 2       // CS or LOAD pin default 2
#define DATA_PIN 4     // DIN or MOSI pin default 4

#define BUZZER_PIN 23  //default 23

#define PLAYER1_PIN 34  //default 34
#define PLAYER2_PIN 35  //default 35

#define BUTTON1_PIN 12
#define BUTTON2_PIN 14

#define GAME_POINT 11  // Winning point
bool deuce = false;    // Flag for deuce condition

int scorePlayer1 = 0;
int scorePlayer2 = 0;

// Create an instance of LedControl
LedControl ledMatrix = LedControl(DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Game variables
struct Vector2 {
  int x;
  int y;
};

byte display[8][8 * MAX_DEVICES];  // ON or OFF
Vector2 pongPosition;
Vector2 pongVelocity;

Vector2 player1Position;
Vector2 player2Position;

int pot1Val = 0;  // 0 Min | 4095 Max
int pot2Val = 0;

// Numeration
// 5x3 Numeric Digit Patterns for 0-9
const byte digitPatterns[10][3] = {
  {B11111, B10001, B11111}, // 0
  {B01000, B11111, B00000}, // 1
  {B10111, B10101, B11101}, // 2
  {B10101, B10101, B11111}, // 3
  {B11100, B00100, B11111}, // 4
  {B11101, B10101, B10111}, // 5
  {B11111, B10101, B10111}, // 6
  {B10000, B10000, B11111}, // 7
  {B11111, B10101, B11111}, // 8
  {B11101, B10101, B11111}  // 9
};



char sliceText[50] = "Welcome";

void setup() {
  Serial.begin(115200);
  Serial.println("Game Starting...");

  // Initialize LED matrix
  for (int i = 0; i < MAX_DEVICES; i++) {
    ledMatrix.shutdown(i, false);   // Wake up MAX7219
    ledMatrix.setIntensity(i, 10);  // Set brightness level (0-15)
    ledMatrix.clearDisplay(i);      // Clear all LEDs
  }

  // Game initialization
  pongPosition = { 8 * MAX_DEVICES / 2 - 1, 8 / 2 - 1 };
  pongVelocity = { 1, 1 };

  player1Position = { 7, 8 / 2 - 1 };
  player2Position = { 8 * MAX_DEVICES - 8, 8 / 2 - 1 };

  pinMode(BUTTON1_PIN, INPUT_PULLUP); // Configure Button 1 with internal pull-up resistor
  pinMode(BUTTON2_PIN, INPUT_PULLUP); // Configure Button 2 with internal pull-up resistor

  attachInterrupt(PLAYER1_PIN, MovePlayer1, CHANGE);
  attachInterrupt(PLAYER2_PIN, MovePlayer2, CHANGE);

  StartCountdown();
}

void loop() {
  if (scorePlayer1 >= GAME_POINT && scorePlayer2 >= GAME_POINT && abs(scorePlayer1 - scorePlayer2) < 2) {
    deuce = true;  // Deuce condition
  }

  // Display Deuce if required
  if (deuce) {
    DisplayDeuce();
    deuce = false;
  }

  // Check if Player 1 wins
  if (scorePlayer1 >= GAME_POINT && scorePlayer1 >= scorePlayer2 + 2) {
    AnnounceWinner("Left Win");
  }

  // Check if Player 2 wins
  else if (scorePlayer2 >= GAME_POINT && scorePlayer2 >= scorePlayer1 + 2) {
    AnnounceWinner("Right Win");
  }

  MovePlayer1();
  MovePlayer2();
  UpdatePlayerPosition();
  UpdatePongPosition();
  DisplayScores();
  DisplayMatrix();
  delay(50);

  if (digitalRead(BUTTON1_PIN) == LOW) {
  Serial.println("Button 1 Pressed");
  }

  if (digitalRead(BUTTON2_PIN) == LOW) {
    Serial.println("Button 2 Pressed");
  }

}

// Display the current frame on the LED matrix
void DisplayMatrix() {
  for (int device = 0; device < MAX_DEVICES; device++) {
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
}

void UpdatePongPosition() {
  // Reset the current pong position on the display
  display[pongPosition.y][pongPosition.x] = 0;

  // Check collision with Player 1 paddle
  if (pongPosition.x == player1Position.x + 1 && 
      (pongPosition.y == player1Position.y || pongPosition.y == player1Position.y - 1 || pongPosition.y == player1Position.y + 1)) {
    pongVelocity.x = 1;  // Reverse X direction
    PlayBeep();

    // Smash boost if BUTTON1_PIN is pressed
    if (digitalRead(BUTTON1_PIN) == LOW) {
      Serial.println("Smash 1 activated!");
      pongVelocity.x = 3;  // Increase speed in the X direction
    }

    // Randomize vertical direction
    if (pongPosition.y == player1Position.y) {
      pongVelocity.y = random(-1, 2);
    }
  }

  // Check collision with Player 2 paddle
  if (pongPosition.x == player2Position.x - 1 && 
      (pongPosition.y == player2Position.y || pongPosition.y == player2Position.y - 1 || pongPosition.y == player2Position.y + 1)) {
    pongVelocity.x = -1;  // Reverse X direction
    PlayBeep();

    // Smash boost if BUTTON2_PIN is pressed
    if (digitalRead(BUTTON2_PIN) == LOW) {
      Serial.println("Smash 2 activated!");
      pongVelocity.x = -3;  // Increase speed in the X direction
    }

    // Randomize vertical direction
    if (pongPosition.y == player2Position.y) {
      pongVelocity.y = random(-1, 2);
    }
  }

  // Check if ball goes past Player 1
  if (pongPosition.x <= 0) {
    scorePlayer2++;
    Serial.print("Player 2 Scored! Current Scores - Player 1: ");
    Serial.print(scorePlayer1);
    Serial.print(", Player 2: ");
    Serial.println(scorePlayer2);
    PlayVictorySound(2);
    ResetBall();
    return;
  }

  // Check if ball goes past Player 2
  if (pongPosition.x >= 8 * MAX_DEVICES - 1) {
    scorePlayer1++;
    Serial.print("Player 1 Scored! Current Scores - Player 1: ");
    Serial.print(scorePlayer1);
    Serial.print(", Player 2: ");
    Serial.println(scorePlayer2);
    PlayVictorySound(1);
    ResetBall();
    return;
  }

  // Update X boundaries
  if (pongPosition.x >= 8 * MAX_DEVICES - 1) {
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
  pongPosition = { 8 * MAX_DEVICES / 2 - 1, 8 / 2 - 1 };    // Center the ball
  pongVelocity = { random(0, 2) ? 1 : -1, random(-1, 2) };  // Randomize direction
}

void DisplayScores() {
  // Clear the score areas
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 5; col++) {
      display[row][col] = 0; // Clear Player 1 score area (left)
      display[row][8 * MAX_DEVICES - 5 + col] = 0; // Clear Player 2 score area (right)
    }
  }

  // Display Player 1 score (horizontal on left)
  DisplayDigit(scorePlayer1 / 10, 0, 0);   // Tens place
  DisplayDigit(scorePlayer1 % 10, 4, 0);   // Ones place

  // Display Player 2 score (horizontal on right)
  DisplayDigit(scorePlayer2 / 10, 0, 8 * MAX_DEVICES - 5);  // Tens place
  DisplayDigit(scorePlayer2 % 10, 4, 8 * MAX_DEVICES - 5);  // Ones place
}



void DisplayDigit(int digit, int startRow, int startCol) {
  if (digit < 0 || digit > 9) return;  // Ensure the digit is between 0-9

  for (int row = 0; row < 3; row++) {  // 3 rows for digit (rotated)
    for (int col = 0; col < 5; col++) {  // 5 columns for digit
      int globalRow = startRow + row;  // Adjusted for horizontal position
      int globalCol = startCol + col;  // Adjust column offset

      // Check if the pixel is ON in the rotated pattern
      if (digitPatterns[digit][row] & (1 << (4 - col))) {
        display[globalRow][globalCol] = 1;  // Set pixel ON
      } else {
        display[globalRow][globalCol] = 0;  // Set pixel OFF
      }
    }
  }
}

void StartCountdown() {
  // Display "PONG!" with a melody
  DisplayStaticText("PONG");
  PlayWelcomeMelody();
  delay(1500);  // Display "PONG" for 1.5 seconds

  // Countdown 3...2...1
  for (int i = 3; i > 0; i--) {
    DisplayNumber(i);      // Display countdown number
    PlayBeep();            // Short beep
    delay(1000);           // Wait for 1 second
  }

  // Display "SWING!" with a high note
  DisplayStaticText("SWING");
  tone(BUZZER_PIN, 1000, 500);  // High pitch sound for 500ms
  delay(1500);                  // Display SWING for 1.5 seconds
  noTone(BUZZER_PIN);           // Stop sound
}

void AnnounceWinner(const char* winnerText) {
  // Clear the display and show winner text
  ledMatrix.clearDisplay(0);
  DisplayStaticText(winnerText);

  // Play victory sound based on the winner
  if (strcmp(winnerText, "Left Win") == 0) {
    PlayVictorySound(1);  // Left player wins
  } else if (strcmp(winnerText, "Right Win") == 0) {
    PlayVictorySound(2);  // Right player wins
  }

  delay(3000);  // Show "WIN" for 3 seconds
  ResetGame();
}


void ResetGame() {
  ledMatrix.clearDisplay(0);
  scorePlayer1 = 0;
  scorePlayer2 = 0;
  pongPosition = {8 * MAX_DEVICES / 2 - 1, 8 / 2 - 1};
  pongVelocity = {1, 1};
  deuce = false;
  StartCountdown();
}


void DisplayDeuce() {
  DisplayStaticText("DEUCE");


  // Play notification sound
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 400, 150);
    delay(200);
    noTone(BUZZER_PIN);
  }

  delay(2000);  // Pause before resuming the game
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
  int textLength = 0;

  // Calculate the length of the text
  for (int i = 0; text[i] != '\0'; i++) {
    textLength++;
  }

  // Clear the entire LED matrix
  for (int i = 0; i < MAX_DEVICES; i++) {
    ledMatrix.clearDisplay(i);
  }

  // Calculate the starting column to center the text
  int totalWidth = 8 * MAX_DEVICES;
  int textWidth = textLength * 8;
  int startCol = (totalWidth - textWidth) / 2;

  // Display each character one by one
  for (int i = 0; text[i] != '\0'; i++) {
    DisplayChar(text[i], startCol + (i * 8));
  }
}



// Function to display a single character
void DisplayChar(char letter, int startCol) {
  static const byte font[][8] = {
    // Characters: P, O, N, G, S, W (8x8 bitmaps)
    {B11111, B10001, B10001, B11111, B10000, B10000, B10000, B00000},  // P
    {B01110, B10001, B10001, B10001, B10001, B10001, B01110, B00000},  // O
    {B11111, B10001, B10001, B10001, B10001, B10001, B11111, B00000},  // N
    {B01110, B10001, B10001, B10001, B10001, B10001, B01110, B00000},  // G
    {B11111, B10000, B11110, B10000, B11111, B00000, B00000, B00000},  // S
    {B10001, B11011, B10101, B10101, B10101, B10001, B10001, B00000}   // W
  };

  int index;
  switch (letter) {
    case 'P': index = 0; break;
    case 'O': index = 1; break;
    case 'N': index = 2; break;
    case 'G': index = 3; break;
    case 'S': index = 4; break;
    case 'W': index = 5; break;
    default: return;  // Ignore invalid characters
  }

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int globalCol = startCol + col;
      int device = globalCol / 8;       // Determine which device
      int localCol = globalCol % 8;     // Column on that device

      // Set or clear the LED pixel
      if (device < MAX_DEVICES) {
        ledMatrix.setLed(device, row, localCol, font[index][row] & (1 << (7 - col)));
      }
    }
  }
}



void DisplayNumber(int num) {
  static const byte numbers[][8] = {
    {B01110, B10001, B10001, B10001, B10001, B10001, B01110, B00000}, // 0
    {B00100, B01100, B00100, B00100, B00100, B00100, B01110, B00000}, // 1
    {B01110, B10001, B00001, B00010, B00100, B01000, B11111, B00000}, // 2
    {B01110, B10001, B00001, B00110, B00001, B10001, B01110, B00000}  // 3
  };

  for (int i = 0; i < MAX_DEVICES; i++) {
    ledMatrix.clearDisplay(i);  // Clear all displays
  }

  // Center the number across the matrix
  int totalWidth = 8 * MAX_DEVICES;
  int startCol = (totalWidth - 8) / 2;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int globalCol = startCol + col;
      int device = globalCol / 8;
      int localCol = globalCol % 8;

      if (device < MAX_DEVICES) {
        ledMatrix.setLed(device, row, localCol, numbers[num][row] & (1 << (7 - col)));
      }
    }
  }
}



void PlayWelcomeMelody() {
  int melody[] = {262, 294, 330, 262, 330, 392, 440};  // Melody notes
  int noteDurations[] = {300, 300, 300, 300, 300, 400, 600};

  for (int i = 0; i < 7; i++) {
    tone(BUZZER_PIN, melody[i], noteDurations[i]);
    delay(noteDurations[i] + 50);
    noTone(BUZZER_PIN);
  }
}


