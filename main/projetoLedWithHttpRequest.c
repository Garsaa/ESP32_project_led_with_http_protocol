/* 
    PROJECT OWNER: GABRIEL GARCIA
    INSPIRED BY ESPRESSIF CODE OF HTTPS_REQUEST_EXAMPLE!

*/
#include <string.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "sdkconfig.h"

/* Constantes que não são configuradas via menuconfig*/
#define WEB_SERVER "timeapi.io"
#define WEB_PORT "80"
#define WEB_PATH "/api/Time/current/zone?timeZone=America/Sao_Paulo"

//Tag para exibição no console
static const char *TAG = "HTTPS_GET:";

//Criação do template usado para fazer a requisição get para o servidor da API
static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";


//função que seta um pino como saída
void Pin_As_Output(uint8_t pinNum){
    gpio_set_direction(pinNum, GPIO_MODE_OUTPUT);
}

//função que dita o valor do sinal que passa em um pino
void Set_Pin_Level(uint8_t pinNum, bool level){
    gpio_set_level(pinNum, level);
}



//função que faz o get, de 10 em 10 segundos para saber o horário! onde as validações necessárias para noção de funcionamento-
//-do código acontecem!
static void http_get_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    unsigned int a=0;
    char recv_buf[64], importantLine[64], *pointToString[7];

    while(1){
        //Requisição GET acontecendo de fato:
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* lê a resposta da requisição GET e exibe na tela*/
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
           
            for(int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
            a++;
            if(a == 2){
                strcpy(importantLine, recv_buf);
                
            }
        } while(r > 0);

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);

        

        //recolhendo a hora e o minuto do tempo real
        pointToString[0] = strtok(importantLine, " "); 

        for(int i=1;i<6;i++){
            pointToString[i] = strtok(NULL, " "); 
        }

        ESP_LOGI(TAG, "printing the time: %s", pointToString[5]);

        strcpy(importantLine, pointToString[5]);

        


        pointToString[0] = strtok(importantLine, ":");
        pointToString[1] = strtok(NULL, ":");

        ESP_LOGI(TAG, "printing the hour: %s and the MINUT: %s ",pointToString[0], pointToString[1]);

       Pin_As_Output(25);

        //controle dos pinos que ligam ou desligam o led
        if(atoi(pointToString[1]) % 2 != 0){
            Set_Pin_Level(25, 1);
            
        }else{
            Set_Pin_Level(25, 0);
        }
        //contador para que ele o faça de 10 em 10 segundos
        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        
        
    }
}






//Função que controla o fluxo principal do programa
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init() );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /*
         Funções para conexão do wi-fi, onde só prosseguem com a requisição para o servidor WEB, caso a conexão tenha sido um sucesso
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);
}