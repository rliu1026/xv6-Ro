#include "rand.h"

unsigned int rand_seed = 1;

/* The state word must be initialized to non-zero */
int xv6_rand()
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	unsigned int x = rand_seed;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rand_seed = x;
	return x % XV6_RAND_MAX;
}

void 
xv6_srand (unsigned int seed)
{
    rand_seed = seed;
    return;
}



