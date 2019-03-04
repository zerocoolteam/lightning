#include <assert.h>
#include <bitcoin/privkey.c>
#include <ccan/err/err.h>
#include <ccan/time/time.h>
#include <stdio.h>

/* AUTOGENERATED MOCKS START */
/* AUTOGENERATED MOCKS END */

#define RUNS (256 * 10000)
static struct timerel const_time_test(struct secret *s1,
				      struct secret *s2,
				      size_t off)
{
	struct timeabs start, end;
	int result = 0;

	memset(s1, 0, RUNS * sizeof(*s1));
	memset(s2, 0, RUNS * sizeof(*s2));

	for (size_t i = 0; i < RUNS; i++)
		s2[i].data[off] = i;

	start = time_now();
	for (size_t i = 0; i < RUNS; i++)
		result += secret_eq_consttime(&s1[i], &s2[i]);
	end = time_now();

	if (result != RUNS / 256)
		errx(1, "Expected %u successes at offset %zu, not %u!",
		     RUNS / 256, off, result);

	return time_between(end, start);
}

static inline bool secret_eq_nonconst(const struct secret *a,
				      const struct secret *b)
{
	return memcmp(a, b, sizeof(*a)) == 0;
}

static struct timerel nonconst_time_test(struct secret *s1,
					 struct secret *s2,
					 size_t off)
{
	struct timeabs start, end;
	int result = 0;

	memset(s1, 0, RUNS * sizeof(*s1));
	memset(s2, 0, RUNS * sizeof(*s2));

	for (size_t i = 0; i < RUNS; i++)
		s2[i].data[off] = i;

	start = time_now();
	for (size_t i = 0; i < RUNS; i++)
		result += secret_eq_nonconst(&s1[i], &s2[i]);
	end = time_now();

	if (result != RUNS / 256)
		errx(1, "Expected %u successes at offset %zu, not %u!",
		     RUNS / 256, off, result);

	return time_between(end, start);
}

/* Returns true if test result is expected: we consider 5% "same". */
static bool secret_time_test(struct timerel (*test)(struct secret *s1,
						    struct secret *s2,
						    size_t off),
			     bool should_be_const)
{
	struct secret *s1, *s2;
	struct timerel firstbyte_time, lastbyte_time, diff;

	s1 = calloc(RUNS, sizeof(*s1));
	s2 = calloc(RUNS, sizeof(*s2));

	firstbyte_time = test(s1, s2, 0);
	lastbyte_time = test(s1, s2, sizeof(s1->data)-1);

	free(s1);
	free(s2);

	printf("First byte %u psec vs last byte %u psec\n",
	       (int)time_to_nsec(time_divide(firstbyte_time, RUNS / 1000)),
	       (int)time_to_nsec(time_divide(lastbyte_time, RUNS / 1000)));

	/* If they differ by more than 5%, get upset. */
	if (time_less(firstbyte_time, lastbyte_time))
		diff = time_sub(lastbyte_time, firstbyte_time);
	else {
		/* If the lastbyte test was faster, that's a fail it we expected
		 * it to be slower... */
		if (!should_be_const)
			return false;
		diff = time_sub(firstbyte_time, lastbyte_time);
	}

	return time_less(time_multiply(diff, 20), firstbyte_time) == should_be_const;
}

int main(void)
{
	const char *v;
	int success, i;
	setup_locale();

	/* no point running this under valgrind. */
	v = getenv("VALGRIND");
	if (v && atoi(v) == 1)
		exit(0);

	/* I've never seen this fail more than 5 times */
	success = 0;
	for (i = 0; i < 10; i++)
		success += secret_time_test(const_time_test, true);

	printf("=> Within 5%% %u/%u times\n", success, i);
	if (success < i/2)
		errx(1, "Only const time %u/%u?", success, i);

	/* This, should show measurable differences at least 1/2 the time. */
	success = 0;
	for (i = 0; i < 10; i++)
		success += secret_time_test(nonconst_time_test, false);

	printf("=> More than 5%% slower %u/%u times\n", success, i);
	/* This fails without -O2 or above, at least here (x86 Ubuntu gcc 7.3) */
#ifdef __OPTIMIZE__
	if (success < i/2)
		errx(1, "memcmp seemed const time %u/%u?", success, i);
#endif

	return 0;
}
