char keyMap[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
const byte rowPin[4] = {PB0, PB1, PB2, PB3};

char password[6];

unsigned long lastInputTime = 0;
const unsigned long timeoutLimit = 20000;

bool passwordStatus = false;
bool resettingPassword = false;

bool safeLockStatus = true;
bool lastDoorStatus = false;

int numberOfChar = 0;
int attemptCounter = 1;

char savedPassword[6] = {'1', '2', '3', '4','5','6'}; //Edit initial password
char tempSavedPassword[6];

void setup() {
  //Internal clock for servo

  TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
  //TCCR2A: Timer/Counter Control Register 2A - configure the A settings of timer 2
  //COM2B1 - Compare Output Mode for timer 2, Channel B (pin 3), bit 1 (there is also bit 0, but we don't need to change that)
  //WGM - Waveform Generation Mode: Change the way the timer count:
  // + for this mode, it will count up to the OCR2B value, change the pin state to LOW, and continue to 255.
  // + At 255, it will reset back to 0 and continue.

  TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
  //TCCR2B: Timer/Counter Control Register 2B - configure the B settings of timer 2
  //CS22,21,20: with the combination of 3 bits, it can change the speed of the timer.
  //With this combination of the CS bits, 1 tick of the timer will take 64 microseconds

  OCR2B = 0;
  DDRD |= (1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7);
  PORTD |= (1 << PD0);
  PORTD &= ~(1 << PD1);

  DDRC &= ~((1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5)) ;
  PORTC |= (1 << PC0) | (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5);

  DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5);
  PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3);

  lcdInit();
  lcdWrite("Welcome");
  delay(1000);
  lcdReset();
  
}

void loop() {
  if(((PINC >> PC5) & 0b00000001) == 1){ //Check if safe door is opened or not
    if(lastDoorStatus == false){
      safeDoorOpened();
    }
    lastDoorStatus = true;
    return;
  }
  if(lastDoorStatus == true){
    safeLocked();
    lcdReset();
    lastDoorStatus = false;
    lastInputTime = millis();
  }


  char key = readKeyPad();

  if(key != 'n')
    lastInputTime = millis();

  if((safeLockStatus == true && numberOfChar > 0) ||  resettingPassword == true){ //Timeout block; happen after a period of no interation
    if(millis() - lastInputTime >= timeoutLimit)
      safeTimeOut();
  }

  if(resettingPassword == true){ //Resetting password block
    if(key != 'n'){
      if ( key == '*'){
        if(numberOfChar == 6)
          registerNewPassword();
        else{
          lcdCmdWrite(0x01);
          lcdWrite("Must be 6 digits");
          delay(1000);
          numberOfChar = 0;
          lcdCmdWrite(0x01);
          lcdWrite("New code:");
          lcdCmdWrite(0xC0);
        }
      }
      else if (numberOfChar < 6 && key != 'D'){
        tempSavedPassword[numberOfChar] = key;
        char tempString[2] = {key, '\0'};
        lcdWrite(tempString);
        numberOfChar++;
        delay(250);
      }
      else if(key == 'D'){
        lcdDelete();
        delay(100);
      }
    }
    return;
  }
   if(safeLockStatus == false){ //Safe unlocked block
    if (key != 'n') {
      if(key == 'A'){
        numberOfChar = 0;
        for(int i = 0; i < 6; i++) { password[i] = 0; } 
        resettingPassword = true;
        lcdCmdWrite(0x01);
        delay(10);
        lcdWrite("New code:");
        lcdCmdWrite(0xC0);
        delay(250);
        lastInputTime = millis();
      }
      else if(key == '#'){
      safeLocked();
      lcdReset();
      }
    }
    return;
  }

  if(key != 'n'){ //Any key pressed block
    if(key == 'D'){
      lcdDelete();
      delay(100);
    }
    else if(key != '*' && key != '#' && key != 'A' && numberOfChar < 6){
    addKey(key);
    delay(10);    
    }

    if(key == '*' || numberOfChar == 6){
      passwordStatus = checkPassword();
      if(passwordStatus == false){
        if(attemptCounter >= 3){
          lockDown();
          lcdReset();
        }
        else{
          attemptCounter++;
          safeLocked();
          lcdCmdWrite(0xC0);
          lcdWrite("Access Denied");
          delay(100);

          lcdCmdWrite(0x01);
          delay(10);
          char tempString[2] = {(char)(attemptCounter + '0'), '\0'};
          lcdReset();
          lcdWrite("(Try:");
          lcdWrite(tempString);
          lcdWrite(")");
        }
      }
      else
      safeUnlocked();
    }
  }
    if(safeLockStatus == true){ //LED controller
      PORTD |= (1 << PD0);
      PORTD &= ~(1 << PD1);
    }
    else{
      PORTD &= ~(1 << PD0);
      PORTD |= (1 << PD1);
    }

    if (((PINC >> PC4) & 0b00000001) == 0) //Admin override block
      adminOverride();
}

void adminOverride(){
  numberOfChar = 0;
  for(int i = 0; i < 6; i++)
    password[i] = 0;
  resettingPassword = true;
  safeLockStatus = true; 
  lcdCmdWrite(0x01);
  delay(10);
  lcdWrite("Admin Reset");
  lcdCmdWrite(0xC0);
  lcdWrite("New code:");
  delay(250);

  lastInputTime = millis(); 
}


void registerNewPassword(){
  for(int i = 0; i < 6; i++)
    savedPassword[i] = tempSavedPassword[i];

    lcdCmdWrite(0x01);
    lcdWrite("Password Saved!");
    delay(1000);

    resettingPassword = false;
    passwordStatus = false;
    safeLockStatus = true;
    attemptCounter = 1;
    safeLocked();
    lcdReset();
}


void safeTimeOut(){
  numberOfChar = 0;
  for(int i = 0; i < 6; i++){
    password[i] = 0;
    tempSavedPassword[i] = 0;
  }
  lcdCmdWrite(0x01);
  lcdWrite("Timeout!");
  delay(1000);
  resettingPassword = false;
  safeLockStatus = true;
  attemptCounter = 1;
  safeLocked();
  lcdReset();
  lastInputTime = millis();
}


void safeDoorOpened(){
    lcdCmdWrite(0x01);
    lcdWrite("Door opened!");
    lcdCmdWrite(0xC0);
    lcdWrite("Close to proceed");
    numberOfChar = 0;
}


void safeLocked(){
  passwordStatus = false;
  safeLockStatus = true;
  lcdCmdWrite(0x01);
  delay(10);
  lcdWrite("Locked");
  OCR2B = 0;
  numberOfChar = 0;
  delay(1000);
}


void safeUnlocked(){
  safeLockStatus = false;
  lcdCmdWrite(0x01);
  delay(10);
  lcdWrite("Unlocked");
  OCR2B = 24;
  numberOfChar = 0;
  attemptCounter = 1;
}


void lcdReset(){
  lcdCmdWrite(0x01);
  delay(10);
  lcdWrite("Enter code:");
  lcdCmdWrite(0xC0);
}


void lcdDelete(){
  if(numberOfChar > 0){
    numberOfChar--;
    password[numberOfChar] = 0;
    lcdCmdWrite(0x10);
    lcdWrite(" ");
    lcdCmdWrite(0x10);
  }
}


void lockDown(){
  lcdCmdWrite(0x01);
  delay(10);
  lcdWrite("Too Many Attempt");
  attemptCounter = 1;
  numberOfChar = 0;
  for(int i = 0; i < 10; i++){
    buzzer(220, 200);
    delay(500);
  }
  lcdCmdWrite(0x01);
  lcdWrite("Locked for 1 min");
  delay(60000L);
}


void buzzer(int f, int numberOfCycles){
  for(int i = 0; i < numberOfCycles; i++){
    PORTD |= (1 << PD2);
    delayMicroseconds(500000L/f);
    PORTD &= ~(1 << PD2);
    delayMicroseconds(500000L/f);
  }
}


bool checkPassword(){
  for(int i = 0; i < 6; i++){
    if(password[i] != savedPassword[i])
      return false;
  }
  return true;
}


void addKey(char newKey){
  char tempString[2] = {'*', '\0'};
  lcdWrite(tempString);
  password[numberOfChar] = newKey;
  numberOfChar++;
}


char readKeyPad(){
  for(int i = 0; i < 4; i++){ //check each row
    PORTB &= ~(1 << rowPin[i]); 
    delay(1);
    if(((PINC >> PC0) & 0b00000001) == 0){ //check column 1
      PORTB |= (1 << rowPin[i]);
      return keyMap[i][0];
    }
    if(((PINC >> PC1) & 0b00000001) == 0){ //check column 2
      PORTB |= (1 << rowPin[i]);
      return keyMap[i][1];
    }
    if(((PINC >> PC2) & 0b00000001) == 0){ //check column 3
      PORTB |= (1 << rowPin[i]);
      return keyMap[i][2];
    }
    if(((PINC >> PC3) & 0b00000001) == 0){ //check column 4
      PORTB |= (1 << rowPin[i]);
      return keyMap[i][3];
    }
    PORTB |= (1 << rowPin[i]); //if nothing is pressed on this row, reset the row to high
  }
  return 'n'; //if nothing is pressed, return null
}


void lcdInit() 
{
    lcdCmdWrite(0x32); // 1. Send command for 4-bit initialization of the LCD
    delay(15);

    lcdCmdWrite(0x28); // 2. Configure 2 lines, 5x7 matrix in 4-bit mode
    delay(15);

    lcdCmdWrite(0x01); // 3. Clear the display screen
    delay(15);

    lcdCmdWrite(0x0C); // 4. Turn display ON, cursor OFF
    delay(15);
    
    lcdCmdWrite(0x06); // 5. Increment cursor (shift cursor to the right)
    delay(15);

    lcdCmdWrite(0x80); // 6. Set cursor to the beginning of the first line
    delay(15);
}


void lcdWrite(char *data){
  char letter;
  char dataLength = strlen(data);

  for(int i = 0; i < dataLength; i++){
    if(i == 16)
    lcdCmdWrite(0xC0);
    
    letter = data[i];

    PORTB |= (1 << PB4);

    PORTD &= ~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));

    PORTD |= (((letter & 0b00010000) >> 4) << PD4);

    PORTD |= (((letter & 0b00100000) >> 5) << PD5);

    PORTD |= (((letter & 0b01000000) >> 6) << PD6);

    PORTD |= (((letter & 0b10000000) >> 7) << PD7);

    delay(15); //make sure that all of the signals have stablized
    PORTB |= (1 << PB5);
    delay(15);
    PORTB &= ~(1 << PB5);
    delay(15);

    PORTD &= ~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));

    PORTD |= (((letter & 0b00000001) >> 0) << PD4);

    PORTD |= (((letter & 0b00000010) >> 1) << PD5);

    PORTD |= (((letter & 0b00000100) >> 2) << PD6);

    PORTD |= (((letter & 0b00001000) >> 3) << PD7);

    delay(15); //make sure that all of the signals have stablized
    PORTB |= (1 << PB5);
    delay(15);
    PORTB &= ~(1 << PB5);
    delay(15);
  }

}


void lcdCmdWrite(char data){

    PORTB &= ~(1 << PB4);

    PORTD &= ~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));

    PORTD |= (((data & 0b00010000) >> 4) << PD4);

    PORTD |= (((data & 0b00100000) >> 5) << PD5);

    PORTD |= (((data & 0b01000000) >> 6) << PD6);

    PORTD |= (((data & 0b10000000) >> 7) << PD7);

    delay(15); //make sure that all of the signals have stablized
    PORTB |= (1 << PB5);
    delay(15);
    PORTB &= ~(1 << PB5);
    delay(15);

    PORTD &= ~((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7));

    PORTD |= (((data & 0b00000001) >> 0) << PD4);

    PORTD |= (((data & 0b00000010) >> 1) << PD5);

    PORTD |= (((data & 0b00000100) >> 2) << PD6);

    PORTD |= (((data & 0b00001000) >> 3) << PD7);

    delay(15); //make sure that all of the signals have stablized
    PORTB |= (1 << PB5);
    delay(15);
    PORTB &= ~(1 << PB5);
    delay(15);
  }


