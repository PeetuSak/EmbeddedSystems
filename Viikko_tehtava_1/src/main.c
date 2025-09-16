#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <inttypes.h>
#include <zephyr/sys/util.h>

// button configurations
#define BUTTON_0 DT_ALIAS(sw0)

// Led and button pin configurations
static const struct gpio_dt_spec red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;

// led thread initialization
#define STACKSIZE 1024
#define PRIORITY 5

void red_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);

K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);

// Global state variables
volatile int led_state = 0;
volatile int prev_led_state = 0;

// Function prototypes
int init_leds(void);

// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (led_state != 4){
        prev_led_state = led_state;
        led_state = 4;
        printk("Entering pause state. Paused from state: %d\n", prev_led_state);
    } else {
        led_state = prev_led_state;
        printk("Exiting pause state. Resuming to state: %d\n", led_state);
    }
}

// Main program
int main(void)
{
    int ret;
    ret = init_leds();
    if(ret){
        return ret;
    }

    if (!gpio_is_ready_dt(&button_0)) {
        printk("Error: button 0 is not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
    if (ret != 0) {
        printk("Error: failed to configure button pin\n");
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        printk("Error: failed to configure interrupt on pin\n");
        return -1;
    }

    gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
    gpio_add_callback(button_0.port, &button_0_data);
    
    printk("Program started. Button is ready.\n");
    return 0;
}

// Initialize leds
int init_leds() {
    int ret;

    // Red LED
    if (!gpio_is_ready_dt(&red)) {
        printk("Error: Red LED not ready\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Red Led configure failed\n");
        return ret;
    }

    // Green LED
    if (!gpio_is_ready_dt(&green)) {
        printk("Error: Green LED not ready\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Green Led configure failed\n");
        return ret;
    }

 
    gpio_pin_set_dt(&red,0);
    gpio_pin_set_dt(&green,0);

    printk("LEDs initialized successfully.\n");
    return 0;
}

// Task for red led
void red_led_task(void *, void *, void*) {
    while (true) {
        if (led_state == 0) {
            printk("Red ON\n");
            gpio_pin_set_dt(&red, 1);
            k_msleep(1000);
            gpio_pin_set_dt(&red, 0);
            printk("Red OFF\n");

            k_msleep(10);
            if (led_state == 0) {
                led_state = 1;
            }
        }
        k_msleep(100);
    }
}

// Task for yellow led (red + green)
void yellow_led_task(void *, void *, void*) {
    while (true) {
        if (led_state == 1) {
            printk("Yellow ON\n");
            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);
            k_msleep(1000);
            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);
            printk("Yellow OFF\n");

            k_msleep(10);
            if (led_state == 1) { 
                led_state = 2;
            }
        }
        k_msleep(100);
    }
}

// Task for green led
void green_led_task(void *, void *, void*) {
    while (true) {
        if (led_state == 2) {
            printk("Green ON\n");
            gpio_pin_set_dt(&green, 1);
            k_msleep(1000);
            gpio_pin_set_dt(&green, 0);
            printk("Green OFF\n");
            
            k_msleep(10);
            if (led_state == 2) { 
                led_state = 0;
            }
        }
        k_msleep(100);
    }
}
