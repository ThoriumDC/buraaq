/* std.led host runtime — prints LED transitions; Sleep for wait.
 * Board packs (pico_w) provide the same symbols with real GPIO / CYW43. */
#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_ms(int ms) {
    if (ms > 0) {
        Sleep((DWORD)ms);
    }
}
#else
#include <errno.h>
#include <time.h>
static void sleep_ms(int ms) {
    if (ms > 0) {
        struct timespec ts;
        ts.tv_sec = (time_t)(ms / 1000);
        ts.tv_nsec = (long)((ms % 1000) * 1000000L);
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
        }
    }
}
#endif

static int led_state = 0;

void buraaq_led_on(void) {
    led_state = 1;
    fputs("LED on\n", stdout);
    fflush(stdout);
}

void buraaq_led_off(void) {
    led_state = 0;
    fputs("LED off\n", stdout);
    fflush(stdout);
}

void buraaq_led_toggle(void) {
    if (led_state) {
        buraaq_led_off();
    } else {
        buraaq_led_on();
    }
}

void buraaq_led_wait_ms(int ms) {
    sleep_ms(ms);
}
