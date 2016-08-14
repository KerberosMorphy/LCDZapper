#define MICROSECONDS_TO_CYCLES(x) (16*(x))

int led=0;
unsigned short pointerX=230;
unsigned short pointerY=120;
unsigned char pointerButton=0;
unsigned char fibbles[4];
unsigned char fibbleOdd=0;
unsigned char fibbleMask=0;

void setup() {
  pinMode(0, INPUT); // Serial recieve
  pinMode(6, INPUT); // Sync
  pinMode(5, INPUT); // White
  pinMode(4, OUTPUT); // Photosensor
  pinMode(3, OUTPUT); // Dimmer
  pinMode(13, OUTPUT); // LED
  pinMode(17, INPUT_PULLUP); // Button
  pinMode(18, OUTPUT); // Trigger
  digitalWrite(3, HIGH);
  digitalWrite(18, HIGH);
  Serial.begin(9600);
  UCSR0B&=~((1<<7)|(1<<5)); // Clear RX complete + UDR empty interrupt
}

void ProcessLine(short delayValue)
{
  unsigned char lowBits=delayValue&3;
  unsigned char fours=(delayValue>>2)-1;
  unsigned char counter=4; // Make sure sync has settled at high level
  asm volatile
  (
    "      CLI\n"
    "LOOPA:\n"
    "      SBIS %5, %6\n"  // Wait for sync high
    "      RJMP LOOPA\n"
    "      DEC %2\n"
    "      BRNE LOOPA\n"
    "LOOPB:\n"
    "      SBIC %5, %6\n"  // Wait for sync low
    "      RJMP LOOPB\n"
    "      SBRS %0, 1\n"
    "      RJMP SKIP2\n"
    "      NOP\n"
    "      NOP\n"
    "      NOP\n"
    "SKIP2:\n"
    "      SBRC %0, 0\n"
    "      RJMP SKIP1\n"
    "SKIP1:\n"
    "      NOP\n"
    "      DEC %1\n"
    "      BRNE SKIP1\n"
    "      SBIC %5, %7\n"
    "      SBI %3, %4\n"
    "      CBI %3, %8\n"
    "      NOP\n"
    "      NOP\n"
    "      NOP\n"
    "      SBI %3, %8\n"
    "      SEI\n"
    : "+r" (lowBits), "+r" (fours), "+r" (counter)
    : "I" (_SFR_IO_ADDR(PORTD)), "I" (PORTD4), "I" (_SFR_IO_ADDR(PIND)), "I" (PIND6), "I" (PIND5), "I" (PORTD3)
  );
  delay(1); // Phosphor decay time
  digitalWrite(4, LOW);
}

short GetSyncTime()
{
  short syncTime = 0;
  asm volatile
  (
    "      CLI\n"
    "LOOP1:\n" // Wait for sync high
    "      SBIS %1, %2\n"
    "      RJMP LOOP1\n"
    "LOOP2:\n" // Wait for sync low
    "      ADIW %0, 1\n"   // 2 cycles
    "      SBIC %1, %2\n"  // 1 cycle when false
    "      RJMP LOOP2\n" // 2 cycles
    "      SEI\n"
    : "+r" (syncTime)
    : "I" (_SFR_IO_ADDR(PIND)), "I" (PIND6)
  );
  return syncTime;
}

unsigned char ReadFromSerial()
{
  return UDR0;
}

void PollSerial()
{
  if (UCSR0A&(1<<7)) // New serial data available
  {
    unsigned char serial=ReadFromSerial(); // Read serial data
    unsigned char num=serial>>6;
    unsigned char odd=(serial>>5)&1;
    fibbles[num]=serial&0x1F;
    if (odd!=fibbleOdd)
    {
      fibbleOdd=odd;
      fibbleMask=0;
    }
    fibbleMask|=(1<<num);
    if (fibbleMask==0xF)
    {
      pointerX=fibbles[0]+(fibbles[1]<<5);
      pointerY=fibbles[2]+(fibbles[3]<<5);
      pointerButton=0;
      if (pointerY>=512)
      {
        pointerButton=1;
        pointerY-=512;
      }
      fibbleMask=0; // Save some time by not worry about it for a bit more
    }
  }
}

void WaitForVSync()
{
  short syncTime = 0;
  while (syncTime < MICROSECONDS_TO_CYCLES(15) / 5)
  {
    syncTime = GetSyncTime();
    PollSerial();
  }
  PollSerial();
}

void WaitForHSync()
{
  short syncTime = 0;
  while (syncTime < MICROSECONDS_TO_CYCLES(3) / 5 || syncTime > MICROSECONDS_TO_CYCLES(15) / 5)
  {
    syncTime = GetSyncTime();
  }
}

short CalculateDelay(short x)
{
  long cycles=x;
  cycles=MICROSECONDS_TO_CYCLES(cycles*52);
  cycles/=640;
  cycles+=MICROSECONDS_TO_CYCLES(6);
  return (short)cycles;
}

void loop()
{
  unsigned short x,y;
  unsigned short line = 0;
  bool trigger;
  WaitForVSync();
  WaitForVSync();
  WaitForVSync();
  delayMicroseconds(20); // Make sure we ignore the first pulse (we can miss it due to interrupts)
  x = pointerX;
  y = pointerY;
  trigger = !pointerButton;
  digitalWrite(18, trigger);
  y+=35; // Ignore blank lines
  x=(x<20)?20:(x>600)?600:x;
  y=(y<40)?40:(y>275)?275:y;
  digitalWrite(13,led);
  while (true)
  {
    if (line == y)
    {
      ProcessLine(CalculateDelay(x));
      led=1-led;
      break;
    }
    else
    {
      WaitForHSync();
      PollSerial();
    }
    line++;
  }
}

