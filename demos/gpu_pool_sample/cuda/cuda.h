#pragma once

#define BX 256

typedef void* stream_handle;
typedef void* event_handle;

extern "C"
void* d_alloc_space(unsigned num_bytes);

extern "C"
void d_free_space(void *ptr);

extern "C"
void h_vec_add(stream_handle stream_hdl,
               int *a, int *b, int *c, 
               int *d_a, int *d_b, int *d_c, 
               unsigned n);