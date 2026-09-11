/* netlog.c — diffuse les logs ESP-IDF vers un client TCP (port 2323).
 * Se connecter depuis le PC/tel : nc 192.168.20.92 2323  (ou Termius en Telnet)
 * Leger : ne coute presque rien tant qu'aucun client n'est connecte. */
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static int s_client = -1;                 /* socket du client connecte (-1 = aucun) */
static vprintf_like_t s_orig = NULL;      /* fonction de log d'origine (serie) */
static bool s_enabled = true;             /* actif par defaut ; coupe via netlog_set_enabled (UI, plus tard) */

static int netlog_vprintf(const char *fmt, va_list ap) {
    if (s_orig) { va_list c; va_copy(c, ap); s_orig(fmt, c); va_end(c); }  /* garde le serie */
    if (!s_enabled || s_client < 0) return 0;   /* desactive OU aucun client : rien de plus */
    char buf[256];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) {
        if (n > (int)sizeof(buf)) n = sizeof(buf);
        if (send(s_client, buf, n, MSG_DONTWAIT) < 0) { close(s_client); s_client = -1; }
    }
    return 0;
}

static void netlog_task(void *arg) {
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) vTaskDelete(NULL);
    int yes = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(2323), .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(srv, (struct sockaddr *)&a, sizeof(a)) < 0 || listen(srv, 1) < 0) { close(srv); vTaskDelete(NULL); }
    s_orig = esp_log_set_vprintf(netlog_vprintf);
    while (1) {
        struct sockaddr_in ca; socklen_t cl = sizeof(ca);
        int c = accept(srv, (struct sockaddr *)&ca, &cl);
        if (c < 0) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        if (!s_enabled) { close(c); continue; }   /* desactive : on refuse le client */
        if (s_client >= 0) close(s_client);
        s_client = c;
    }
}

void netlog_set_enabled(bool on) {
    s_enabled = on;
    if (!on && s_client >= 0) { close(s_client); s_client = -1; }   /* coupe le client en cours */
}

void netlog_start(void) {
    xTaskCreate(netlog_task, "netlog", 3072, NULL, 3, NULL);
}