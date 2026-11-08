#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LF_CAPTIVE_DNS_MAX 512

/* Start the setup-network DNS responder: every IPv4 lookup resolves to
   ip_network_order (the device's setup address). Safe to call twice. */
bool lf_captive_dns_start(uint32_t ip_network_order);
/* Build the reply for one query in buf (capacity LF_CAPTIVE_DNS_MAX). */
size_t lf_captive_dns_reply(uint8_t *buf, size_t len, uint32_t answer_ip);
/* Diagnostics: is the responder running, lookups answered, portal redirects. */
typedef struct { bool running; uint32_t queries; uint32_t answered; uint32_t redirects; } lf_captive_stats_t;
void lf_captive_stats(lf_captive_stats_t *out);
void lf_captive_note_redirect(void);
