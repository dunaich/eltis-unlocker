/*
 * Digital keypad password brutforce generator
 * (C) 2023 Dmitry Dunaev <dunaich@mail.ru>
 *
 * ┌────────────────┐
 * │ ESP8266  GPIO16├──> Y2
 * │          GPIO5 ├──> Y4
 * │          GPIO4 ├──> Y1
 * │    FLASH/GPIO0 ├──> Open
 * │          GPIO2 ├──> Y3
 * │          3V3   │
 * │          GND   │
 * │          GPIO14├──> X1
 * │          GPIO12├──> X3
 * │          GPIO13├──> X2
 * │          GPIO15├──> NC
 * │       RX/GPIO3 │
 * │       TX/GPIO1 │
 * │          GND   │
 * │          3V3   │
 * └────────────────┘
 */

#include <EEPROM.h>

#define PIN_Y1      4
#define PIN_Y3      2
#define PIN_Y2      16
#define PIN_Y4      5
#define PIN_X1      14
#define PIN_X2      13
#define PIN_X3      12

#define PIN_OP      0

#define X_SHF       0
#define Y_SHF       8

#define PINKEY(x,y) (((x) << X_SHF) | ((y) << Y_SHF))
#define DEFKEY(x,y) PINKEY(PIN_X ## x, PIN_Y ## y)

#define PINX(key)   (((key) >> X_SHF) & 0xff)
#define PINY(key)   (((key) >> Y_SHF) & 0xff)

#define KEY_1       DEFKEY(3, 1)
#define KEY_2       DEFKEY(2, 1)
#define KEY_3       DEFKEY(1, 1)
#define KEY_4       DEFKEY(3, 2)
#define KEY_5       DEFKEY(2, 2)
#define KEY_6       DEFKEY(1, 2)
#define KEY_7       DEFKEY(3, 3)
#define KEY_8       DEFKEY(2, 3)
#define KEY_9       DEFKEY(1, 3)
#define KEY_C       DEFKEY(1, 4)
#define KEY_0       DEFKEY(2, 4)
#define KEY_B       DEFKEY(3, 4)

#define KBD_PRESS_DELAY     100
#define KBD_SHORT_DELAY     200
#define KBD_LONG_DELAY      10000

#define CODE_WIDTH          5
#define CHECK_TICKS         10

#define EEPROM_MAGIC        0xDD

static unsigned int kbd[] =
{
    /* Digits */
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    /* Special keys */
    KEY_B,
    KEY_C,
};

volatile unsigned int code;
volatile bool enable;
volatile int tick;

static bool is_open(void)
{
    unsigned char n = 0;
    int i;

    for (i = 0; i < CHECK_TICKS; ++i) {
        if (digitalRead(PIN_OP) == LOW)
            n += 1;
    }

    return n > (CHECK_TICKS / 2);
}

static void key_reset(void)
{
    /* Reset Y lines */
    digitalWrite(PIN_Y1, LOW);
    digitalWrite(PIN_Y2, LOW);
    digitalWrite(PIN_Y3, LOW);
    digitalWrite(PIN_Y4, LOW);

    /* Reset X lines */
    digitalWrite(PIN_X1, LOW);
    digitalWrite(PIN_X2, LOW);
    digitalWrite(PIN_X3, LOW);
}

static void key_down(unsigned int key)
{
    unsigned char x = PINX(key);
    unsigned char y = PINY(key);

    digitalWrite(x, HIGH);
    digitalWrite(y, HIGH);

    delay(KBD_PRESS_DELAY);
}

static void key_press(unsigned int key)
{
    unsigned char x = PINX(key);
    unsigned char y = PINY(key);

    digitalWrite(x, HIGH);
    digitalWrite(y, HIGH);

    delay(KBD_PRESS_DELAY);

    digitalWrite(x, LOW);
    digitalWrite(y, LOW);

    delay(KBD_PRESS_DELAY);
}

static inline void key_call(void)
{
    key_press(KEY_B);
}

static inline void key_clear(void)
{
    key_press(KEY_C);
}

static void code_send(unsigned int dcode)
{
    unsigned char digit;
    int i;

    for (i = 0; i < CODE_WIDTH; ++i) {
        digit = dcode % 10;
        key_press(kbd[digit]);
        dcode = dcode / 10;
    }

    key_call();
}

static unsigned int code_load(void)
{
    volatile unsigned int code1, code2;
    byte *pc1 = (byte *)&code1;
    byte *pc2 = (byte *)&code2;
    int i;

   EEPROM.begin(10);

   if (EEPROM.read(0) != EEPROM_MAGIC)
        return 0;

    for (i = 0; i < sizeof(code); ++i) {
        pc1[i] = EEPROM.read(i + 1);
        pc2[i] = EEPROM.read(i + 5);
    }

    if (code1 != code2)
        return 0;

    return code1;
}

static void code_save(unsigned int dcode)
{
    unsigned int xcode = dcode;
    byte *pc = (byte *)&xcode;
    int i;

    for (i = 0; i < sizeof(xcode); ++i) {
        EEPROM.write(i + 1, pc[i]);
        EEPROM.write(i + 5, pc[i]);
    }

    EEPROM.write(0, EEPROM_MAGIC);
    EEPROM.commit();
}

void control(void)
{
    char c;

    /* Check data in serial console */
    if (!Serial.available())
        return;

    /* Get char */
    c = Serial.read();

    /* Check operation status */
    if (enable && c != 'p' && c != 'P' && isAlpha(c)) {
        printf("Unlocker is enabled!\n");
        return;
    }

    switch (c) {
    case 'p':
    case 'P':
        printf("Unlocker is %s\n", enable ? "paused" : "continued");
        enable = !enable;
        break;
    case 'z':
    case 'Z':
        printf("Clear EEPROM\n");
        code_save(0);
        break;
    case 'c':
    case 'C':
        printf("press: C\n");
        key_clear();
        break;
    case 'b':
    case 'B':
        printf("press: B\n");
        key_call();
        break;
    case '0'...'9':
        printf("press: %c\n", c);
        c -= 0x30;
        key_press(kbd[c]);
        break;
    case 'h':
    case 'H':
        puts("\nHotkeys:\n"
            "--------\n"
            "P    - pause/resume unlocker\n"
            "Z    - clear EEPROM\n"
            "C    - emulate press 'C' key\n"
            "B    - emulate press 'B' key\n"
            "0..9 - emulate press digit key\n"
            "\n");
    default:
        break;
    }
}

void setup()
{
    /* Startup delay */
    delay(1000);

    /* Setup serial and cleanup output buffer */
    Serial.begin(115200);
    Serial.println();

    /* Setup open pin as input */
    pinMode(PIN_OP, INPUT_PULLUP);

    /* Setup keyboard pins as input */
    pinMode(PIN_Y1, OUTPUT);
    pinMode(PIN_Y2, OUTPUT);
    pinMode(PIN_Y3, OUTPUT);
    pinMode(PIN_Y4, OUTPUT);
    pinMode(PIN_X1, OUTPUT);
    pinMode(PIN_X2, OUTPUT);
    pinMode(PIN_X3, OUTPUT);

    /* Reset lines */
    key_reset();

    /* Load last code from EEPROM */
    code = code_load();
    printf("Start from code %05u\n", code);

    /* No code entered yet */
    tick = 0;

    /* Start operate */
    enable = true;
}

void loop()
{
    /* Perform control */
    control();

    /* No operations if disabled */
    if (!enable)
        return;

    printf("Try %d code %05u\n", tick, code);
    code_send(code);
    
    /* Check open */
    if (is_open()) {
        printf("Last code is %05u\n", code);
        code_save(code);
        enable = false;
    }

    /* Next try delay */
    if (tick == 1) {
        delay(KBD_LONG_DELAY);
        tick = 0;
    } else {
        delay(KBD_SHORT_DELAY);
        tick += 1;
    }

    /* Next code */
    code += 1;
}
