#ifndef MISC_H
#define MISC_H

#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)

#define JOIN2(x, y) x ## y
#define JOIN(x, y) JOIN2(x, y)

#ifdef NDEBUG
#   define NOINIT __attribute__ ((section (".noinit")))
#else
#   define NOINIT
#endif

#define ArduinoDeclare() \
    /* Declared weak in Arduino.h to allow user redefinitions. */ \
    int atexit(void (* /*func*/ )()) { return 0; } \
    /* Weak empty variant initialization function. */ \
    /* May be redefined by variant files. */ \
    void initVariant() __attribute__((weak)); \
    void initVariant() { }


#define ArduinoInit() \
    do { \
        init(); \
        initVariant(); \
    } while (0);

#define IsInWindowT(type, currentWindow, windowSize, value) ((type)(((type)(currentWindow))-((type)(value))) < ((type)(windowSize)))
#define IsInWindow8(currentWindow, windowSize, value) IsInWindowT(uint8_t, currentWindow, windowSize, value)
#define IsInWindow16(currentWindow, windowSize, value) IsInWindowT(uint16_t, currentWindow, windowSize, value)
#define IsInWindow32(currentWindow, windowSize, value) IsInWindowT(uint32_t, currentWindow, windowSize, value)

#ifndef _countof
#   define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

#endif /* MISC_H */
