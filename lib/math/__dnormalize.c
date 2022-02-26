#include <float.h>
#include <math.h>
#include <stdint.h>

int
__dnormalize(uint16_t *raw)
{
	int exp, sign;

	if (!(raw[__D0] & __DBL_FRAC) && !raw[__D1] && !raw[__D2] && !raw[__D3])
		return 0;

	// Remember the sign bit.
	sign = raw[__D0] & __DBL_SIGN;
	raw[__D0] &= __DBL_FRAC;

	// To improve performance, try to shift left by 16 bits at a time (may
  // overshoot).
	for (exp = 0; raw[__D0] == 0; exp -= 16) {
		raw[__D0] = raw[__D1];
		raw[__D1] = raw[__D2];
		raw[__D2] = raw[__D3];
		raw[__D3] = 0;
	}

	// If we have overshoot, back up by shifting right.
	for ( ; raw[__D0] >= (1 << (__DBL_EOFF + 1)); exp++) {
		raw[__D3] = (raw[__D3] >> 1) | (raw[__D2] << 15);
		raw[__D2] = (raw[__D2] >> 1) | (raw[__D1] << 15);
		raw[__D1] = (raw[__D1] >> 1) | (raw[__D0] << 15);
		raw[__D0] = (raw[__D0] >> 1);
	}

	// Otherwise, continue shifting left by 1 bit at a time.
	for ( ; raw[__D0] < (1 << (__DBL_EOFF)); exp--) {
		raw[__D0] = (raw[__D0] << 1) | (raw[__D1] >> 15);
		raw[__D1] = (raw[__D1] << 1) | (raw[__D2] >> 15);
		raw[__D2] = (raw[__D2] << 1) | (raw[__D3] >> 15);
		raw[__D3] = (raw[__D3] << 1);
	}

	// Clear the implicit fraction bit and restore the sign bit.
	raw[__D0] = (raw[__D0] & __DBL_FRAC) | sign;

	return exp;
}
