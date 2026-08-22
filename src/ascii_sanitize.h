#ifndef ASCII_SANITIZE_H
#define ASCII_SANITIZE_H

#include <stddef.h> /* size_t */

/* Writes an ASCII-only rendering of `utf8_in` into `out` (out_size bytes,
 * always NUL-terminated). Latin letters with diacritics are transliterated
 * to their base letter (e.g. "Çandarlı" -> "Candarli"); a codepoint with no
 * reasonable ASCII equivalent is dropped, not replaced with a placeholder.
 * Use this only where a string is about to become visible UI chrome -- a
 * Motif widget's label or the window title -- neither of which render
 * arbitrary Unicode correctly on this app's Motif build (a Unicode window
 * title ends up blank, not just garbled). Leave the original string
 * untouched everywhere else, e.g. geocoding queries. */
void ascii_sanitize(const char *utf8_in, char *out, size_t out_size);

#endif /* ASCII_SANITIZE_H */
