#ifndef BITAMP_H_INCLUDED
#define BITAMP_H_INCLUDED

// this should be in bitmap.h

// use dynamic allocation with these functions
// compile with -Dbitmap_64 to force 64-bit words
// compile with -O [dash capital o] for more efficient machine code
#ifdef bitmap_64
  #define bitmap_type unsigned long long int
#else	// assumed to be 32 bits
  #define bitmap_type unsigned int
#endif

typedef struct {
  int bits;	// number of bits in the array
  int words;	// number of words in the array
  bitmap_type *array;
} bitmap;

void bitmap_set  (bitmap *b, int n);	// n is a bit index
void bitmap_clear(bitmap *b, int n);
int  bitmap_read (bitmap *b, int n);

bitmap * bitmap_allocate(int bits);
void bitmap_deallocate(bitmap *b);

void bitmap_print(bitmap *b);

#endif // BITAMP_H_INCLUDED
