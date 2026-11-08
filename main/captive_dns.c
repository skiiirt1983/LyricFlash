#include "captive_dns.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

/* Captive-portal DNS for the setup network.
 *
 * While LyricFlash runs its own Wi-Fi network, every name lookup is answered
 * with the device's address. A phone or laptop that joins then finds its
 * "am I online?" check (Apple, Android, Windows) landing on the LyricFlash web
 * server, which redirects it to the setup page, so the OS opens that page by
 * itself. Only A (IPv4) questions get an answer; anything else gets an empty
 * NOERROR reply so clients fall back to IPv4. Never used on home Wi-Fi. */

static const char *TAG = "LFCaptiveDNS";
static TaskHandle_t dns_task;
static uint32_t answer_ip; /* network byte order */

#define DNS_PORT 53
#define DNS_MAX LF_CAPTIVE_DNS_MAX

static void dns_worker(void *arg)
{
    (void)arg;
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGW(TAG, "socket failed");
        dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_port = htons(DNS_PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&local, sizeof local) != 0) {
        ESP_LOGW(TAG, "bind to port 53 failed");
        close(sock);
        dns_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Captive portal DNS active");
    static uint8_t buf[DNS_MAX];
    for (;;) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof from;
        const int n = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &from_len);
        if (n <= 0) continue;
        const size_t reply = lf_captive_dns_reply(buf, (size_t)n, answer_ip);
        if (reply) sendto(sock, buf, reply, 0, (struct sockaddr *)&from, from_len);
    }
}

bool lf_captive_dns_start(uint32_t ip_network_order)
{
    if (dns_task) return true;
    answer_ip = ip_network_order;
    return xTaskCreate(dns_worker, "lf_dns", 3072, NULL, 4, &dns_task) == pdPASS;
}
