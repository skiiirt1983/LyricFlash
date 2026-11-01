#include "lyrics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static lf_lyrics_t lyrics; // Avoid large stack on embedded-class host tools.
    assert(lf_parse_lrc(NULL, &lyrics) == 0);
    const char *lrc = "[ar:Someone]\\n"
                      "[00:12.50][00:14.02]First line\\n"
                      "[00:03.123]Earlier line\\r\\n"
                      "[00:15.2]Third line\\n";
    // Test actual line breaks rather than escaped literal characters.
    char b[256]; size_t k = 0;
    for (size_t i=0; lrc[i] && k < sizeof(b)-1; i++) {
        if (lrc[i] == '\\' && lrc[i+1] == 'n') { b[k++] = '\n'; i++; }
        else if (lrc[i] == '\\' && lrc[i+1] == 'r') { b[k++] = '\r'; i++; }
        else b[k++] = lrc[i];
    }
    b[k] = 0;
    assert(lf_parse_lrc(b, &lyrics) == 4);
    assert(lyrics.line[0].start_ms == 3123);
    assert(!strcmp(lyrics.line[0].text,"Earlier line"));
    assert(lyrics.line[1].start_ms == 12500);
    assert(lyrics.line[2].start_ms == 14020);
    assert(lyrics.line[3].start_ms == 15200);
    assert(lf_lyric_at_ms(&lyrics, 0) == -1);
    assert(lf_lyric_at_ms(&lyrics, 3123) == 0);
    assert(lf_lyric_at_ms(&lyrics, 15000) == 2);
    assert(lf_lyric_at_ms(&lyrics, 999999) == 3);
    assert(lf_lyric_progress(1000,0,0) == 0);
    assert(lf_lyric_progress(1000,2000,-2000) == 0);
    assert(lf_lyric_progress(1000,2000,0) == 0.5f);
    assert(lf_lyric_progress(1000,2000,4000) == 1);
    puts("PASS: LRC sorting, repeated tags, timing, progress, edge cases");
    return 0;
}
