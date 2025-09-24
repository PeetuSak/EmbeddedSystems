#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <stdlib.h>

// Thread initializations
#define STACKSIZE 500
#define PRIORITY 5

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);

// FIFO dispatcher data type
struct data_t {
    void *fifo_reserved;
    char msg[20];
};

//UART alustus 

int init_uart(void) {
    if (!device_is_ready(uart_dev)) {
        return 1;
    }
    return 0;
}

// ledi määrittelyt
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios); // punainen
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios); // vihreä

static void leds_init(void) {
    gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
}

static void leds_off(void) {
    gpio_pin_set_dt(&led0, 0);
    gpio_pin_set_dt(&led1, 0);
}

int main(void)
{
    int ret = init_uart();
    if (ret != 0) {
        printk("UART initialization failed!\n");
        return ret;
    }

    leds_init();

    printk("Program started. Waiting for UART input...\n");
    return 0;
}

static void uart_task(void *unused1, void *unused2, void *unused3)
{
    char rc = 0;

    while (true) {
        if (uart_poll_in(uart_dev, &rc) == 0) {
            printk("UART-task vastaanotti: %c\n", rc);  // DEBUG

            if (rc == 'R' || rc == 'Y' || rc == 'G') {
                struct data_t *buf = k_malloc(sizeof(struct data_t));
                if (buf != NULL) {
                    buf->fifo_reserved = NULL;
                    buf->msg[0] = rc;
                    buf->msg[1] = '\0';
                    k_fifo_put(&dispatcher_fifo, buf);
                }
            }
        }
        k_msleep(10);
    }
}

static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
    while (true) {
        struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
        char c = rec_item->msg[0];
        printk("Dispatcher sai: %c\n", c);  // DEBUG
        k_free(rec_item);

        leds_off();

        switch (c) {
        case 'R':
            printk("Sytytetään punainen LED\n");
            gpio_pin_set_dt(&led0, 1);
            break;
        case 'G':
            printk("Sytytetään vihreä LED\n");
            gpio_pin_set_dt(&led1, 1);
            break;
        case 'Y':
            printk("Sytytetään keltainen (punainen + vihreä)\n");
            gpio_pin_set_dt(&led0, 1);
            gpio_pin_set_dt(&led1, 1);
            break;
        default:
            break;
        }

        k_msleep(1000); // LED palaa 1s
        leds_off();
    }
}

K_THREAD_DEFINE(dis_thread, STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_thread, STACKSIZE, uart_task, NULL, NULL, NULL, PRIORITY, 0, 0);
