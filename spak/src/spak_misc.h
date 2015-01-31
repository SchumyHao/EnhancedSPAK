/*
 * SPAK internal header file --- not for client use
 */
 #ifndef __SPAK_MISC_H__
 #define __SPAK_MISC_H__

#define MAX_NAMELEN 70

/*
 * ceiling function for integers
 */
static inline time_value div_ceil (time_value x, time_value y)
{
  if (x%y == 0) {
    return x/y;
  } else {
    return 1+(x/y);
  }
}

/*
 * floor function for integers
 */
static inline time_value div_floor (time_value x, time_value y)
{
  return x/y;
}

#define XMALLOC_CNT 1

#ifdef XMALLOC_CNT
extern int xmalloc_cnt;
#endif

static inline void *xmalloc (size_t size)
{
  void *p = malloc (size);
  if (!p) {
    printf ("oops: out of memory\n");
    assert (0);
  }
#ifdef XMALLOC_CNT
  xmalloc_cnt++;
#endif
  return p;
}

static inline void xfree (void *p)
{
#ifdef XMALLOC_CNT
  xmalloc_cnt--;
#endif
  free (p);
}

#endif
