#include "captive_dns.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Builds a standard query for name with the given type into buf. */
static size_t query(uint8_t *buf, const char *name, uint16_t type)
{
    memset(buf, 0, LF_CAPTIVE_DNS_MAX);
    buf[0] = 0x12; buf[1] = 0x34;  /* id */
    buf[2] = 0x01; buf[3] = 0x00;  /* standard query, recursion desired */
    buf[5] = 1;                    /* one question */
    size_t pos = 12;
    const char *label = name;
    while (*label) {
        const char *dot = strchr(label, '.');
        const size_t n = dot ? (size_t)(dot - label) : strlen(label);
        buf[pos++] = (uint8_t)n;
        memcpy(buf + pos, label, n);
        pos += n;
        label += n + (dot ? 1 : 0);
    }
    buf[pos++] = 0;
    buf[pos++] = (uint8_t)(type >> 8); buf[pos++] = (uint8_t)type;
    buf[pos++] = 0; buf[pos++] = 1; /* class IN */
    return pos;
}

int main(void)
{
    uint8_t buf[LF_CAPTIVE_DNS_MAX];
    const uint8_t ip[4] = {192, 168, 4, 1};
    uint32_t ip_n;
    memcpy(&ip_n, ip, 4);

    /* Android's connectivity check resolves to the device. */
    size_t qlen = query(buf, "connectivitycheck.gstatic.com", 1);
    size_t r = lf_captive_dns_reply(buf, qlen, ip_n);
    assert(r == qlen + 16);
    assert(buf[0] == 0x12 && buf[1] == 0x34);  /* same id */
    assert(buf[2] & 0x80);                      /* is a response */
    assert(buf[2] & 0x01);                      /* RD echoed */
    assert((buf[3] & 0x0F) == 0);               /* NOERROR */
    assert(buf[5] == 1 && buf[7] == 1);         /* 1 question, 1 answer */
    assert(buf[qlen] == 0xC0 && buf[qlen + 1] == 0x0C);
    assert(buf[qlen + 3] == 1 && buf[qlen + 5] == 1); /* A, IN */
    assert(memcmp(buf + qlen + 12, ip, 4) == 0);

    /* Apple's check, a longer name. */
    qlen = query(buf, "captive.apple.com", 1);
    assert(lf_captive_dns_reply(buf, qlen, ip_n) == qlen + 16);

    /* IPv6 lookups get an empty answer so clients use IPv4. */
    qlen = query(buf, "www.msftconnecttest.com", 28);
    r = lf_captive_dns_reply(buf, qlen, ip_n);
    assert(r == qlen);
    assert(buf[2] & 0x80);
    assert(buf[7] == 0);

    /* Replies, empty and truncated packets are ignored. */
    qlen = query(buf, "example.com", 1);
    buf[2] |= 0x80;
    assert(lf_captive_dns_reply(buf, qlen, ip_n) == 0);
    assert(lf_captive_dns_reply(buf, 5, ip_n) == 0);
    qlen = query(buf, "example.com", 1);
    assert(lf_captive_dns_reply(buf, qlen - 3, ip_n) == 0);
    buf[12] = 0xC0; /* compressed name in a query: ignore */
    assert(lf_captive_dns_reply(buf, qlen, ip_n) == 0);

    puts("PASS: captive portal DNS answers A lookups with the device address and ignores bad packets");
    return 0;
}
