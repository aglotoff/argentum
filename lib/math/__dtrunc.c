#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>

int
__dtrunc(double *x, int texp)
{
	static uint16_t mask[] = {
		0x0000, 0x0001, 0x0003, 0x0007,
		0x000f, 0x001f, 0x003f, 0x007f,
		0x00ff, 0x01ff, 0x03ff, 0x07ff,
		0x0fff, 0x1fff, 0x3fff, 0x7fff,
	};
	static int sub[] = { __D3, __D2, __D1, __D0 };

	uint16_t *raw;
	int exp, frac, drop_bits, drop_words;
	
	raw  = (uint16_t *) x;
	exp  = (raw[__D0] & __DBL_EXP) >> __DBL_EOFF;
	frac = (raw[__D0] & __DBL_FRAC) || raw[__D1] || raw[__D2] || raw[__D3];

	if (exp == __DBL_EMAX) {
		if (!frac)
			return FP_INFINITE;

		errno = EDOM;
		return FP_NAN;
	}

	if ((exp == 0) && !frac)
		return FP_ZERO;

	// Determine the number of fraction bits to drop.
	drop_bits = __DBL_FBITS - (exp - __DBL_EBIAS) - texp;
	
	if (drop_bits <= 0)
    // Nothing to drop.
		return FP_ZERO;

	if (drop_bits >= __DBL_FBITS) {
    // Clear all fraction bits.
		raw[__D0] = raw[__D1] = raw[__D2] = raw[__D3] = 0;
		return exp ? FP_NORMAL : FP_SUBNORMAL;
	}

	drop_words = drop_bits >> 4;
	drop_bits &= 0xF;

	// Clear fraction bits in the highest word.
	frac = raw[sub[drop_words]] & mask[drop_bits];
	raw[sub[drop_words]] &= ~mask[drop_bits];

	// Clear whole 16-bit words.
	switch (drop_words) {
	case 3:
		frac |= raw[__D1];
    raw[__D1] = 0;
    // fall through
	case 2:
		frac |= raw[__D2];
    raw[__D2] = 0;
    // fall through
	case 1:
		frac |= raw[__D3];
    raw[__D3] = 0;
    // fall through
	}

	if (!frac)
		return FP_ZERO;
	return exp ? FP_NORMAL : FP_SUBNORMAL;
}
