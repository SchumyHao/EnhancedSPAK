/*
 * Copyright (c) 2002 University of Utah and the Flux Group.
 * All rights reserved.
 *
 * This file is part of SPAK.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation is hereby granted without fee, provided that the
 * above copyright notice and this permission/disclaimer notice is
 * retained in all copies or modified versions, and that both notices
 * appear in supporting documentation.  THE COPYRIGHT HOLDERS PROVIDE
 * THIS SOFTWARE "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE COPYRIGHT
 * HOLDERS DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
 * RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Users are requested, but not required, to send to csl-dist@cs.utah.edu
 * any improvements that they make and grant redistribution rights to the
 * University of Utah.
 *
 * Author: John Regehr (regehr@cs.utah.edu)
 */

/*
 * speedy single-instance priority queue implemented using a heap
 */

#include "spak_public.h"
#include "spak_internal.h"

struct heap_elt {
  time_value key;
  void *ptr;
};

static struct heap_elt *the_heap = NULL;
static unsigned int heap_size;
static unsigned int max_heap_size;

#define INIT_MAX_SIZE 1000

static inline unsigned int parent (unsigned int i)
{
  return i/2;
}

static inline unsigned int left (unsigned int i)
{
  return i*2;
}

static inline unsigned int right (unsigned int i)
{
  return (i*2) + 1;
}

static void heapify (unsigned int i) 
{
  unsigned int l, r, smallest;
  struct heap_elt tmp;

  l = left (i);
  r = right (i);

  if (l < heap_size && the_heap[l].key < the_heap[i].key) {
    smallest = l;
  } else {
    smallest = i;
  }

  if (r < heap_size && the_heap[r].key < the_heap[smallest].key) {
    smallest = r;
  }

  if (smallest != i) {
    tmp = the_heap[smallest];
    the_heap[smallest] = the_heap[i];
    the_heap[i] = tmp;
    heapify (smallest);
  }
}

time_value pri_q_extract_min (void **addr)
{
  void *ptr;
  time_value key;

  assert (the_heap);

  if (heap_size == 0) {
    *addr = NULL;
    return -1;
  }

  ptr = the_heap[0].ptr;
  key = the_heap[0].key;

  the_heap[0] = the_heap[heap_size-1];
  heap_size--;
  heapify (0);

  *addr = ptr;
  return key;
}

void pri_q_insert (time_value key, void *ptr)
{
  unsigned int i;

  assert (the_heap);
  assert (ptr);

  heap_size++;
  i = heap_size-1;

  /*
   * grow the heap if it's about to overflow
   */
  if (heap_size > max_heap_size) {
    max_heap_size *= 2;
    the_heap = (struct heap_elt *) 
      realloc (the_heap, max_heap_size * sizeof (struct heap_elt));
    assert (the_heap);
  }

  while (i>0 && the_heap[parent(i)].key > key) {
    the_heap[i] = the_heap[parent(i)];
    i = parent(i);
  }

  the_heap[i].key = key;
  the_heap[i].ptr = ptr;
}

void init_pri_q (void)
{
  assert (!the_heap);
  heap_size = 0;
  max_heap_size = INIT_MAX_SIZE;
  the_heap = (struct heap_elt *) xmalloc (max_heap_size * sizeof (struct heap_elt));
  assert (the_heap);
}

void deinit_pri_q (void)
{
  assert (the_heap);
  xfree (the_heap);
  the_heap = NULL;
}
