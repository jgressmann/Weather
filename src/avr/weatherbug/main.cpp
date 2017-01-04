#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/cpufunc.h>
#include <avr/signature.h>
#include <util/delay.h>
#include <util/atomic.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>


#include "Watchdog.h"
#include "../../Misc.h"
#include "../Debug.h"
#include "../USART0.h"

#include "../SPI.h"
#include "../RF24.h"


#include "../../Batman.h"
#include "../../Network.h"
#include "../../Time.h"
#include "../../TCP.h"
#include "../DHT.h"

// Atmel doc, p. 51 & 55
#define MAX_WDT_SLEEP_MS 8192

#define FEAT_USART0     0
#define FEAT_RF24       1

static volatile uint8_t s_Features[2] NOINIT;

static
inline
void
FEAT_Init() {
    memset((void*)s_Features, 0, sizeof(s_Features));
}

static
void
FEAT_Acquire(uint8_t features) {
    //Commented by the virtue that all code paths using it
    // top-level assert that interrupts are off
    //ASSERT_INTERRUPTS_OFF(return, "main");
    if (features & _BV(FEAT_USART0)) {
//        DEBUG_P("USART0 %d\n", s_Features[FEAT_USART0]);
        if (0 == s_Features[FEAT_USART0]++) {
            power_usart0_enable();
            USART0_Init();
            DEBUG_P("USART0 started\n");
        }
    }

    if ((features & _BV(FEAT_RF24))) {
//        DEBUG_P("RF24 %d\n", s_Features[FEAT_RF24]);
        if (0 == s_Features[FEAT_RF24]++) {
            power_spi_enable();

            RF24_PowerUp();

            // set CE
            PORTB |= _BV(PB1);
            DEBUG_P("RF24 started\n");
        }
    }
}

static
void
FEAT_Release(uint8_t features) {
    //ASSERT_INTERRUPTS_OFF(return, "main");
    if (features & _BV(FEAT_USART0)) {
        if (--s_Features[FEAT_USART0] == 0) {
            DEBUG_P("USART0 stopped\n");
            USART0_SendFlush();
            USART0_Uninit();
            power_usart0_disable();
        }
    }

    if ((features & _BV(FEAT_RF24))) {
        if (--s_Features[FEAT_RF24] == 0) {
            DEBUG_P("RF24 stopped\n");

            // clear CE line
            PORTB &= ~_BV(PB1);

            RF24_PowerDown();
            RF24_FlushRx();
            RF24_FlushTx();
            RF24_UpdateRegister(RF24_REG_STATUS, RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS | RF24_STATUS_RX_DR, RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS | RF24_STATUS_RX_DR);
            RF24_TxQueueClear();

            // this sequence appears to reliably make the power
            // comsumption for r.1.2 drop from 100uA to ~25uA
            RF24_PowerUp();
            _delay_ms(5);
            RF24_PowerDown();

            power_spi_disable();
        }
    }
}

static
inline
int8_t
FEAT_Available(uint8_t features) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    if ((features & _BV(FEAT_USART0)) && !s_Features[FEAT_USART0]) {
        return 0;
    }

    if ((features & _BV(FEAT_RF24)) && !s_Features[FEAT_RF24]) {
        return 0;
    }

    return 1;
}


typedef void (*WorkCallback)(void* ctx);
typedef struct _Work_Item {
    uint32_t TimeTillUpdate;
    WorkCallback Callback;
    void* Ctx;
} Work_Item;
volatile uint8_t s_Work_ItemCount NOINIT;
Work_Item s_Work_Items[8] NOINIT;

static
inline
void
WORK_Add(void* ctx, WorkCallback callback) {
    ASSERT_FILE(s_Work_ItemCount < _countof(s_Work_Items), return, "main");
    s_Work_Items[s_Work_ItemCount].Callback = callback;
    s_Work_Items[s_Work_ItemCount].Ctx = ctx;
    s_Work_Items[s_Work_ItemCount].TimeTillUpdate = 0;
    ++s_Work_ItemCount;
//    DEBUG_P("Work: %" PRIu32 " add %d\n", Time_Now(), s_Work_ItemCount);
}

#define WORK_AddEx(ctx, callback) \
    do { \
        /*DEBUG_P(STRINGIFY(callback) "\n"); */ \
        WORK_Add(ctx, callback); \
    } while (0)


static
void
WORK_Remove(WorkCallback callback) {
//    DEBUG_P("Work: %" PRIu32 " remove %d ", Time_Now(), s_Work_ItemCount);
    for (uint8_t i = s_Work_ItemCount - 1; i < s_Work_ItemCount; --i) {
        if (s_Work_Items[i].Callback == callback) {
            s_Work_Items[i] = s_Work_Items[--s_Work_ItemCount];
            return;
        }
    }

//    ASSERT_FILE(0, "main");
//    DEBUG_P("%d\n", s_Work_ItemCount);
}

static
void
WORK_Update(uint32_t elapsed) {
    ASSERT_INTERRUPTS_OFF(return, "main");
    for (uint8_t i = s_Work_ItemCount - 1; i < s_Work_ItemCount; --i) {
        if (s_Work_Items[i].TimeTillUpdate <= elapsed) {
            s_Work_Items[i].TimeTillUpdate = 0;
            //DEBUG_P("Work: %" PRIu32 " idx %d\n", Time_Now(), i);
            s_Work_Items[i].Callback(s_Work_Items[i].Ctx);
        } else {
            s_Work_Items[i].TimeTillUpdate -= elapsed;
            //DEBUG_P("Work: %" PRIu32 " idx %d: %" PRIu32 "\n", Time_Now(), i, s_Work_Items[i].TimeTillUpdate);
        }
    }
}

static
void
WORK_RequestUpdate(WorkCallback callback, uint32_t time) {
    //DEBUG("Work: update %p %" PRIu32 "\n", callback, time);
    for (uint8_t i = 0; i < s_Work_ItemCount; ++i) {
        if (s_Work_Items[i].Callback == callback) {
            s_Work_Items[i].TimeTillUpdate = time;
            break;
        }
    }
}

static
uint32_t
WORK_NextUpdate() {
    //DEBUG("Work: %u\n", s_Work_Callback_Count);
    uint32_t next = ~UINT32_C(0);
    for (uint8_t i = 0; i < s_Work_ItemCount; ++i) {
        if (s_Work_Items[i].TimeTillUpdate < next) {
            next = s_Work_Items[i].TimeTillUpdate;
        }
    }

    return next;
}

static
inline
void
WORK_Init() {
    s_Work_ItemCount = 0;
}


static volatile uint8_t s_SendsQueued;
static
void
RF24_BatchSendCallback(void*) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    WORK_Remove(RF24_BatchSendCallback);
    s_SendsQueued = 0;
    if (FEAT_Available(_BV(FEAT_RF24))) {
        RF24_TxQueueProcess();
    } else {
        //DEBUG_P("psq clear\n");
        RF24_TxQueueClear();
    }
}

static
void
RF24_QueueForBatchSend(uint8_t* ptr, uint8_t size) {
//    ASSERT_FILE(ptr, return, "main");
//    ASSERT_FILE(size, return, "main");
//    ASSERT_INTERRUPTS_OFF(return, "main");
    if (FEAT_Available(_BV(FEAT_RF24))) {
Retry:
        if (!RF24_TxQueueSubmit(ptr, size)) {
            RF24_TxQueueProcess();
            goto Retry;
        }

        if (!s_SendsQueued) {
            WORK_AddEx(NULL, RF24_BatchSendCallback);
        }

        ++s_SendsQueued;
//            DEBUG_P("qfs %u\n", s_SendsQueued);
    } else {
        if (s_SendsQueued) {
            s_SendsQueued = 0;
            WORK_Remove(RF24_BatchSendCallback);
            RF24_TxQueueClear();
        }
    }
}


#define DHT_Uninit() \
    do { \
        /* don't ask me why but this needs to be like so */ \
        DDRD |= _BV(PD5); \
        PORTD &= ~(_BV(PD5) | _BV(PD6)); \
        DDRD &= ~_BV(PD6); \
    } while (0)


#define DHT_Init() \
    do { \
        /* data & power line to low */ \
        PORTD &= ~(_BV(PD5) | _BV(PD6)); \
        /* turn on power to DHT */ \
        DDRD |= _BV(PD5); \
        PORTD |= _BV(PD5); \
    } while (0)


enum {
    MODE_STATE_UNINITIALIZED = -1,
    MODE_STATE_INITIAL = 0,
    MODE_STATE_SERVICE = 1,
    MODE_STATE_DEFAULT = 2,
};


struct ModeContext {
    uint32_t StartOfInterval;
    DHT_Context DHTContext;
    uint16_t Iterations;
    uint8_t USART0Index;
    int8_t CurrentState;
    uint8_t TargetState         : 2;
    uint8_t Enter               : 1;
    uint8_t Interval            : 1;
    uint8_t FeaturesReleased    : 1;
    uint8_t Run                 : 1;
    uint8_t DHTTries;
    uint8_t Temperature;
    uint8_t Humidity;
    char USART0Buffer[32];
};

static ModeContext s_ModeContext NOINIT;

#define RF24_PIPE_BASE_ADDRESS 0xdeadbeef




/* save MCUSR early on to figure out why we booted */
static uint8_t s_Mcusr NOINIT;
static void Bootstrap() __attribute__((naked,used,section(".init3")));
static void Bootstrap() {
    s_Mcusr = MCUSR;
    MCUSR = 0;
    WDT_Off();
}

#define NAME "Weatherbug"
#define VERSION_MAJOR 2
#define VERSION_MINOR 5
#define VERSION_PATCH 0
#define NODE_NAME_BUFFER_LENGTH 7
#define NODE_NAME_BUFFER_SIZE (NODE_NAME_BUFFER_LENGTH + 1)
static const char s_VersionString_P[] PROGMEM = NAME " " STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH);
static const char DHT_Error_P[] PROGMEM = "DHT error %d\n";
static const char Network_Address_Format_P[] PROGMEM = "%#02x\n";

enum {
    DHT11,
    DHT22
};

struct {
    uint8_t RF24_Power : 3;
    uint8_t DHT_Type : 1;
    uint8_t Unused : 4;
} s_Flags NOINIT;


#define INITIALIZED_SIGNATURE ((uint16_t)(((uint16_t)('J') << 8) | 'G'))
static uint16_t s_EEP_Initialized EEMEM;
static uint8_t s_EEP_RF24_Channel EEMEM;
static uint8_t s_EEP_Flags EEMEM;
static char s_EEP_Name[NODE_NAME_BUFFER_SIZE] EEMEM;
static uint8_t s_EEP_RF24_DataRate EEMEM;
static uint8_t s_EEP_Network_MyId EEMEM;
static uint8_t s_EEP_Network_TargetId EEMEM;
static uint8_t s_EEP_Network_Ttl EEMEM;
static uint8_t s_EEP_BATV_Offset EEMEM;
static uint8_t s_EEP_DHT_Temperature_Offset EEMEM;
static uint8_t s_EEP_DHT_Humidity_Offset EEMEM;
static uint16_t s_EEP_DHT_Humidity_Factor EEMEM;
static uint8_t s_RF24_Channel NOINIT;
static char s_Name[NODE_NAME_BUFFER_SIZE] NOINIT;
static uint8_t s_RF24_DataRate NOINIT;
static uint8_t s_Network_TargetId NOINIT;
static int8_t s_BATV_Offset NOINIT;
static int8_t s_DHT_Temperature_Offset NOINIT;
static int8_t s_DHT_Humidity_Offset NOINIT;
static int16_t s_DHT_Humidity_Factor NOINIT;



#define Flush(x) USART0_SendFlush()

static FILE* s_FILE_USART0;
static
int
USART0_PutChar(char c, FILE *stream) {
    (void)stream;
    USART0_SendByte(c);
    return 0;
}

static
int
USART0_GetChar(FILE *stream) {
    (void)stream;
    if (USART0_HasReceivedByte()) {
        return USART0_FetchReceivedByte();
    }

    return EOF;
}


static const char Error_Prefix_P[] PROGMEM = "ERROR ";
static const char OK_P[] PROGMEM = "OK\n";
#define DHT_A_SCALE 256
#define DHT_A_SHIFT 8


#define SendError(stream, fmt, ...) \
    do { \
        fprintf_P(stream, Error_Prefix_P); \
        fprintf_P(stream, fmt, __VA_ARGS__); \
    } while (0)

#define SendOK(stream) fprintf_P(stream, OK_P)


static
inline
void
PrintHelp(FILE* stream) {
    const char* helpString = PSTR(
        "Usage:\n"
        "  ?     : prints this help\n"

        "  !cfad : activates default configuration\n"
        "  !cfld : loads configuration from EEPROM\n"
        "  !cfst : stores configuration to EEPROM\n"

        "  ?btvl : read battery voltage\n"
        "  ?btvo : read battery voltage offset\n"
        "  !btvo : <value> write battery voltage offset\n"

        "  !dflt : activates default mode\n"
        "  ?dhtr : read DHT sensor\n"
        "  ?dhtt : read DHT sensor type (0 == DHT11, 1 == DHT22)\n"
        "  !dhtt : <value> write DHT sensor type\n"

        "  ?mcsr : read MCUSR from boot\n"

        "  ?name : read node name\n"
        "  !name : write node name (7 chars)\n"
        "  ?nmid : read my network id\n"
        "  !nmid : <value> write my network id\n"
        "  ?ntid : read target network id\n"
        "  !ntid : <value> write target network id\n"
        "  ?nttl : read network packet TTL\n"
        "  !nttl : <value> write network packet TTL\n"

        "  ?rchl : read RF24 radio channel\n"
        "  !rchl : <value> write RF24 radio channel\n"
        "  ?rdtr : read RF24 data rate\n"
        "  !rdtr : <value> write RF24 data rate (0=250KBPS, 1=1MPBS, 2=2MBPS)\n"
        "  ?rdmp : dump RF24 state\n"
        "  ?rpwr : read RF24 radio power setting (min = 0, 3 = max)\n"
        "  !rpwr : <value> write RF24 power setting\n"
        "  !rset : soft reset device\n"

        "  !serv : activates service mode\n"
        "  ?snha : read DHT hum. factor a ." STRINGIFY(DHT_A_SCALE) ", line fit\n"
        "  !snha : <value> write DHT hum. factor a ." STRINGIFY(DHT_A_SCALE) ", line fit\n"
        "  ?snhb : read DHT hum. offset b, line fit\n"
        "  !snhb : <value> write DHT hum. offset b, line fit\n"
        "  ?snto : read DHT temperature offset .1\n"
        "  !snto : <value> write DHT temperature offset\n"

        "  ?vers : prints the firmware version\n"

    );

    fprintf_P(stream, helpString);
}


static
void
CONF_Load() {
    s_RF24_Channel = eeprom_read_byte(&s_EEP_RF24_Channel);
    *reinterpret_cast<uint8_t*>(&s_Flags) = eeprom_read_byte(&s_EEP_Flags);
    eeprom_read_block(s_Name, s_EEP_Name, sizeof(s_Name));
    s_RF24_DataRate = eeprom_read_byte(&s_EEP_RF24_DataRate);
    Network_SetAddress(eeprom_read_byte(&s_EEP_Network_MyId));
    s_Network_TargetId = eeprom_read_byte(&s_EEP_Network_TargetId);
    Network_SetTtl(eeprom_read_byte(&s_EEP_Network_Ttl));
    s_BATV_Offset = (int8_t)eeprom_read_byte(&s_EEP_BATV_Offset);
    s_DHT_Temperature_Offset = (int8_t)eeprom_read_byte(&s_EEP_DHT_Temperature_Offset);
    s_DHT_Humidity_Offset = (int8_t)eeprom_read_byte(&s_EEP_DHT_Humidity_Offset);
    s_DHT_Humidity_Factor = (int16_t)eeprom_read_word(&s_EEP_DHT_Humidity_Factor);
}

static
void
CONF_Store() {
    eeprom_write_byte(&s_EEP_DHT_Temperature_Offset, s_DHT_Temperature_Offset);
    eeprom_write_word(&s_EEP_DHT_Humidity_Factor, s_DHT_Humidity_Factor);
    eeprom_write_byte(&s_EEP_DHT_Humidity_Offset, s_DHT_Humidity_Offset);
    eeprom_write_byte(&s_EEP_BATV_Offset, s_BATV_Offset);
    eeprom_write_byte(&s_EEP_Network_Ttl, Network_GetTtl());
    eeprom_write_byte(&s_EEP_Network_TargetId, s_Network_TargetId);
    eeprom_write_byte(&s_EEP_Network_MyId, Network_GetAddress());
    eeprom_write_block(s_Name, s_EEP_Name, sizeof(s_Name));
    eeprom_write_byte(&s_EEP_Flags, *reinterpret_cast<const uint8_t*>(&s_Flags));
    eeprom_write_byte(&s_EEP_RF24_DataRate, s_RF24_DataRate);
    eeprom_write_byte(&s_EEP_RF24_Channel, s_RF24_Channel);
    eeprom_write_word(&s_EEP_Initialized, INITIALIZED_SIGNATURE);
}

static
void
CONF_ActivateDefault() {
    s_RF24_Channel = 76; // see RF24.cpp, begin()
    s_Flags.RF24_Power = RF24_TX_PWR_MAX;
    s_Flags.DHT_Type = DHT11;
    s_Flags.Unused = 0;
    strcpy_P(s_Name, PSTR("fixme"));
    s_RF24_DataRate = RF24_DR_2MBPS;
    Network_SetAddress(0xff);
    s_Network_TargetId = 0xff;
    Network_SetTtl(0xff);
    s_BATV_Offset = 0;
    s_DHT_Temperature_Offset = 0;
    s_DHT_Humidity_Offset = 0;
    s_DHT_Humidity_Factor = DHT_A_SCALE;
}


static
int8_t
TryParseULong(const char* str, unsigned long* value, int8_t base) {
    char* end;

    // abcdef are stop chars
    end = NULL;
    *value = strtoul(str, &end, base);
    if (end && end != str) {
        return 1;
    }

    return 0;
}

static
int8_t
TryParseLong(const char* str, long* value, int8_t base) {
    char* end;

    // abcdef are stop chars
    end = NULL;
    *value = strtol(str, &end, base);
    if (end && end != str) {
        return 1;
    }

    return 0;
}


static void RF24_Init();
static uint16_t BATV_Read();
static uint16_t BATV_ToMilliVolt(uint16_t counts);


static
inline
void SEN_Correct(uint8_t temperature, uint8_t humidity, uint8_t& outTemperature, uint8_t& outHumidity) {
    uint16_t t = temperature, h = humidity;
    t *= 10;
    t += s_DHT_Temperature_Offset;
    t /= 10;
    outTemperature = t;
    h *= s_DHT_Humidity_Factor;
    h >>= DHT_A_SHIFT;
    h += s_DHT_Humidity_Offset;
    outHumidity = h;
}

static
int8_t
SEN_Read(uint8_t& temperature, uint8_t& humidity) {
//    ASSERT_INTERRUPTS_OFF(return -1, "main");
    // power up DHT
    DHT_Init();
    DHT_Context ctx;
    ctx.ddr = &DDRD;
    ctx.port = &PORTD;
    ctx.pin = &PIND;
    ctx.mask = _BV(PD6);
    DHT_PrepareRead(&ctx);
    int8_t error;
    switch (s_Flags.DHT_Type) {
    case DHT11:
    default:
        _delay_ms(DHT11_PREPARE_TIME_MS);
        error = DHT11_Read(&ctx, &temperature, &humidity);
        break;
    case DHT22:
        _delay_ms(DHT22_PREPARE_TIME_MS);
        error = DHT22_Read(&ctx, &temperature, &humidity);
        break;
    }

#ifndef NDEBUG
    for (uint8_t i = 0; i < sizeof(g_DHT_Bits); ++i)
    {
        fprintf_P(s_FILE_Debug, PSTR("%c"), ' ' + g_DHT_Bits[i]);
    }
    fprintf(s_FILE_Debug, "\n");
#endif
    DHT_Uninit();
    return error;
}


static
int8_t
ProcessCommand(FILE* stream, char* input, uint8_t len) {

#define PARSE_INT8(str) \
    do { \
        long l; \
        if (!TryParseLong(str, &l, 0)) { \
            SendError(stream, PSTR("Could not convert '%s' to long\n"), str); \
            return 0; \
        } \
        i8Arg = (int8_t)l; \
    } while (0)

#define PARSE_UINT8(str) \
    do { \
        unsigned long l; \
        if (!TryParseULong(str, &l, 0)) { \
            SendError(stream, PSTR("Could not convert '%s' to ulong\n"), str); \
            return 0; \
        } \
        u8Arg = (uint8_t)l; \
    } while (0)


#define PARSE_INT16(str) \
    do { \
        long l; \
        if (!TryParseLong(str, &l, 0)) { \
            SendError(stream, PSTR("Could not convert '%s' to long\n"), str); \
            return 0; \
        } \
        i16Arg = (int16_t)l; \
    } while (0)

    if (len == 0) {
        return 0;
    }

    switch (input[0]) {
    case '!':
    case '?':
        break;
    default:
        return -1;
    }

    const int8_t printcmd = 1;
    uint8_t u8Arg;
    int8_t i8Arg;
    int16_t i16Arg;
    const uint8_t o = 1;
    const uint8_t r = input[0] == '?';

    if (printcmd) {
        const char* charFormat = PSTR("  %c");
        const char* hexFormat = PSTR(" %02x");
        fprintf_P(stream, PSTR("CMD (chars):"));
        for (uint8_t i = 0; i < len; ++i) {
            fprintf_P(stream, charFormat, (char)input[i]);
        }
        fprintf_P(stream, PSTR("\nCMD (hex)  :"));
        for (uint8_t i = 0; i < len; ++i) {;
            fprintf_P(stream, hexFormat, input[i]);
        }
        putc('\n', stream);
    }

    if (len == 1 && input[0] == '?') {
        PrintHelp(stream);
        return 1;
    }

    if (len < 5) {
        goto Error;
    }

    switch (toupper(input[o])) {
    case 'B':
        switch (toupper(input[o+1])) {
        case 'T':
            switch (toupper(input[o+2])) {
            case 'V':
                switch (toupper(input[o+3])) {
                case 'L': {
                        uint16_t counts = BATV_Read();
                        uint16_t mv = BATV_ToMilliVolt(counts);
                        fprintf_P(stream, PSTR("counts %u, offset %d, %u [mV]\n"), counts, s_BATV_Offset, mv);
                        return 1;
                    } break;
                case 'O': {
                        if (r) {
                            fprintf_P(stream, PSTR("battery voltage offset (counts) %d\n"), s_BATV_Offset);
                            return 1;
                        }
                        PARSE_INT8(input + o + 5);
                        s_BATV_Offset = i8Arg;
                        SendOK(stream);
                        return 1;
                    } break;
                }
            }
            break;
        } break;
    case 'C':
        switch (toupper(input[o+2])) {
        case 'A':
            CONF_ActivateDefault();
            SendOK(stream);
            return 1;
        case 'L':
            CONF_Load();
            SendOK(stream);
            return 1;
        default:
            CONF_Store();
            SendOK(stream);
            return 1;
        }
        break;
    case 'D':
        switch (toupper(input[o+1])) {
        case 'F':
            s_ModeContext.TargetState = MODE_STATE_DEFAULT;
            SendOK(stream);
            return 1;
        default:
            switch (toupper(input[o+3])) {
                case 'R': {
                    uint8_t temperature, humidity;
                    int8_t error = SEN_Read(temperature, humidity);
                    switch (error) {
                    case 0:
                        SEN_Correct(temperature, humidity, temperature, humidity);
                        fprintf_P(stream, PSTR("%d °C (offset .1 %d), %d %%rh (a ." STRINGIFY(DHT_A_SCALE) " %d, b %d)\n"), (int)temperature, s_DHT_Temperature_Offset, (int)humidity, s_DHT_Humidity_Factor, s_DHT_Humidity_Offset);
                        break;
                    default:
                        fprintf_P(stream, DHT_Error_P, error);
                        break;
                    }
                    return 1;
                } break;
                case 'T': {
                    if (r) {
                       switch (s_Flags.DHT_Type) {
                       case DHT11:
                       default:
                           fprintf_P(stream, PSTR("DHT11\n"));
                           break;
                       case DHT22:
                           fprintf_P(stream, PSTR("DHT22\n"));
                           break;
                       }
                       return 1;
                    }

                    PARSE_INT8(input + o + 5);
                    s_Flags.DHT_Type = i8Arg;
                    SendOK(stream);
                    return 1;
                } break;
            }
            break;
        }
        break;
    case 'M':
        fprintf_P(stream, PSTR("%02x\n"), s_Mcusr);
        return 1;
    case 'N':
        switch (toupper(input[o+1])) {
        case 'A': {
                if (r) {
                    fprintf(stream, "%s\n", s_Name);
                    return 1;
                }
                if (len > NODE_NAME_BUFFER_LENGTH) len = NODE_NAME_BUFFER_LENGTH;
                memcpy(s_Name, input + o + 5, len);
                s_Name[NODE_NAME_BUFFER_LENGTH] = 0;
                SendOK(stream);
                return 1;
            }
            break;
        case 'M': { // my network id
                if (r) {
                    fprintf_P(stream, Network_Address_Format_P, Network_GetAddress());
                    return 1;
                }

                PARSE_UINT8(input + o + 4);;
                Network_SetAddress(u8Arg);
                SendOK(stream);
                return 1;
            }
            break;
        case 'T':
            switch (toupper(input[o+2])) {
            case 'T': { // TTL
                if (r) {
                    fprintf(stream, "%u\n", Network_GetTtl());
                    return 1;
                }

                PARSE_UINT8(input + o + 4);
                Network_SetTtl(u8Arg);
                SendOK(stream);
                return 1;
            } break;
            default: { // target network id
                    if (r) {
                        fprintf_P(stream, Network_Address_Format_P, s_Network_TargetId);
                        return 1;
                    }

                    PARSE_UINT8(input + o + 4);
                    s_Network_TargetId = u8Arg;
                    SendOK(stream);
                    return 1;
                } break;
            }
        }
        break;
    case 'R':
        switch (toupper(input[o+1])) {
        case 'C': { // channel
                if (r) {
                    fprintf(stream, "%d\n", s_RF24_Channel);
                    return 1;
                }

                PARSE_UINT8(input + o + 4);
                s_RF24_Channel = u8Arg;
                RF24_Init();
                SendOK(stream);
                return 1;
            }
            break;
        case 'D':;
            switch (toupper(input[o+2])) {
            case 'T': { // data rate
                    if (r) {
                        switch (s_RF24_DataRate) {
                        case RF24_DR_250KBPS:
                            u8Arg = 0;
                            break;
                        case RF24_DR_1MBPS:
                            u8Arg = 1;
                            break;
                        default:
                            u8Arg = 2;
                            break;
                        }
                        fprintf(stream, "%d\n", u8Arg);
                        return 1;
                    }

                    PARSE_UINT8(input + o + 4);
                    switch (u8Arg) {
                    case 0:
                        s_RF24_DataRate = RF24_DR_250KBPS;
                        break;
                    case 1:
                        s_RF24_DataRate = RF24_DR_1MBPS;
                        break;
                    default:
                        s_RF24_DataRate = RF24_DR_2MBPS;
                        break;
                    }
                    RF24_Init();
                    SendOK(stream);
                    return 1;;
                } break;
            case 'M':  // dump
                RF24_Dump(stream);
                return 1;
            }
            break;
        case 'P': { // power
                if (r) {
                    switch (s_Flags.RF24_Power) {
                    case RF24_TX_PWR_M18DB:
                        u8Arg = 0;
                        break;
                    case RF24_TX_PWR_M12DB:
                        u8Arg = 1;
                        break;
                    case RF24_TX_PWR_M6DB:
                        u8Arg = 2;
                        break;
                    default:
                        u8Arg = 3;
                        break;
                    }
                    fprintf(stream, "%d\n", u8Arg);
                    return 1;
                }

                PARSE_UINT8(input + o + 4);
                switch (u8Arg) {
                case 0:
                    s_Flags.RF24_Power = RF24_TX_PWR_M18DB;
                    break;
                case 1:
                    s_Flags.RF24_Power = RF24_TX_PWR_M12DB;
                    break;
                case 2:
                    s_Flags.RF24_Power = RF24_TX_PWR_M6DB;
                    break;
                default:
                    s_Flags.RF24_Power = RF24_TX_PWR_MAX;
                    break;
                }
                RF24_Init();
                SendOK(stream);
                return 1;
            } break;
        case 'S':
            SendOK(stream);
            Flush(stream);
            DEV_SoftReset();
            return 1;
        }
        break;
    case 'S':
        switch (toupper(input[o+1])) {
        case 'E':
            s_ModeContext.TargetState = MODE_STATE_SERVICE;
            SendOK(stream);
            return 1;
        case 'N':
            switch (toupper(input[o+2])) {
                case 'H':
                    switch (toupper(input[o+3])) {
                        case 'A':
                            if (r) {
                                fprintf_P(stream, PSTR("DHT humidity factor a ." STRINGIFY(DHT_A_SCALE) " %d\n"), s_DHT_Humidity_Factor);
                                return 1;
                            }

                            PARSE_INT16(input + o + 4);
                            s_DHT_Humidity_Factor = i16Arg;
                            SendOK(stream);
                            return 1;
                        case 'B':
                            if (r) {
                                fprintf_P(stream, PSTR("DHT humidity offset %d\n"), s_DHT_Humidity_Offset);
                                return 1;
                            }

                            PARSE_INT8(input + o + 4);
                            s_DHT_Humidity_Offset = i8Arg;
                            SendOK(stream);
                            return 1;
                    }
                    break;
                case 'T':
                    if (r) {
                        fprintf_P(stream, PSTR("DHT temperature offset .1 %d\n"), s_DHT_Temperature_Offset);
                        return 1;
                    }

                    PARSE_INT8(input + o + 4);
                    s_DHT_Temperature_Offset = i8Arg;
                    SendOK(stream);
                    return 1;
            }
            break;
        }
        break;
    case 'V':
        fprintf_P(stream, s_VersionString_P);
        putc('\n', stream);
        return 1;
    }

Error:
    fprintf_P(stream, PSTR("Unknown command '%s'. Try '?'.\n"), input);

    return 0;


#undef PARSE_INT8
#undef PARSE_UINT8
#undef PARSE_UINT16
}


static
int8_t
ProcessCommands(FILE* stream, char* buffer, uint8_t* index, uint8_t size) {
    int8_t commands = 0;
    int c;
    int8_t result;
    while ((c = getc(stream)) != EOF) {
        if (*index >= size) {
            DEBUG_P("I/O buffer full, no command\n");
            *index = 0;
        }
        if (c < 0x20) { // space
            if (c == '\n' || c == '\r') {
                buffer[*index] = 0;
                result = ProcessCommand(stream, buffer, *index);
                *index = 0;
                if (result == -1) {
                    return result;
                }
                Flush(stream);
                ++commands;
            } else {
                // ignore all control chars
            }
        } else {
//            DEBUG_P("%c", c);
            buffer[(*index)++] = (char)c;
        }
    }

    return commands;
}



static volatile uint8_t s_Watchdog_Expired;
static
void
WatchdogCallback() {
    //USART0_SendString("WDT\n");
    WDT_Off();
    s_Watchdog_Expired = 1;
}


#define ever (;;)

static
void
RF24_MessageReceivedHandler(uint8_t* ptr, uint8_t size) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
//    ASSERT_FILE(ptr, return, "main");
//    DEBUG_P("RF24 %u bytes\n", size);

    // sanity check
    if (size != sizeof(NetworkPacket)) {
        DEBUG_P("Drop ill-sized net packet %u\n", size);
        return; // size mismatch, drop
    }

    NetworkPacket* packet = (NetworkPacket*)ptr;

    switch (packet->Type) {
    case BATMAN_PACKET_TYPE:
        Batman_Process(packet);
        break;
    case TIME_PACKET_TYPE:
        Time_Process(packet);
        break;
    case TCP_PACKET_TYPE:
        TCP_Process(packet);
        break;
    default:
        // Unknown packet type, drop
        DEBUG_P("Unknown packet: ");
#ifndef NDEBUG
        for (uint8_t i = 0; i < size; ++i) {
            DEBUG_P("%02x ", ptr[i]);
        }
        DEBUG_P("\n");
#endif
        break;
    }
}

#define RF24_PollEx() \
    do { \
        /*ASSERT_INTERRUPTS_OFF(break, "main"); */ \
        if (FEAT_Available(_BV(FEAT_RF24))) { \
            RF24_Poll(); \
        } \
    } while (0)






static
void
RF24_OfflineReceiveHandler(void*) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    WORK_Remove(RF24_OfflineReceiveHandler);
    if (FEAT_Available(_BV(FEAT_RF24))) {
        RF24_Poll();
    }
}

ISR(PCINT0_vect) {
//    DEBUG_P("PCINT0\n");
    WORK_Remove(RF24_OfflineReceiveHandler);
    if (FEAT_Available(_BV(FEAT_RF24))) {
        WORK_AddEx(NULL, RF24_OfflineReceiveHandler);
    }
}


static
void
NetworkSendCallback(NetworkPacket* packet) {
    RF24_QueueForBatchSend((uint8_t*)packet, sizeof(*packet));
}

static
void
RF24_Init() {
    // We don't know in what state we get the device so
    // reset everything


    // The CE line needs to be high to actually perform RX/TX
    DDRB |= _BV(PB1); // pin 9 -> output
    PORTB &= ~_BV(PB1); // low, this is the CE line

#if F_CPU >= 16000000L
    SPI_Master_Init(SPI_MSB_FIRST, SPI_CLOCK_DIV_4, SPI_CLOCK_SPEED_1X, SPI_MODE_0);
#else
    SPI_Master_Init(SPI_MSB_FIRST, SPI_CLOCK_DIV_4, SPI_CLOCK_SPEED_2X, SPI_MODE_0);
#endif

    // Arduino pin 8 (PB0 on ATmega328P) is connected to the interrupt line
    DDRB &= ~_BV(PB0); // pin 8
    // activate pull up resistor
    PORTB |= _BV(PB0);

    // enable PCINT0
    PCICR |= _BV(PCIE0);
    PCMSK0 |= _BV(PCINT0);
    PCIFR = 0; // clear any flags
    RF24_SetInterruptMask(RF24_IRQ_MASK_MAX_RT | RF24_IRQ_MASK_TX_DS);

    RF24_SetCrc(RF24_CRC_16);
    RF24_SetChannel(s_RF24_Channel);
    // Looks like using byte zero won't work
    RF24_SetAddressWidth(RF24_ADDR_WITDH_5);
    RF24_SetTxAddress(RF24_PIPE_BASE_ADDRESS, 0x01);
    RF24_SetRxBaseAddress(RF24_PIPE_BASE_ADDRESS);
    RF24_SetRxAddresses(0x01, 0, 0, 0, 0, 0);
    RF24_SetRxPipeEnabled(0x01);
    RF24_SetRxPayloadSizes(RF24_MAX_PAYLOAD_SIZE, 0, 0, 0, 0, 0);
    RF24_SetDataRate(s_RF24_DataRate);
    RF24_SetTxPower(s_Flags.RF24_Power);

    // These too lines effectively disable Enhanced Shockburst
    RF24_SetPipeAutoAcknowledge(0);
    RF24_SetTxRetries(15, 0);

    // clear any pending interrupts
    RF24_UpdateRegister(RF24_REG_STATUS, RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS | RF24_STATUS_RX_DR, RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS | RF24_STATUS_RX_DR);

    RF24_PowerUp();
    RF24_SetRxMode();
    RF24_FlushTx();
    RF24_FlushRx();


    // finally set CE
    PORTB |= _BV(PB1);

#ifndef NDEBUG
    RF24_Dump(s_FILE_Debug);
#endif
}

#ifndef NDEBUG
extern "C" {
FILE* s_FILE_Debug NOINIT;
}
#endif




#define DoMode(ctx, enter, exit, iteration) \
    do { \
        const uint8_t Step = 8; /* This needs to be set to something were the device _does not_ sleep, else no USART */ \
        if (ctx->Enter) { \
            ctx->Enter = 0; \
            enter; \
        } \
        int8_t result; \
        uint8_t any; \
        while (1) { \
            any = 0; \
            if (ctx->TargetState != ctx->CurrentState) { \
                ctx->CurrentState = ctx->TargetState; \
                ctx->Enter = 1; \
                exit; \
                WORK_RequestUpdate(ProcessMode, 0); \
                break; \
            } \
            result = ProcessCommands(s_FILE_USART0, ctx->USART0Buffer, &ctx->USART0Index, sizeof(ctx->USART0Buffer)); \
            if (result == -1) { \
                USART0_SendString(ctx->USART0Buffer); \
            } \
            any |= result != 0; \
            iteration; \
            if (!any) { \
                WORK_RequestUpdate(ProcessMode, Step); \
                break; \
            } \
        } \
    } while (0)

static
void
ProcessMode(void* ctx) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    ModeContext* c = (ModeContext*)ctx;

    switch (c->CurrentState) {
    case MODE_STATE_UNINITIALIZED:
        FEAT_Acquire(_BV(FEAT_USART0) | _BV(FEAT_RF24));
        DEBUG_P("Mode: uninit\n");
        c->CurrentState = MODE_STATE_INITIAL;
        c->TargetState = MODE_STATE_INITIAL;
        c->Enter = 1;
        c->Interval = 0;
        c->Iterations = 0;
        c->USART0Index = 0;
        c->DHTContext.ddr = &DDRD;
        c->DHTContext.port = &PORTD;
        c->DHTContext.pin = &PIND;
        c->DHTContext.mask = _BV(PD6);
        break;
    case MODE_STATE_INITIAL: {
            DoMode(
                c,
                fprintf_P(s_FILE_USART0, PSTR("**** STARTUP MODE ****\nDevice will process commands for 10 seconds\n")),
                ;,
                if (c->Iterations > 0 && (c->Iterations % (1000/Step)) == 0) {
                       USART0_SendByte('.');
                }
                if (c->Iterations >= (10000/Step)) {
                    USART0_SendByte('\n');
                    c->TargetState = MODE_STATE_DEFAULT;
                    any = 1;
                }
                if (any) {
                    c->Iterations = 0;
                } else {
                    ++c->Iterations;
                });
        } break;
    case MODE_STATE_SERVICE: {
            DoMode(
                c,
                fprintf_P(s_FILE_USART0, PSTR("**** SERVICE MODE ****\n")),
                ;,
                ;);
        } break;
    case MODE_STATE_DEFAULT: {
            DoMode(
                c,
                do {
                    USART0_SendString_P(PSTR("**** DEFAULT MODE ****\n"));
                    c->Interval = 0;
                    c->Run = 0;
                    c->FeaturesReleased = 0;
                    WORK_RequestUpdate(ProcessMode, Step);
                } while (0),
                do {
                    DEBUG_P("Default: exit\n");
                    DHT_Uninit();

                    if (c->FeaturesReleased) {
                        c->FeaturesReleased = 0;
                        DEBUG_P("Default: acquire\n");
                        FEAT_Acquire(_BV(FEAT_USART0) | _BV(FEAT_RF24));
                    }
                } while (0),
                if (c->Interval) {
                    DEBUG_P("Default: int %" PRIu32 "\n", Time_Now());
                    c->Iterations = 0;
                    c->Interval = 0;
                    c->Run = 1;
                    c->DHTTries = 0;
                    c->Temperature = 0xff; // guard for send code
                    c->StartOfInterval = Time_Now();

                    DHT_Init();
                    DHT_PrepareRead(&c->DHTContext);

                    if (c->FeaturesReleased) {
                        c->FeaturesReleased = 0;
                        DEBUG_P("Default: acquire\n");
                        FEAT_Acquire(_BV(FEAT_USART0) | _BV(FEAT_RF24));
                    }

                    fprintf_P(s_FILE_USART0, PSTR("Resume command processing for %" PRIu32 " [ms]\n"), NETWORK_RXTX_DURATION);
                    WORK_RequestUpdate(ProcessMode, 0);

                } else if (c->Run) {
                    WORK_RequestUpdate(ProcessMode, Step);
                    ++c->Iterations;

                    // Start as early as possible, TCP will handle data resending
                    const uint8_t MaxDHTTries = 3;
                    const uint32_t now = Time_Now();
                    bool dhtPrepared = false;
                    switch (s_Flags.DHT_Type) {
                    case DHT11:
                        dhtPrepared = DHT11_IsReadPrepared(now - c->StartOfInterval);
                        break;
                    case DHT22:
                        dhtPrepared = DHT22_IsReadPrepared(now - c->StartOfInterval);
                        break;
                    }

                    if (dhtPrepared &&
                        c->DHTTries < MaxDHTTries && c->Temperature == 0xff) {
                        DEBUG_P("Default: read DHT\n");
                        ++c->DHTTries;
                        bool send = c->DHTTries >= MaxDHTTries;
                        bool bogus = false;
                        int8_t error = -1;
                        switch (s_Flags.DHT_Type) {
                        case DHT11:
                            error = DHT11_Read(&c->DHTContext, &c->Temperature, &c->Humidity);
                            break;
                        case DHT22:
                            error = DHT22_Read(&c->DHTContext, &c->Temperature, &c->Humidity);
                            break;
                        }

                        switch (error) {
                        case 0:
                            if (c->Temperature == 0 && c->Humidity == 0) {
                                bogus = true;
                                if (!send) { // sentinel, have another try
                                    c->Temperature = 0xff;
                                }
                                DEBUG_P("Default: bogus reading\n");
                            } else {
                                send = true;
                            }
                            break;
                        default:
                            bogus = true;
                            fprintf_P(s_FILE_USART0, DHT_Error_P, error);
                            break;
                        }

                        if (send) {
                            if (bogus) {
                                c->Temperature = 0;
                                c->Humidity = 0;
                            } else {
                                SEN_Correct(c->Temperature, c->Humidity, c->Temperature, c->Humidity);
                            }
                            DEBUG_P("Default: °C %u (O %d), %%rH %u (O %d)\n", c->Temperature, s_DHT_Temperature_Offset, c->Humidity, s_DHT_Humidity_Offset);
                            c->DHTTries = 0xff;


                            uint16_t mv = 0;
                            const uint16_t counts = BATV_Read();
                            mv = BATV_ToMilliVolt(counts);
                            DEBUG_P("Default: C %u, O %d, %u [mV]\n", counts, s_BATV_Offset, mv);

                            fprintf_P(s_FILE_USART0, PSTR("Device reports %u °C, %u %%rH, %u mV\n"), c->Temperature, c->Humidity, mv);
                            // let TCP handle it from here
                            uint8_t via = Batman_Route(s_Network_TargetId);
                            char buffer[TCP_PAYLOAD_SIZE];
                            int8_t bytes = snprintf_P((char*)buffer, sizeof(buffer), PSTR("WB%s;%d;%d;%u;%02x"), s_Name, (int)c->Temperature, (int)c->Humidity, mv, via);
//                              DEBUG_P("(%d) %s\n", bytes, buffer);
                            TCP_Send(s_Network_TargetId, (const uint8_t*)buffer, bytes);
                        }

                    } else if (!IsInWindow32(now, NETWORK_RXTX_DURATION, c->StartOfInterval)) {
                        DEBUG_P("Default: stop\n");
                        c->Run = 0;
                    }
                } else {
                    DHT_Uninit();
                    const uint32_t x = Time_IsSynced() ? Time_TimeToNextInterval() : NETWORK_PERIOD - NETWORK_RXTX_DURATION;
                    fprintf_P(s_FILE_USART0, PSTR("Pause command processing for %" PRIu32 " [ms]\n"), x);
                    if (!c->FeaturesReleased) {
                        DEBUG_P("Default: release\n");
                        c->FeaturesReleased = 1;
                        FEAT_Release(_BV(FEAT_USART0) | _BV(FEAT_RF24));
                    }
                    WORK_RequestUpdate(ProcessMode, NETWORK_PERIOD); // will wake up before this due to callback
                    break;
                });
        } break;
    default:
        DEBUG_P("Unhandled state %d\n", c->CurrentState);
        break;
    }
}

#undef DoMode

static
void
UpdateBatman(void*) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    WORK_RequestUpdate(UpdateBatman, 8);
    Batman_Update();
}

static
void
BroadcastBatman(void*) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    WORK_RequestUpdate(BroadcastBatman, 512);
    Batman_Broadcast();
}


static
void
UpdateTcp(void*) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    WORK_RequestUpdate(UpdateTcp, 8);
    TCP_Update();
}


enum {
    SYNC_TIME_STATE_UNINITIALIZED,
    SYNC_TIME_STATE_UNSYNCED,
    SYNC_TIME_STATE_SYNCED_ACTIVATE_RF24,
    SYNC_TIME_STATE_SYNCED_DEACTIVATE_RF24,
};


struct SyncTimeContext {
    uint8_t State   : 2;
    uint8_t On      : 1;
};

#define TIME_SYNC_SCAN_DEVIDER 8

static
void
SyncTimeSyncWindowCallback(int8_t what) {
//    ASSERT_INTERRUPTS_OFF(return, "main");
    DEBUG_P("sync window %d\n", what);

    switch (what) {
    case TIME_INT_START:
        s_ModeContext.Interval = 1;
        WORK_RequestUpdate(ProcessMode, 0);
        WORK_AddEx(NULL, UpdateBatman);
        WORK_AddEx(NULL, BroadcastBatman);
        WORK_AddEx(NULL, UpdateTcp);
        break;
    case TIME_INT_STOP:
        WORK_Remove(UpdateBatman);
        WORK_Remove(BroadcastBatman);
        WORK_Remove(UpdateTcp);
        TCP_Purge();
        break;
    }
}

static const char s_ContinueScanMessage_P[] PROGMEM = "Resume scan for %" PRIu32 " [ms]\n";
static const uint32_t ScanTime = NETWORK_RXTX_DURATION / TIME_SYNC_SCAN_DEVIDER;

static void SyncTime(void* ctx);
static
inline
void
SyncTimeScanStart(SyncTimeContext* c) {
    FEAT_Acquire(_BV(FEAT_USART0));
    fprintf_P(s_FILE_USART0, s_ContinueScanMessage_P, ScanTime);
    FEAT_Release(_BV(FEAT_USART0));

    WORK_RequestUpdate(SyncTime, ScanTime);
    Time_NotifyStartListening(1);
    c->State = SYNC_TIME_STATE_UNSYNCED;
    c->On = 1;
}

static
void
SyncTimeScanContinue(SyncTimeContext* c) {
    // Devide the network listen period into
    // n parts and listen only 1/nth the time.
    // Since time requests are broadcast quite
    // frequently this should be sufficient to
    // to pick up a peer to latch on to
    FEAT_Acquire(_BV(FEAT_USART0));
    if (c->On) {
        const uint32_t x = NETWORK_RXTX_DURATION - NETWORK_RXTX_DURATION / TIME_SYNC_SCAN_DEVIDER;
        fprintf_P(s_FILE_USART0, PSTR("Pause scan for %" PRIu32 " [ms]\n"), x);
        c->On = 0;
        FEAT_Release(_BV(FEAT_RF24));
        WORK_RequestUpdate(SyncTime, x);
    } else {
        fprintf_P(s_FILE_USART0, s_ContinueScanMessage_P, ScanTime);
        c->On = 1;
        FEAT_Acquire(_BV(FEAT_RF24));
        WORK_RequestUpdate(SyncTime, ScanTime);
        Time_NotifyStartListening(1);
    }

    FEAT_Release(_BV(FEAT_USART0));
}

static
void
SyncTime(void* ctx) {
//    ASSERT_INTERRUPTS_OFF(return, "main");

    SyncTimeContext* c = (SyncTimeContext*)ctx;
    switch (c->State) {
    case SYNC_TIME_STATE_UNINITIALIZED:
        DEBUG_P("SYNC_TIME_STATE_UNINITIALIZED\n");
        Time_Init();
        Time_SetSyncWindowCallback(SyncTimeSyncWindowCallback);
        Batman_Init();
        TCP_Init();
        FEAT_Acquire(_BV(FEAT_RF24));
        SyncTimeScanStart(c);
        break;
    case SYNC_TIME_STATE_UNSYNCED: {
            DEBUG_P("SYNC_TIME_STATE_UNSYNCED\n");
            if (Time_IsSynced()) {
                Time_NotifyStopListening();
                uint32_t timeTillInterval = Time_TimeToNextInterval();
//                DEBUG_P("ST time till int %" PRIu32 " -> %" PRIu32 "\n", timeTillInterval, Time_Now() + timeTillInterval);
                FEAT_Acquire(_BV(FEAT_USART0));


                if (timeTillInterval > 0) {
                    fprintf_P(s_FILE_USART0, PSTR("Scan successful, sleep for %" PRIu32 " [ms]\n"), timeTillInterval);
                    c->State = SYNC_TIME_STATE_SYNCED_ACTIVATE_RF24;
                    WORK_RequestUpdate(SyncTime, timeTillInterval);
                    if (c->On) {
                        FEAT_Release(_BV(FEAT_RF24));
                    }
                } else {
                    USART0_SendString_P(PSTR("Scan successful\n"));
                    c->State = SYNC_TIME_STATE_SYNCED_DEACTIVATE_RF24;
                    WORK_RequestUpdate(SyncTime, NETWORK_RXTX_DURATION);
                    Time_NotifyStartListening(0);
                }
                FEAT_Release(_BV(FEAT_USART0));
            } else {
                SyncTimeScanContinue(c);
            }
        } break;
    case SYNC_TIME_STATE_SYNCED_ACTIVATE_RF24:
        DEBUG_P("SYNC_TIME_STATE_SYNCED_ACTIVATE_RF24\n");
        FEAT_Acquire(_BV(FEAT_RF24));
        WORK_RequestUpdate(SyncTime, NETWORK_RXTX_DURATION);
        c->State = SYNC_TIME_STATE_SYNCED_DEACTIVATE_RF24;
        Time_NotifyStartListening(0);
        break;
    case SYNC_TIME_STATE_SYNCED_DEACTIVATE_RF24:
        DEBUG_P("SYNC_TIME_STATE_SYNCED_DEACTIVATE_RF24\n");
        Time_NotifyStopListening();
        if (Time_IsSynced()) {
            uint32_t timeTillInterval = Time_TimeToNextInterval();
//            DEBUG_P("ST time till int %" PRIu32 " -> %" PRIu32 "\n", timeTillInterval, Time_Now() + timeTillInterval);
            if (timeTillInterval > 0) {
                c->State = SYNC_TIME_STATE_SYNCED_ACTIVATE_RF24;
                WORK_RequestUpdate(SyncTime, timeTillInterval);
                FEAT_Release(_BV(FEAT_RF24));
            } else {
                WORK_RequestUpdate(SyncTime, NETWORK_RXTX_DURATION);
                Time_NotifyStartListening(0);
            }
        } else {
            SyncTimeScanStart(c);
        }
        break;
    }
}

static
void
SleepOneMillisecond() {
    if (FEAT_Available(_BV(FEAT_RF24))) {
        _delay_us(200);
        RF24_Poll();
        _delay_us(200);
        RF24_Poll();
        _delay_us(200);
        RF24_Poll();
        _delay_us(200);
        RF24_Poll();
    } else {
        _delay_ms(1);
    }
}

static
inline
void
BATV_Init() {
//    ADCSRA = _BV(ADIF); // turn off AD and clear any interrupt bit
//    power_adc_disable();

//    DDRD |= _BV(PD7); // output
//    PORTD |= _BV(PD7); // high

//    DDRC |= _BV(PC0); // output
//    PORTC |= _BV(PC0); // high

//    return;
    ADCSRA = _BV(ADIF); // turn off AD and clear any interrupt bit

    DDRC = 0;   // all input
    PORTC = 0;  // tri-state
    DIDR0 = 63; // disable digital I/O on unused analog pins

    // GND line of voltage divider is connected to PD7
    // input line to A0
    DDRD &= ~_BV(PD7); // input
    PORTD &= ~_BV(PD7); // tri-state

    // disabling the ADC effectively
    // voids all changes to ADCSRA/B, ADMUX
    power_adc_disable();
}

static
uint16_t
BATV_Read() {
//    return 0;
    power_adc_enable();

    ASSERT_FILE(!(ADCSRA & _BV(ADEN)), return 0, "main");
//    ADCSRA = _BV(ADIF); // turn off AD and clear any interrupt bit

    // setup prescaler to 128 this will work for 8 and 16 MHz
#if F_CPU >= 16000000L
    ADCSRA |= 7; // div by 128
#else
    ADCSRA |= 6; // div by 64
#endif

    ADCSRB = 0; // free run mode

    // internal 1.1V reference voltage with cap on AREF pin
    // also connect A0 pin
    ADMUX = _BV(REFS1) | _BV(REFS0);

    DDRD |= _BV(PD7); //output

    // enable ADC
    ADCSRA |= _BV(ADEN); // enable conversion


    const int8_t StableThreshold = 3;
    const int8_t Samples = 7;
    uint16_t sum = 0;
    uint16_t l, h;
    for (int8_t i = 0; i < Samples; ++i) {
        ADCSRA |= _BV(ADSC);

#if !defined(NDEBUG) && 0
        uint8_t y = ADCSRA;
        uint8_t z = ADMUX;
        DEBUG_P("ADCSRA %02x\n", y);
        DEBUG_P("ADMUX %02x\n", z);
#endif
        loop_until_bit_is_clear(ADCSRA, ADSC);

        l = ADCL;
        h = ADCH;
        uint16_t result = h;
        result <<= 8;
        result |= l;

        if (i >= StableThreshold) {
            sum += result;
        }

        // Clearing of ADIF is apparently not necessary
        //ADCSRA |= _BV(ADIF); // clear interrupt flag
    }

    sum /= Samples - StableThreshold;

    DEBUG_P("A0: %u\n", sum);


    // it is not enough to cut power to the ADC circuitry,
    // ADC needs to be turned off beforehand
    ADCSRA = _BV(ADIF); // turn off ADC and clear any interrupt bit

    DDRD &= ~_BV(PD7); // input

    power_adc_disable();

    return sum;
}

static
inline
uint16_t
BATV_ToMilliVolt(uint16_t counts) {
    // for 0 counts = 0v
//    const int32_t m1k = INT32_C(6449);
//    const int32_t b1k = INT32_C(0);

//    const int32_t m1k = INT32_C(6111);
//    const int32_t b1k = INT32_C(275059);

//    int32_t mv = b1k;
//    mv += m1k * counts;
//    mv >>= 10;
//    return mv;

    // ref. voltage is 1.1v
    // @ 6v we have a target voltage of U6 = 6*(10KOhm/(50KOhm + 10KOhm)) = 1.0v, ratio  1/1.1 = 0.909090, counts = 930
    // 930 @ 6v, 0 @ 0v
    // x/count = 6v/930 => x = 6v*count/930

    counts += s_BATV_Offset;

    int32_t mv = 6606;
    mv *= counts;
    mv >>= 10;
    return mv;
}

int
main() {
    cli();
    // turn off stuff we absolutely don't use
    //power_ada_disable();
    //power_usb_disable();
    //power_rtc_enable();
    //power_adca_disable();
    //power_evsys_disable();



    power_twi_disable();
    power_timer0_disable();
    power_timer1_disable();
    power_timer2_disable();
    DHT_Uninit();

    Network_SetSendCallback(NetworkSendCallback);
    WDT_SetCallback(WatchdogCallback);

    FEAT_Init();
    WORK_Init();

    BATV_Init();
    USART0_Init();
    s_FILE_USART0 = fdevopen(USART0_PutChar, USART0_GetChar);
#ifndef NDEBUG
    stderr = s_FILE_Debug = s_FILE_USART0;
#endif
    //SleepTest();
//    USART0_SendString("Start\n");


//    {
//        char buf[32];
//        snprintf(buf, sizeof(buf), "MCUSR %02x\n", s_Mcusr);
//        USART0_SendString(buf);
//    }

    if (s_Mcusr & _BV(PORF)) {
        const char* message = PSTR("Power-on reset.\n");
        USART0_SendString_P(message);
    }
    if (s_Mcusr & _BV(EXTRF)) {
        const char* message = PSTR("External reset!\n");
        USART0_SendString_P(message);
    }
    if (s_Mcusr & _BV(BORF)) {
        const char* message = PSTR("Brownout reset!\n");
        USART0_SendString_P(message);
    }
    if (s_Mcusr & _BV(WDRF)) {
        const char* message = PSTR("Watchdog reset!\n");
        USART0_SendString_P(message);
    }
   // if (mcucsr & (1<<JTRF )) DEBUG(("JTAG reset!\n");

    {
        fprintf_P(s_FILE_USART0, PSTR("Hello there!\nThis is "));
        fprintf_P(s_FILE_USART0, s_VersionString_P);
        putc('\n', s_FILE_USART0);
        fprintf_P(s_FILE_USART0, PSTR("Copyright (c) 2016 Jean Gressmann <jean@0x42.de>"));
        putc('\n', s_FILE_USART0);
        putc('\n', s_FILE_USART0);
    }
    {
        uint16_t confInitialized =  eeprom_read_word(&s_EEP_Initialized);
        if (confInitialized != INITIALIZED_SIGNATURE) {
            const char* str = PSTR("No configuration found in EEPROM. Activating default configuration.\n");
            USART0_SendString_P(str);
            CONF_ActivateDefault();
            CONF_Store();
        } else {
            CONF_Load();
            const char* str = PSTR("Configuration loaded.\n");
            USART0_SendString_P(str);
        }
    }


    RF24_Init();
    RF24_SetMessageReceivedCallback(RF24_MessageReceivedHandler);

    DEBUG_P("RF24 stopped\n");
    RF24_PowerDown();
    power_spi_disable();
    DEBUG_P("USART0 stopped\n");
    USART0_SendFlush();
    USART0_Uninit();
    power_usart0_disable();

#ifndef NDEBUG
    FEAT_Acquire(_BV(FEAT_USART0));
#endif

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    SyncTimeContext syncTimeContext;
    memset(&syncTimeContext, 0, sizeof(syncTimeContext));
    WORK_Add(&syncTimeContext, SyncTime);

    memset(&s_ModeContext, 0, sizeof(s_ModeContext));
    s_ModeContext.CurrentState = MODE_STATE_UNINITIALIZED;
    WORK_Add(&s_ModeContext, ProcessMode);

    uint32_t sleepStart = Time_Now();

    for ever {
        const uint32_t sleepEnd = Time_Now();
        const uint32_t elapsed = sleepEnd - sleepStart;
        sleepStart = sleepEnd;

        WORK_Update(elapsed);

        const uint32_t sleep = WORK_NextUpdate();

        if (sleep) {

            //DEBUG_P("Sleep: %" PRIu32 "\n", sleep);
#if 0
            const uint16_t SleepStep = 128;
            if (sleep >= SleepStep) {
                //DEBUG("Z");
                _delay_ms(SleepStep);
                Time_Update(SleepStep);
                RF24_PollEx();
            } else {
                for (uint8_t i = 0; i < sleep; ++i) {
                    //DEBUG("z");
                    SleepOneMillisecond();
                }

                Time_Update(sleep);
                RF24_PollEx();
            }

            //DEBUG("Sleep done\n");

#else
            uint16_t passed;
            uint16_t millisecondsToSleep = sleep > MAX_WDT_SLEEP_MS ? MAX_WDT_SLEEP_MS : sleep;
            if (millisecondsToSleep < 16) {
                passed = millisecondsToSleep;
                while (millisecondsToSleep--) {
                    SleepOneMillisecond();
                }
            } else {
                uint8_t prescaler = 9;
                uint16_t millis = MAX_WDT_SLEEP_MS;
                while (millis > millisecondsToSleep) {
                    --prescaler;
                    millis >>= 1;
                }

                //DEBUG_P("%u", prescaler);

#ifndef NDEBUG
                USART0_SendFlush();
#endif

                s_Watchdog_Expired = 0;
                WDT_On(0, 1, prescaler);

                sleep_enable();
                sleep_bod_disable();
                sei();
                sleep_cpu();
                cli();
                sleep_disable();

                if (s_Watchdog_Expired) {
                    //  DEBUG_P("Sleep ok %u\n", millis);
                    passed = millis;
                } else { // woke up before WDT timeout
                    WDT_Off();
                    passed = 0;
                }
            }

            if (passed) {
                Time_Update(passed);
            }
#endif
        } else {
            RF24_PollEx();
        }
    }

    return 0;
}

