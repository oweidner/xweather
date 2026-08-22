/* This file is UTF-8 encoded and contains literal non-ASCII characters in
 * the test strings below (deliberately -- ascii_sanitize() exists to strip
 * exactly this). Embedding the real characters, rather than \x hex escapes,
 * sidesteps a real gotcha: a \xNN escape greedily consumes any hex-digit
 * characters that follow it (e.g. "\x87andarl" reads as the single,
 * out-of-range escape \x87a, not \x87 followed by "andarl"). */

#include "test.h"
#include "../src/ascii_sanitize.h"

static void
test_plain_ascii_passthrough(void)
{
    char out[64];

    ascii_sanitize("Aachen, North Rhine-Westphalia, Germany", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Aachen, North Rhine-Westphalia, Germany");
}

static void
test_turkish_dotless_i(void)
{
    char out[64];

    /* The exact case that motivated this whole module: an ASCII "i" and a
     * Turkish dotless "ı" are different letters, not diacritic variants of
     * each other, but this is still the closest readable ASCII rendering. */
    ascii_sanitize("Çandarlı, İzmir Province, Republic of Türkiye", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Candarli, Izmir Province, Republic of Turkiye");
}

static void
test_german_umlaut(void)
{
    char out[64];

    ascii_sanitize("Zürich, Zürich, Switzerland", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Zurich, Zurich, Switzerland");
}

static void
test_spanish_accent(void)
{
    char out[64];

    ascii_sanitize("Málaga, Andalusia, Spain", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Malaga, Andalusia, Spain");
}

static void
test_portuguese_tilde(void)
{
    char out[64];

    ascii_sanitize("São Paulo, São Paulo, Brazil", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Sao Paulo, Sao Paulo, Brazil");
}

static void
test_polish_stroke_and_dot(void)
{
    char out[64];

    ascii_sanitize("Łódź, Łódzkie, Poland", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Lodz, Lodzkie, Poland");
}

static void
test_czech_caron(void)
{
    char out[64];

    ascii_sanitize("Č ě ř š ž", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "C e r s z");
}

static void
test_french_ligature(void)
{
    char out[64];

    ascii_sanitize("Œuvre, Île-de-France, France", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "OEuvre, Ile-de-France, France");
}

static void
test_empty_string(void)
{
    char out[64];

    ascii_sanitize("", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "");
}

static void
test_output_buffer_truncation(void)
{
    char out[4];

    /* Must stay NUL-terminated within a buffer too small for the whole
     * (already-ASCII) input, never overrun it. */
    ascii_sanitize("Aachen", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Aac");
}

static void
test_unmappable_codepoint_dropped(void)
{
    char out[64];

    /* Tokyo in Japanese script has no Latin transliteration table entry --
     * dropped, not replaced with a placeholder, leaving the ASCII "Tokyo"
     * around it intact. */
    ascii_sanitize("Tokyo 東京", out, sizeof(out));
    TEST_ASSERT_STR_EQ(out, "Tokyo ");
}

int
main(void)
{
    test_plain_ascii_passthrough();
    test_turkish_dotless_i();
    test_german_umlaut();
    test_spanish_accent();
    test_portuguese_tilde();
    test_polish_stroke_and_dot();
    test_czech_caron();
    test_french_ligature();
    test_empty_string();
    test_output_buffer_truncation();
    test_unmappable_codepoint_dropped();

    TEST_SUMMARY();
}
