#include <inttypes.h>
#include <avr/io.h>
#include <avr/eeprom.h>
//#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>

#include "Debug.h"
#include "Watchdog.h"

#define NAME "Weatherbug"
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_STRING NAME " " STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR)
#define COPYRIGHT_STRING "Copyright (c) 2016 Jean Gressmann <jean@0x42.de>"

#define SendError(...) \
    do { \
        char buf[64]; \
        snprintf(buf, sizeof(buf), "ERROR " __VA_ARGS__); \
        USART0_SendString(buf); \
    } while (0)

#define NOINIT __attribute__ ((section (".noinit")))

enum {
    Mode_Initial,
    Mode_Default,
    Mode_Service,
    Mode_Count
};

#define ASSERT_VALID_MODE \
    do { \
        uint8_t sreg = SREG; \
        int x; \
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { x = s_Mode; } \
        if (sreg & (1<<7)) DEBUG("INT ON\n"); \
        if (x < 0 || x >= Mode_Count) { \
            DEBUG("MODE FAIL: %d\n", x); \
            assert(0 && "Mode error"); \
        } \
    } while (0)


static int8_t s_Mode = Mode_Initial;
static const char* const s_ModeNames[Mode_Count] = {
    "Initial",
    "Default",
    "Service",
};

static char s_IoBuffer[64];
static uint8_t s_IoBufferIndex;

static uint8_t EEMEM s_EEP_RF24Id  = 0;
static uint8_t s_RF24Id NOINIT;
static uint16_t EEMEM s_EEP_SecondsToSleep = 5;
static uint16_t s_SecondsToSleep NOINIT;

#define ClearCommandBuffer() \
    do { \
        s_IoBufferIndex = 0; \
        s_IoBuffer[0] = 0;  \
    } while (0)

#define LOG(...) \
    do { \
        char buf[96]; \
        snprintf(buf, sizeof(buf), __VA_ARGS__); \
        USART0_SendString(buf); \
        USART0_SendFlush(); \
    } while (0)

#define POWERDOWN_MODE 3
#define DEFAULT_CMD_PROCESS_TIME_SECONDS 15
#define DEFAULT_CMD_PROCESS_TIME_MILLISECONDS (1000 * DEFAULT_CMD_PROCESS_TIME_SECONDS)

#define LogModeChange(was, is) LOG("Mode change: %s -> %s\n", s_ModeNames[was], s_ModeNames[is])

enum {
    Cmd_Eeprom_Read,
    Cmd_Eeprom_Write,
    Cmd_RF24_SetNodeId,
};


static
void
PrintHelp() {
    USART0_SendString("Usage:\n");
    USART0_SendString("  help: prints this help\n");
    USART0_SendString("  ver : prints the firware version\n");
    USART0_SendString("  rst : reset device\n");
    USART0_SendString("  ser : Activates service mode\n");
    USART0_SendString("  def : Activates default mode\n");
    USART0_SendString("  nir : <value> write node id for RF24 communication (least signficant byte)\n");
    USART0_SendString("  niw : read node id for RF24 communication (least signficant byte)\n");
    USART0_SendString("  cst : stores configuration to EEPROM\n");
    USART0_SendString("  cld : loads configuration from EEPROM\n");
    USART0_SendString("  stp : stop command processing\n");
    USART0_SendString("  mod : read device mode\n");
//    USART0_SendString("  epr : <addr> <bytes> reads up to 4 bytes from EEPROM\n");
//    USART0_SendString("  epw : <addr> <bytes> <value> writes value to EEPROM\n");

}

static
void
CONF_Load() {
    s_RF24Id = eeprom_read_byte(&s_EEP_RF24Id);
    s_SecondsToSleep = eeprom_read_word(&s_EEP_SecondsToSleep);
}

static
void
CONF_Store() {
    eeprom_write_byte(&s_EEP_RF24Id, s_RF24Id);
    eeprom_write_word(&s_EEP_SecondsToSleep, s_SecondsToSleep);
}

static
int
TryParseByte(const char* str, uint8_t* byte) {
    char* end;

    assert(str);
    assert(byte);

    // abcdef are stop chars
    end = NULL;
    *byte = (uint8_t)strtoul(str, &end, 0);
    if (end > str) {
        return 1;
    }

    return 0;
}

static
int
ProcessCommand(int* stop) {
    const int printcmd = 1;
    const int printmode = 0;
    int index = 0;
    int cmd = -1;
    uint32_t u32Arg;
    uint8_t u8Arg;
    //char c;
    char* input = s_IoBuffer;
    char* end;
    const char* separator;
    char* result;
    char* commandString = NULL;

    assert(stop);

    if (printcmd) {
        size_t len = strlen(s_IoBuffer);
        char buf[4];
        USART0_SendString("CMD (chars):");
        for (size_t i = 0; i < len; ++i) {
            sprintf(buf, "  %c", (char)s_IoBuffer[i]);
            USART0_SendString(buf);
        }
        USART0_SendString("\n");
        USART0_SendString("CMD (hex)  :");
        for (size_t i = 0; i < len; ++i) {
            sprintf(buf, " %02x", s_IoBuffer[i]);
            USART0_SendString(buf);
        }
        USART0_SendString("\n");
    }

    if (printmode) {
        ASSERT_VALID_MODE;
        char buf[16];
        snprintf(buf, sizeof(buf), "mode %d\n", s_Mode);
        USART0_SendString(buf);
    }

    separator = " \t";
    while ((result = strtok_r(input, separator, &end)) != NULL) {
        input = NULL;
        switch (index) {
        case 0:
            commandString = result;
            if (strcasecmp("HELP", result) == 0) {
                PrintHelp();
                return 1;
            }

            if (strcasecmp("VER", result) == 0) {
                USART0_SendString(VERSION_STRING "\n");
                return 1;
            }

            if (strcasecmp("STP", result) == 0) {
                *stop = 1;
                USART0_SendString("OK\n");
                return 1;
            }

            if (strcasecmp("NIR", result) == 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Id: %d %02x\nLSB %02X%02X%02X%02X%02X MSB\n", s_RF24Id, s_RF24Id, s_RF24Id, 0, 0, 0, 0);
                USART0_SendString(buf);
                return 1;
            }

            if (strcasecmp("MOD", result) == 0) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d\n", s_Mode);
                USART0_SendString(buf);
                return 1;
            }

            if (strcasecmp("SER", result) == 0) {
                s_Mode = Mode_Service;
                USART0_SendString("OK\n");
                return 1;
            }

            if (strcasecmp("DEF", result) == 0) {
                s_Mode = Mode_Default;
                USART0_SendString("OK\n");
                return 1;
            }

            if (strcasecmp("EPR", result) == 0) {
                ++index;
                cmd = Cmd_Eeprom_Read;
            } else if (strcasecmp("EPW", result) == 0) {
                ++index;
                cmd = Cmd_Eeprom_Write;
            } else if (strcasecmp("NIW", result) == 0) {
                ++index;
                cmd = Cmd_RF24_SetNodeId;
            } else if (strcasecmp("CST", result) == 0) {
                CONF_Store();
                USART0_SendString("OK\n");
                return 1;
            } else if (strcasecmp("CLD", result) == 0) {
                CONF_Load();
                USART0_SendString("OK\n");
            } else {
                SendError("Unknown command '%s'. Try 'help'\n", result);
                return 0;
            }
            break;
        case 1:
            switch (cmd) {
            case Cmd_RF24_SetNodeId:
                if (TryParseByte(result, &u8Arg)) {
                    s_RF24Id = u8Arg;
                    USART0_SendString("OK\n");
                    return 1;
                } else {
                    SendError("Could not convert %s to uint8_t\n", result);
                }
                break;
            }

            break;
        }
    }

    if (index > 0) {
        SendError("Command '%s' requires arguments\n", commandString);
    }

    // empty command

    return 0;
}


static
int
ProcessCommands(int* stop) {
    int commands = 0;

    assert(stop);

    // command loop
    while (USART0_HasReceivedByte()) {
        if (s_IoBufferIndex < sizeof(s_IoBuffer)) {
            uint8_t c = USART0_FetchReceivedByte() & 127;
            if (c < 0x20) { // space
                if (c == '\n' || c == '\r') {
                    //DEBUG("Got \\r\\n, try process command\n");
                    s_IoBuffer[s_IoBufferIndex] = 0;
                    ProcessCommand(stop);
                    ClearCommandBuffer();
                    ++commands;
                    if (*stop) {
                        DEBUG("STOP\n");
                        break;
                    }
                } else {
                    // ignore all control chars
                }
            } else {
                s_IoBuffer[s_IoBufferIndex++] = (char)c;
            }
        } else {
            DEBUG("I/O buffer full, no command\n");
            ClearCommandBuffer();
        }
    }

    return commands;
}

static
void
ProcessCommandsForSeconds(int seconds) {
    const int targetMode = s_Mode;
    ASSERT_VALID_MODE;
    int commandsProcessed = 0;
    int stop = 0;
    LOG("Mode %s, device will process commands for the next %d seconds\n", s_ModeNames[targetMode], seconds);
    for (int i = 0, end = (1000 * seconds); i < end; ++i) {
        ASSERT_VALID_MODE;
        if (targetMode != s_Mode) {
            LogModeChange(targetMode, s_Mode);
            break;
        }

        commandsProcessed = ProcessCommands(&stop);
        if (commandsProcessed > 0) {
            if (stop) {
                DEBUG("STOP\n");
                break;
            }

            DEBUG("Timer reset to %d seconds\n", seconds);
            i = 0;
        }

         _delay_ms(1);
         if (i % 1000 == 0) {
            USART0_SendByte('.');
         }
    }

    USART0_SendByte('\n');

    // make sure everything has been sent
    USART0_SendFlush();
}

static volatile uint16_t s_SecondsElapsed;
static
void
WatchdogCallback(void* ctx) {
    (void)ctx;
    ++s_SecondsElapsed;
    //s_SecondsToSleep = 3;
    PINB = 1<<PIN5;
}

int
main() {
    const uint8_t mcusr = MCUSR;
    MCUSR = 0;

    cli();
    WDT_off();
    power_all_enable();
    power_adc_disable();
    //power_aca_disable();
    power_timer0_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_twi_disable();

    set_sleep_mode(POWERDOWN_MODE);

    USART0_Init();
    if(mcusr & (1<<PORF )) DEBUG("Power-on reset.\n");
    if(mcusr & (1<<EXTRF)) DEBUG("External reset!\n");
    if(mcusr & (1<<BORF )) DEBUG("Brownout reset!\n");
    if(mcusr & (1<<WDRF )) DEBUG("Watchdog reset!\n");
   // if(mcucsr & (1<<JTRF )) DEBUG(("JTAG reset!\n");

    // output port 1 p. 76 to toggle on-board LED connected to PIN 13
    DDRB |= 1 << DDB5;

//    PINB = 1<<PIN5;
//    _delay_ms(1000);
//    PINB = 1<<PIN5;


    WDT_ChangePrescaler(WDTO_1S);

    //DEBUG("Main sizeof(int): %u, sizeof(long): %u\n", sizeof(int), sizeof(long));
    LOG("Hello there!\nThis is " VERSION_STRING "\n" COPYRIGHT_STRING "\n\n");
    LOG("Loading configuration. This may take a while.\n");
    CONF_Load();
    LOG("Configuration loaded.\n");
    DEBUG("Seconds to sleep %d\n", s_SecondsToSleep);


    ClearCommandBuffer();
//    DEBUG("Newline d=%d,h=%02x\n", '\n', '\n');
//    DEBUG("Carrige return d=%d,h=%02x\n", '\r', '\r');
    ASSERT_VALID_MODE;

    while (1) {
        switch (s_Mode) {
        case Mode_Default:
            LOG("**** DEFAULT MODE ****\n");
            cli();
            DEBUG("Shutting down peripherals\n");
            USART0_Uninit();
            power_spi_disable();
            power_usart0_disable();
BackToSleep:
            WDT_On(0, 1, NULL, WatchdogCallback);
            sleep_enable();
            sleep_bod_disable();
            sei();
            sleep_cpu();
            sleep_disable();
            cli();
            WDT_off();
            if (s_SecondsElapsed < s_SecondsToSleep) {
                goto BackToSleep;
            }

            s_SecondsElapsed = 0;
            power_spi_enable();
            power_usart0_enable();
            USART0_Init();
            ProcessCommandsForSeconds(5);
            break;

        case Mode_Initial:
            ProcessCommandsForSeconds(10);
            if (Mode_Initial == s_Mode) {
                s_Mode = Mode_Default;
                LogModeChange(Mode_Initial, Mode_Default);
            }
            break;

        case Mode_Service:
            LOG("**** SERVICE MODE ****\n");
            while (1) {
                int stop = 0;
                if (Mode_Service != s_Mode) {
                    LogModeChange(Mode_Service, s_Mode);
                    break;
                }

                ProcessCommands(&stop);
                _delay_ms(1);
            }
            break;
        default:
            ASSERT_VALID_MODE;
            break;
        }
    }

    return 0;
}

