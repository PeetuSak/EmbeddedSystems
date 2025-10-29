

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <string.h>
#include <stdlib.h>


//parser
#define TIME_LEN_ERROR      -1
#define TIME_ARRAY_ERROR    -2
#define TIME_VALUE_ERROR    -1

// time format: HHMMSS
int time_parse(char *time) {
    if(time == NULL) return TIME_ARRAY_ERROR;
    if(strlen(time) != 6) return TIME_LEN_ERROR;

    int values[3];
    values[2] = atoi(time+4); time[4]=0;
    values[1] = atoi(time+2); time[2]=0;
    values[0] = atoi(time);

    if(values[0]<0 || values[0]>23 || values[1]<0 || values[1]>59 || values[2]<0 || values[2]>59)
        return TIME_VALUE_ERROR;

    return values[1]*60 + values[2]; 
}


// Thread initializations
#define STACKSIZE 500
#define PRIORITY 5

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// LED alustus
static const struct gpio_dt_spec red   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

// dispatcher and FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);
K_FIFO_DEFINE(debug_fifo);

// dispatcher FIFO
struct data_t {
    void *fifo_reserved;
    char msg[20];
};

//debug FIFO
struct debug_msg_t {
    void *fifo_reserved;
    char text[64];
    uint64_t time;
};

// Mutex  
K_MUTEX_DEFINE(red_mutex);
K_MUTEX_DEFINE(green_mutex);
K_MUTEX_DEFINE(yellow_mutex);

// condition variables
K_CONDVAR_DEFINE(red_cv);
K_CONDVAR_DEFINE(green_cv);
K_CONDVAR_DEFINE(yellow_cv);

volatile bool debug_enabled = true;

uint64_t red_time = 0;
uint64_t green_time = 0;
uint64_t yellow_time = 0;

void timer_led_handler(struct k_timer *timer_id)
{
    printk("Timer handler kutsuttiin!\n");
    k_mutex_lock(&red_mutex, K_FOREVER);
    k_condvar_signal(&red_cv);
    k_mutex_unlock(&red_mutex);
}
K_TIMER_DEFINE(timer_led, timer_led_handler, NULL);

// LED initialization
void init_leds(void *a, void *b, void *c)
{
    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    k_msleep(10);
}

//uart alustus
int init_uart(void) {
    if (!device_is_ready(uart_dev)) {
        return 1;
    }
    return 0;
}


static void uart_task(void *unused1, void *unused2, void *unused3)
{
    printk("UART-taski käynnistyi!\n");
    char rc=0;
    char uart_msg[20];
    memset(uart_msg,0,sizeof(uart_msg));
    int uart_msg_cnt = 0;

     while (true) {
         if (uart_poll_in(uart_dev,&rc) == 0) {
            if (rc == 'X') {
                if (uart_msg_cnt == 6) {
                    uart_msg[uart_msg_cnt] = '\0'; 
                    int result = time_parse(uart_msg);
                    char outbuf[16];
                    int len = snprintf(outbuf, sizeof(outbuf), "%dX", result);
                    for (int i = 0; i < len; i++) {
                        uart_poll_out(uart_dev, outbuf[i]);
                    }
                }
                uart_msg_cnt = 0;
                memset(uart_msg, 0, sizeof(uart_msg));
            } else {
                if (uart_msg_cnt < 6) { 
                uart_msg[uart_msg_cnt++] = rc;} 
            }
        k_msleep(1);
        }
    }
}
static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
    while (true) {
        struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
        char sequence[20];
        memcpy(sequence, rec_item->msg, sizeof(sequence));
        k_free(rec_item);

 // parser
        bool is_time_string = true;
        for (int i = 0; i < strlen(sequence); i++) {
            if (sequence[i] < '0' || sequence[i] > '9') {
                is_time_string = false;
                break;
            }
        }

        if (is_time_string) {
            int delay_sec = time_parse(sequence);

            if (delay_sec >= 0) {
                printk("Ajastin asetetaan %d sekunnin päähän\n", delay_sec);

                // Käynnistä ajastin, joka sytyttää punaisen LEDin
                k_timer_start(&timer_led, K_SECONDS(delay_sec), K_NO_WAIT);
            } else {
                printk("Virhe aikamerkkijonossa! (code: %d)\n", delay_sec);
            }

            continue; 
        }
    // parser loppuu

        for (int i=0; i<strlen(sequence); i++) {
            char c = sequence[i];

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
            }else if (c == 'D'){
                debug_enabled = !debug_enabled;
                printk("Debug mode: %s\n", debug_enabled ? "ON" : "OFF");
            }
            // Odotetaan että edellinen LED ehtii sammua
            k_msleep(1100);
        }
        uint64_t total_time = (red_time + green_time + yellow_time);
        //printk("Total task duration: %llu \n", total_ns);
        struct debug_msg_t *buf = k_malloc(sizeof(struct debug_msg_t));
		if (buf == NULL) {
			return;
		    }
        buf->time = total_time;
        snprintf(buf->text, sizeof(buf->text), "total task duration: %llu us", buf->time);
        k_fifo_put(&debug_fifo, buf);

    }
}

void red_task(void *a, void *b, void *c)
{
    while (true) {
        timing_start();
        timing_t red_start_time = timing_counter_get();

        k_mutex_lock(&red_mutex, K_FOREVER);
        k_condvar_wait(&red_cv, &red_mutex, K_FOREVER);
        k_mutex_unlock(&red_mutex);

        gpio_pin_set_dt(&red, 1);
        k_msleep(1000);
        gpio_pin_set_dt(&red, 0);

        timing_stop();
        timing_t red_end_time = timing_counter_get();
        red_time = timing_cycles_to_ns(timing_cycles_get(&red_start_time, &red_end_time))/1000;
        // printk("Red task duration: %llu \n", red_time);

        struct debug_msg_t *buf = k_malloc(sizeof(struct debug_msg_t));
		if (buf == NULL) {
			return;
		    }
        buf->time = red_time;
        snprintf(buf->text, sizeof(buf->text), "Red task duration: %llu us", buf->time);
        k_fifo_put(&debug_fifo, buf);

        
    }
}

void green_task(void *a, void *b, void *c)
{
    while (true) {
        timing_start();
        timing_t green_start_time = timing_counter_get();

        k_mutex_lock(&green_mutex, K_FOREVER);
        k_condvar_wait(&green_cv, &green_mutex, K_FOREVER);
        k_mutex_unlock(&green_mutex);

        gpio_pin_set_dt(&green, 1);
        k_msleep(1000);
        gpio_pin_set_dt(&green, 0);

        timing_stop();
        timing_t green_end_time = timing_counter_get();
        green_time = timing_cycles_to_ns(timing_cycles_get(&green_start_time, &green_end_time))/1000;

        struct debug_msg_t *buf = k_malloc(sizeof(struct debug_msg_t));
		if (buf == NULL) {
			return;

        }
    buf->time = green_time;
    snprintf(buf->text, sizeof(buf->text), "Green task duration: %llu us", buf->time);
    k_fifo_put(&debug_fifo, buf);

    }
}


void yellow_task(void *a, void *b, void *c)
{
    while (true) {
        timing_start();
        timing_t yellow_start_time = timing_counter_get();

        k_mutex_lock(&yellow_mutex, K_FOREVER);
        k_condvar_wait(&yellow_cv, &yellow_mutex, K_FOREVER);
        k_mutex_unlock(&yellow_mutex);

        gpio_pin_set_dt(&red, 1);
        gpio_pin_set_dt(&green, 1);
        k_msleep(1000);
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);

        timing_stop();
        timing_t yellow_end_time = timing_counter_get();
        yellow_time = timing_cycles_to_ns(timing_cycles_get(&yellow_start_time, &yellow_end_time))/1000;

        struct debug_msg_t *buf = k_malloc(sizeof(struct debug_msg_t));
		if (buf == NULL) {
		return;
        }
    buf->time = yellow_time;
    snprintf(buf->text, sizeof(buf->text), "Yellow task duration: %llu us", buf->time);
    k_fifo_put(&debug_fifo, buf);

    }
}   

void debug_task(void *a, void *b, void *c)
{
    struct debug_msg_t *received;

    while (true) {
        received = k_fifo_get(&debug_fifo, K_FOREVER);

        if(debug_enabled) {
        printk("time received: %s\n", received->text);//debug päällä vain jos debug_enabled on true
        }

        k_free(received);
        k_yield();
    }
}

int main(void)
{
    timing_init();
    
    if (init_uart() != 0) {
        return -1;
    }
    return 0;
}


// Threads
K_THREAD_DEFINE(init_leds_thread, STACKSIZE, init_leds, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_thread, STACKSIZE, uart_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dis_thread, STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(red_thread, STACKSIZE, red_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(debug_thread, STACKSIZE, debug_task, NULL, NULL, NULL, PRIORITY+1, 0, 0); // debug taskille suurempi prioriteetti

