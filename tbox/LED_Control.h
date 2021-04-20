#ifndef __LED_CONTROL_H__
#define __LED_CONTROL_H__

// 通过内核控制 led

#define LED_POWER_ON() do { \
    system("echo 1 > /sys/class/leds/ledsw1\\:green/brightness"); \
} while(0)

#define LED_POWER_OFF() do { \
    system("echo 0 > /sys/class/leds/ledsw1\\:green/brightness"); \
} while(0)

#define LED_STATUS_BLINK_FAST() do { \
    system("echo timer > /sys/class/leds/ledsw2\\:red/trigger"); \
    system("echo 100 > /sys/class/leds/ledsw2\\:red/delay_on"); \
    system("echo 200 > /sys/class/leds/ledsw2\\:red/delay_off"); \
} while(0)

#define LED_STATUS_BLINK_SLOW() do { \
    system("echo timer > /sys/class/leds/ledsw2\\:red/trigger"); \
    system("echo 1000 > /sys/class/leds/ledsw2\\:red/delay_on"); \
    system("echo 1000 > /sys/class/leds/ledsw2\\:red/delay_off"); \
} while(0)

#define LED_STATUS_BLINK_NORMAL() do { \
    system("echo timer > /sys/class/leds/ledsw2\\:red/trigger"); \
} while(0)

#define LED_STATUS_OFF() do { \
    system("echo 0 > /sys/class/leds/ledsw2\\:red/brightness"); \
} while(0)

#define LED_STATUS_ON() do { \
    system("echo 1 > /sys/class/leds/ledsw2\\:red/brightness"); \
} while(0)

#define DEVICE_STATUS_START()          LED_STATUS_BLINK_SLOW()
#define DEVICE_STATUS_ERROR()          LED_STATUS_BLINK_FAST()
#define DEVICE_STATUS_NORMAL()         LED_STATUS_BLINK_NORMAL()
#define DEVICE_STATUS_OFF()            LED_STATUS_OFF()
#define DEVICE_KL15_ERROR()            LED_STATUS_ON()

#endif