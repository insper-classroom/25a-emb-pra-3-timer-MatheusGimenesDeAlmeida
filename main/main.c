 #include "hardware/rtc.h"
 #include "pico/util/datetime.h"
 #include "pico/stdlib.h"
 #include "hardware/gpio.h"
 #include "hardware/timer.h"
 #include <stdio.h>
 #include <string.h>
 
 #define TRIGGER_PIN 5
 #define ECHO_PIN    15


 #define TIMEOUT_MS  50
 
 typedef struct {
     alarm_id_t alarm;
     absolute_time_t inicio;
     absolute_time_t fim;
     bool aguarda_pulso;
     int pulso_fall;
 } sensor_data_t;
 
 volatile static sensor_data_t sensor_data = {0};
 
 int callback_timeout(alarm_id_t id, void *user_data) {
     if (sensor_data.aguarda_pulso) {
         datetime_t agora;
         rtc_get_datetime(&agora);
        
         printf("%02d:%02d:%02d - Erro: pulso não recebido\n", agora.hour, agora.min, agora.sec);
         sensor_data.aguarda_pulso = false;
     }
     return 0; 
 }
 
 void callback_echo(uint gpio, uint32_t eventos) {
     if (eventos & GPIO_IRQ_EDGE_RISE) {
         sensor_data.inicio = get_absolute_time();
         if (sensor_data.aguarda_pulso) {
             cancel_alarm(sensor_data.alarm);
             sensor_data.aguarda_pulso = false;
         }
     } else if (eventos & GPIO_IRQ_EDGE_FALL) {
         sensor_data.fim = get_absolute_time();
         sensor_data.pulso_fall = 1;
     }
 }
 
 int main() {
     stdio_init_all();
 
     datetime_t tempo_inicial = {
         .year  = 2025,
         .month = 3,
         .day   = 18,
         .dotw  = 0,   
         .hour  = 0,
         .min   = 0,
         .sec   = 0
     };
     rtc_init();
     rtc_set_datetime(&tempo_inicial);
 
     gpio_init(TRIGGER_PIN);
     gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
 
     gpio_init(ECHO_PIN);
     gpio_set_dir(ECHO_PIN, GPIO_IN);
     gpio_set_irq_enabled_with_callback(ECHO_PIN,
                                        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                        true,
                                        callback_echo);
 
     bool medindo = false;
 
     while (true) {
         int ch = getchar_timeout_us(500);
         if (ch == 'm') {
             medindo = !medindo;
             if (medindo) {
                 printf("Medições iniciadas\n");
             } else {
                 printf("Medições pausadas\n");
             }
         }
 
         if (medindo) {
             gpio_put(TRIGGER_PIN, 1);
             sleep_us(10);
             gpio_put(TRIGGER_PIN, 0);
 
             sensor_data.aguarda_pulso = true;
             sensor_data.alarm = add_alarm_in_ms(TIMEOUT_MS, callback_timeout, NULL, false);
 
             if (sensor_data.pulso_fall == 1) {
                 sensor_data.pulso_fall = 0;
                 int64_t delta_us = absolute_time_diff_us(sensor_data.inicio, sensor_data.fim);
                 double distancia = (delta_us * 0.0343) / 2.0;  
 
                 datetime_t agora;
                 rtc_get_datetime(&agora);
                 char buffer_data[256];
                 datetime_to_str(buffer_data, sizeof(buffer_data), &agora);
                 printf("%s - Distância: %.2f cm\n", buffer_data, distancia);
             }
         }
 
         sleep_ms(1000);
     }
 }
 