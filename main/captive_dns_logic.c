#include "captive_dns.h"
#include <string.h>

/* Turns a DNS query (in place) into a reply that resolves an IPv4 (A)
   question to answer_ip; other question types get an empty NOERROR reply.
   Returns the reply length, or 0 to ignore the packet. Pure logic: host
   tested in tests/test_captive_dns.c. */
size_t lf_captive_dns_reply(uint8_t *buf, size_t len, uint32_t answer_ip)
{
    if (len < 12) return 0;
    const uint16_t flags = (uint16_t)((buf[2] << 8) | buf[3]);
    const uint16_t qdcount = (uint16_t)((buf[4] << 8) | buf[5]);
    if ((flags & 0x8000) || qdcount < 1) return 0; /* not a query */
    /* Walk the first question's name (queries never use compression). */
    size_t pos = 12;
    while (pos < len && buf[pos] != 0) {
        if (buf[pos] & 0xC0) return 0;
        pos += (size_t)buf[pos] + 1;
    }
    pos += 1; /* terminating zero */
    if (pos + 4 > len) return 0;
    const uint16_t qtype = (uint16_t)((buf[pos] << 8) | buf[pos + 1]);
    const uint16_t qclass = (uint16_t)((buf[pos + 2] << 8) | buf[pos + 3]);
    pos += 4; /* reply carries only the first question */
    const bool answer = qtype == 1 && qclass == 1; /* A, IN */
    buf[2] = 0x84 | (buf[2] & 0x01); /* response, authoritative, keep RD */
    buf[3] = 0x00;                   /* NOERROR */
    buf[4] = 0; buf[5] = 1;          /* one question */
    buf[6] = 0; buf[7] = answer ? 1 : 0;
    buf[8] = buf[9] = buf[10] = buf[11] = 0;
    if (!answer) return pos;
    if (pos + 16 > LF_CAPTIVE_DNS_MAX) return 0;
    const uint8_t rr[12] = {
        0xC0, 0x0C,             /* name: pointer to the question */
        0x00, 0x01, 0x00, 0x01, /* type A, class IN */
        0x00, 0x00, 0x00, 0x3C, /* TTL 60 s */
        0x00, 0x04              /* 4-byte address */
    };
    memcpy(buf + pos, rr, sizeof rr);
    memcpy(buf + pos + sizeof rr, &answer_ip, 4);
    return pos + 16;
}

