#include "ascii_sanitize.h"

/* Latin-1 Supplement (U+00C0-U+00FF) and Latin Extended-A (U+0100-U+017F),
 * indexed by (codepoint - 0xC0) -- together these two contiguous blocks
 * cover the diacritics/letters that actually show up in European place
 * names (French/German/Spanish/Portuguese/Italian accents, Nordic letters,
 * and Turkish/Polish/Czech/Slovak/Romanian/Croatian/Hungarian letters like
 * dotless "ı", "ł", "ş", "č", "ő"). NULL entries are the two Latin-1
 * symbols in this range that aren't letters (multiplication/division
 * signs) and are dropped, same as any codepoint outside this table. */
static const char *const LATIN_TABLE[] = {
    /* 0x00C0 */ "A", "A", "A", "A", "A", "A", "AE", "C",
    /* 0x00C8 */ "E", "E", "E", "E", "I", "I", "I", "I",
    /* 0x00D0 */ "D", "N", "O", "O", "O", "O", "O", NULL,
    /* 0x00D8 */ "O", "U", "U", "U", "U", "Y", "TH", "ss",
    /* 0x00E0 */ "a", "a", "a", "a", "a", "a", "ae", "c",
    /* 0x00E8 */ "e", "e", "e", "e", "i", "i", "i", "i",
    /* 0x00F0 */ "d", "n", "o", "o", "o", "o", "o", NULL,
    /* 0x00F8 */ "o", "u", "u", "u", "u", "y", "th", "y",
    /* 0x0100 */ "A", "a", "A", "a", "A", "a", "C", "c",
    /* 0x0108 */ "C", "c", "C", "c", "C", "c", "D", "d",
    /* 0x0110 */ "D", "d", "E", "e", "E", "e", "E", "e",
    /* 0x0118 */ "E", "e", "E", "e", "G", "g", "G", "g",
    /* 0x0120 */ "G", "g", "G", "g", "H", "h", "H", "h",
    /* 0x0128 */ "I", "i", "I", "i", "I", "i", "I", "i",
    /* 0x0130 */ "I", "i", "IJ", "ij", "J", "j", "K", "k",
    /* 0x0138 */ "k", "L", "l", "L", "l", "L", "l", "L",
    /* 0x0140 */ "l", "L", "l", "N", "n", "N", "n", "N",
    /* 0x0148 */ "n", "n", "N", "n", "O", "o", "O", "o",
    /* 0x0150 */ "O", "o", "OE", "oe", "R", "r", "R", "r",
    /* 0x0158 */ "R", "r", "S", "s", "S", "s", "S", "s",
    /* 0x0160 */ "S", "s", "T", "t", "T", "t", "T", "t",
    /* 0x0168 */ "U", "u", "U", "u", "U", "u", "U", "u",
    /* 0x0170 */ "U", "u", "U", "u", "W", "w", "Y", "y",
    /* 0x0178 */ "Y", "Z", "z", "Z", "z", "Z", "z", "s",
};

#define LATIN_TABLE_LOW  0x00C0u
#define LATIN_TABLE_HIGH 0x017Fu

void
ascii_sanitize(const char *utf8_in, char *out, size_t out_size)
{
    const unsigned char *p = (const unsigned char *)utf8_in;
    size_t                written = 0;

    if (out_size == 0)
        return;

    while (*p != '\0' && written + 1 < out_size) {
        unsigned char b0 = *p;
        unsigned int  codepoint;
        int           extra_bytes;

        if (b0 < 0x80) {
            out[written++] = (char)b0;
            p++;
            continue;
        }

        if ((b0 & 0xE0) == 0xC0) {
            codepoint = b0 & 0x1F;
            extra_bytes = 1;
        } else if ((b0 & 0xF0) == 0xE0) {
            codepoint = b0 & 0x0F;
            extra_bytes = 2;
        } else if ((b0 & 0xF8) == 0xF0) {
            codepoint = b0 & 0x07;
            extra_bytes = 3;
        } else {
            /* Stray continuation byte or a lead byte this decoder doesn't
             * handle -- skip it and resync on the next byte rather than
             * stopping output early. */
            p++;
            continue;
        }

        {
            const unsigned char *cont = p + 1;
            int                   i;

            for (i = 0; i < extra_bytes; i++) {
                if (cont[i] == '\0' || (cont[i] & 0xC0) != 0x80) {
                    extra_bytes = -1; /* truncated/malformed sequence */
                    break;
                }
                codepoint = (codepoint << 6) | (cont[i] & 0x3F);
            }

            if (extra_bytes < 0) {
                p++; /* skip just the bad lead byte */
                continue;
            }

            p += 1 + extra_bytes;
        }

        if (codepoint >= LATIN_TABLE_LOW && codepoint <= LATIN_TABLE_HIGH) {
            const char *replacement = LATIN_TABLE[codepoint - LATIN_TABLE_LOW];
            const char *r;

            if (!replacement)
                continue;

            for (r = replacement; *r != '\0' && written + 1 < out_size; r++)
                out[written++] = *r;
        }
        /* Any other codepoint (CJK, Cyrillic, Arabic, emoji, ...) has no
         * entry in LATIN_TABLE and is simply dropped. */
    }

    out[written] = '\0';
}
