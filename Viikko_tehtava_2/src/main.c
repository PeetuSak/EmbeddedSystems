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

// LED alustus
static const struct gpio_dt_spec red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);

// FIFO dispatcher data type
struct data_t {
    void *fifo_reserved;
    char msg[20];
};

// Condition variables + mutexit valoille
K_MUTEX_DEFINE(red_mutex);
K_MUTEX_DEFINE(green_mutex);
K_MUTEX_DEFINE(yellow_mutex);

K_CONDVAR_DEFINE(red_cv);
K_CONDVAR_DEFINE(green_cv);
K_CONDVAR_DEFINE(yellow_cv);

//uart alustus

int init_uart(void) {
    if (!device_is_ready(uart_dev)) {
        return 1;
    }
    return 0;
}


static void uart_task(void *unused1, void *unused2, void *unused3)
{
    char rc=0;
    char uart_msg[20];
    memset(uart_msg,0,20);
    int uart_msg_cnt = 0;

    while (true) {
        if (uart_poll_in(uart_dev,&rc) == 0) {
            printk("UART key: %c\n", rc);  // Debug

            if (rc != '\r') {
                uart_msg[uart_msg_cnt++] = rc;
            } else {
                struct data_t *buf = k_malloc(sizeof(struct data_t));
                if (buf == NULL) return;
                snprintf(buf->msg, sizeof(buf->msg), "%s", uart_msg);
                k_fifo_put(&dispatcher_fifo, buf);

                uart_msg_cnt = 0;
                memset(uart_msg,0,20);
            }
        }
        k_msleep(10);
    }
}

static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
    while (true) {
        struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
        char sequence[20];
        memcpy(sequence, rec_item->msg, sizeof(sequence));
        k_free(rec_item);

        printk("Dispatcher got: %s\n", sequence);

        for (int i=0; i<strlen(sequence); i++) {
            char c = sequence[i];
            printk("Dispatcher: char %c\n", c);

            if (c == 'R') {
                k_mutex_lock(&red_mutex, K_FOREVER);
                k_condvar_signal(&red_cv);
                k_mutex_unlock(&red_mutex);
            } else if (c == 'G') {
                k_mutex_lock(&green_mutex, K_FOREVER);
                k_condvar_signal(&green_cv);
                k_mutex_unlock(&green_mutex);
            } else if (c == 'Y') {
                k_mutex_lock(&yellow_mutex, K_FOREVER);
                k_condvar_signal(&yellow_cv);
                k_mutex_unlock(&yellow_mutex);
            }
            // Odotetaan että edellinen LED ehtii sammua
            k_msleep(1100);
        }
    }
}

void red_task(void *a, void *b, void *c)
{
    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);

    while (1) {
        k_mutex_lock(&red_mutex, K_FOREVER);
        k_condvar_wait(&red_cv, &red_mutex, K_FOREVER);
        k_mutex_unlock(&red_mutex);
        
        printk("Red ON\n");
        gpio_pin_set_dt(&red, 1);
        k_msleep(1000);
        gpio_pin_set_dt(&red, 0);
        printk("Red OFF\n");
    }
}

void green_task(void *a, void *b, void *c)
{
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);

    while (1) {
        k_mutex_lock(&green_mutex, K_FOREVER);
        k_condvar_wait(&green_cv, &green_mutex, K_FOREVER);
        k_mutex_unlock(&green_mutex);

        printk("Green ON\n");
        gpio_pin_set_dt(&green, 1);
        k_msleep(1000);
        gpio_pin_set_dt(&green, 0);
        printk("Green OFF\n");
    }
}


void yellow_task(void *a, void *b, void *c)
{
    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);

    while (1) {
        k_mutex_lock(&yellow_mutex, K_FOREVER);
        k_condvar_wait(&yellow_cv, &yellow_mutex, K_FOREVER);
        k_mutex_unlock(&yellow_mutex);

        printk("Yellow ON\n");
        gpio_pin_set_dt(&red, 1);
        gpio_pin_set_dt(&green, 1);
        k_msleep(1000);
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);
        printk("Yellow OFF\n");
    }
}

int main(void)
{
    if (init_uart() != 0) {
        printk("UART init failed!\n");
        return -1;
    }
    printk("Ohjelma käynnistetty. \n");
    return 0;
}

// Threads
K_THREAD_DEFINE(uart_thread, STACKSIZE, uart_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dis_thread, STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(red_thread, STACKSIZE, red_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_task, NULL, NULL, NULL, PRIORITY, 0, 0);
