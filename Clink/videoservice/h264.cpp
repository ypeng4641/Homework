
#include "h264.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>


#ifdef __cplusplus
extern "C" {
#endif

#define NEG_SSR32(a,s) ((( int32_t)(a))>>(32-(s)))
#define NEG_USR32(a,s) (((uint32_t)(a))>>(32-(s)))

static inline const int sign_extend(int val, unsigned bits)
{
	unsigned shift = 8 * sizeof(int) - bits;
	union { unsigned u; int s; } v = { (unsigned) val << shift };
	return v.s >> shift;
}


#   define AV_RB32(x)                                \
	(((uint32_t)((const uint8_t*)(x))[0] << 24) |    \
	(((const uint8_t*)(x))[1] << 16) |    \
	(((const uint8_t*)(x))[2] <<  8) |    \
	((const uint8_t*)(x))[3])


#   define AV_WB32(p, d) do {                   \
	((uint8_t*)(p))[3] = (d);               \
	((uint8_t*)(p))[2] = (d)>>8;            \
	((uint8_t*)(p))[1] = (d)>>16;           \
	((uint8_t*)(p))[0] = (d)>>24;           \
} while(0)


#   define AV_NE(be, le) (le)
//rounded division & shift
#define RSHIFT(a,b) ((a) > 0 ? ((a) + ((1<<(b))>>1))>>(b) : ((a) + ((1<<(b))>>1)-1)>>(b))
/* assume b>0 */
#define ROUNDED_DIV(a,b) (((a)>0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))
#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FFSIGN(a) ((a) > 0 ? 1 : -1)

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMAX3(a,b,c) FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define FFMIN3(a,b,c) FFMIN(FFMIN(a,b),c)

#define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define FFALIGN(x, a) (((x)+(a)-1)&~((a)-1))

/* misc math functions */
const uint8_t ff_log2_tab[256]={
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
static inline const int av_log2(unsigned int v)
{
	int n = 0;
	if (v & 0xffff0000) {
		v >>= 16;
		n += 16;
	}
	if (v & 0xff00) {
		v >>= 8;
		n += 8;
	}
	n += ff_log2_tab[v];

	return n;
}

/* Bitstream reader API docs:
name
    arbitrary name which is used as prefix for the internal variables

gb
    getbitcontext

OPEN_READER(name, gb)
    load gb into local variables

CLOSE_READER(name, gb)
    store local vars in gb

UPDATE_CACHE(name, gb)
    refill the internal cache from the bitstream
    after this call at least MIN_CACHE_BITS will be available,

GET_CACHE(name, gb)
    will output the contents of the internal cache, next bit is MSB of 32 or 64 bit (FIXME 64bit)

SHOW_UBITS(name, gb, num)
    will return the next num bits

SHOW_SBITS(name, gb, num)
    will return the next num bits and do sign extension

SKIP_BITS(name, gb, num)
    will skip over the next num bits
    note, this is equivalent to SKIP_CACHE; SKIP_COUNTER

SKIP_CACHE(name, gb, num)
    will remove the next num bits from the cache (note SKIP_COUNTER MUST be called before UPDATE_CACHE / CLOSE_READER)

SKIP_COUNTER(name, gb, num)
    will increment the internal bit counter (see SKIP_CACHE & SKIP_BITS)

LAST_SKIP_BITS(name, gb, num)
    like SKIP_BITS, to be used if next call is UPDATE_CACHE or CLOSE_READER

for examples see get_bits, show_bits, skip_bits, get_vlc
*/

typedef struct GetBitContext {
	const uint8_t *buffer, *buffer_end;
	int index;
	int size_in_bits;
} GetBitContext;


#define MIN_CACHE_BITS 25

#define OPEN_READER(name, gb)                   \
    unsigned int name##_index = (gb)->index;    \
    unsigned int name##_cache = 0

#define CLOSE_READER(name, gb) (gb)->index = name##_index

#define UPDATE_CACHE(name, gb) name##_cache = \
        AV_RB32((gb)->buffer + (name##_index >> 3)) << (name##_index & 7)

# define SKIP_CACHE(name, gb, num) name##_cache <<= (num)

#define SKIP_COUNTER(name, gb, num) name##_index += (num)

#define SKIP_BITS(name, gb, num) do {           \
        SKIP_CACHE(name, gb, num);              \
        SKIP_COUNTER(name, gb, num);            \
    } while (0)

#define LAST_SKIP_BITS(name, gb, num) SKIP_COUNTER(name, gb, num)

#define SHOW_UBITS(name, gb, num) NEG_USR32(name##_cache, num)
#define SHOW_SBITS(name, gb, num) NEG_SSR32(name##_cache, num)

#define GET_CACHE(name, gb) ((uint32_t)name##_cache)

static inline int get_bits_count(const GetBitContext *s)
{
    return s->index;
}

static inline void skip_bits_long(GetBitContext *s, int n){
    s->index += n;
}

/**
 * read mpeg1 dc style vlc (sign bit + mantisse with no MSB).
 * if MSB not set it is negative
 * @param n length in bits
 */
static inline int get_xbits(GetBitContext *s, int n)
{
    register int sign;
    register int32_t cache;
    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    cache = GET_CACHE(re, s);
    sign = ~cache >> 31;
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    return (NEG_USR32(sign ^ cache, n) ^ sign) - sign;
}

static inline int get_sbits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    tmp = SHOW_SBITS(re, s, n);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    return tmp;
}

/**
 * Read 1-25 bits.
 */
static inline unsigned int get_bits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    tmp = SHOW_UBITS(re, s, n);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
    return tmp;
}

/**
 * Show 1-25 bits.
 */
static inline unsigned int show_bits(GetBitContext *s, int n)
{
    register int tmp;
    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    tmp = SHOW_UBITS(re, s, n);
    return tmp;
}

static inline void skip_bits(GetBitContext *s, int n)
{
    OPEN_READER(re, s);
    UPDATE_CACHE(re, s);
    LAST_SKIP_BITS(re, s, n);
    CLOSE_READER(re, s);
}

static inline unsigned int get_bits1(GetBitContext *s)
{
    unsigned int index = s->index;
    uint8_t result = s->buffer[index>>3];
    result <<= index & 7;
    result >>= 8 - 1;
        index++;
    s->index = index;

    return result;
}

static inline unsigned int show_bits1(GetBitContext *s)
{
    return show_bits(s, 1);
}

static inline void skip_bits1(GetBitContext *s)
{
    skip_bits(s, 1);
}

/**
 * Read 0-32 bits.
 */
static inline unsigned int get_bits_long(GetBitContext *s, int n)
{
    if (n <= MIN_CACHE_BITS)
        return get_bits(s, n);
    else {
        int ret = get_bits(s, 16) << (n-16);
        return ret | get_bits(s, n-16);
    }
}

/**
 * Read 0-32 bits as a signed integer.
 */
static inline int get_sbits_long(GetBitContext *s, int n)
{
    return sign_extend(get_bits_long(s, n), n);
}

/**
 * Show 0-32 bits.
 */
static inline unsigned int show_bits_long(GetBitContext *s, int n)
{
    if (n <= MIN_CACHE_BITS)
        return show_bits(s, n);
    else {
        GetBitContext gb = *s;
        return get_bits_long(&gb, n);
    }
}

/*static inline int check_marker(GetBitContext *s, const char *msg)
{
    int bit = get_bits1(s);
    if (!bit)
        av_log(NULL, AV_LOG_INFO, "Marker bit missing %s\n", msg);

    return bit;
}*/

/**
 * Inititalize GetBitContext.
 * @param buffer bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE bytes larger than the actual read bits
 * because some optimized bitstream readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 */
static inline void init_get_bits(GetBitContext *s, const uint8_t *buffer,
                                 int bit_size)
{
    int buffer_size = (bit_size+7)>>3;
    if (buffer_size < 0 || bit_size < 0) {
        buffer_size = bit_size = 0;
        buffer = NULL;
    }

    s->buffer       = buffer;
    s->size_in_bits = bit_size;
    s->buffer_end   = buffer + buffer_size;
    s->index        = 0;
}

static inline void align_get_bits(GetBitContext *s)
{
    int n = -get_bits_count(s) & 7;
    if (n) skip_bits(s, n);
}

static inline int decode012(GetBitContext *gb)
{
    int n;
    n = get_bits1(gb);
    if (n == 0)
        return 0;
    else
        return get_bits1(gb) + 1;
}

static inline int decode210(GetBitContext *gb)
{
    if (get_bits1(gb))
        return 0;
    else
        return 2 - get_bits1(gb);
}

static inline int get_bits_left(GetBitContext *gb)
{
    return gb->size_in_bits - get_bits_count(gb);
}

typedef struct PutBitContext {
    uint32_t bit_buf;
    int bit_left;
    uint8_t *buf, *buf_ptr, *buf_end;
    int size_in_bits;
} PutBitContext;

/**
 * Initialize the PutBitContext s.
 *
 * @param buffer the buffer where to put bits
 * @param buffer_size the size in bytes of buffer
 */
static inline void init_put_bits(PutBitContext *s, uint8_t *buffer, int buffer_size)
{
    if(buffer_size < 0) {
        buffer_size = 0;
        buffer = NULL;
    }

    s->size_in_bits= 8*buffer_size;
    s->buf = buffer;
    s->buf_end = s->buf + buffer_size;
    s->buf_ptr = s->buf;
    s->bit_left=32;
    s->bit_buf=0;
}

/**
 * @return the total number of bits written to the bitstream.
 */
static inline int put_bits_count(PutBitContext *s)
{
    return (s->buf_ptr - s->buf) * 8 + 32 - s->bit_left;
}

/**
 * Pad the end of the output stream with zeros.
 */
static inline void flush_put_bits(PutBitContext *s)
{
    if (s->bit_left < 32)
        s->bit_buf<<= s->bit_left;
    while (s->bit_left < 32) {
        *s->buf_ptr++=s->bit_buf >> 24;
        s->bit_buf<<=8;
        s->bit_left+=8;
    }
    s->bit_left=32;
    s->bit_buf=0;
}

/**
 * Write up to 31 bits into a bitstream.
 * Use put_bits32 to write 32 bits.
 */
static inline void put_bits(PutBitContext *s, int n, unsigned int value)
{
    unsigned int bit_buf;
    int bit_left;

    //    printf("put_bits=%d %x\n", n, value);
    assert(n <= 31 && value < (1U << n));

    bit_buf = s->bit_buf;
    bit_left = s->bit_left;

    //    printf("n=%d value=%x cnt=%d buf=%x\n", n, value, bit_cnt, bit_buf);
    /* XXX: optimize */
    if (n < bit_left) {
        bit_buf = (bit_buf<<n) | value;
        bit_left-=n;
    } else {
        bit_buf<<=bit_left;
        bit_buf |= value >> (n - bit_left);
        AV_WB32(s->buf_ptr, bit_buf);
        //printf("bitbuf = %08x\n", bit_buf);
        s->buf_ptr+=4;
        bit_left+=32 - n;
        bit_buf = value;
    }

    s->bit_buf = bit_buf;
    s->bit_left = bit_left;
}

static inline void put_sbits(PutBitContext *pb, int n, int32_t value)
{
    assert(n >= 0 && n <= 31);

    put_bits(pb, n, value & ((1<<n)-1));
}

/**
 * Write exactly 32 bits into a bitstream.
 */
static void put_bits32(PutBitContext *s, uint32_t value)
{
    int lo = value & 0xffff;
    int hi = value >> 16;
    put_bits(s, 16, hi);
    put_bits(s, 16, lo);
}

/**
 * Return the pointer to the byte where the bitstream writer will put
 * the next bit.
 */
static inline uint8_t* put_bits_ptr(PutBitContext *s)
{
        return s->buf_ptr;
}

/**
 * Skip the given number of bytes.
 * PutBitContext must be flushed & aligned to a byte boundary before calling this.
 */
static inline void skip_put_bytes(PutBitContext *s, int n)
{
        assert((put_bits_count(s)&7)==0);
        assert(s->bit_left==32);
        s->buf_ptr += n;
}

/**
 * Skip the given number of bits.
 * Must only be used if the actual values in the bitstream do not matter.
 * If n is 0 the behavior is undefined.
 */
static inline void skip_put_bits(PutBitContext *s, int n)
{
    s->bit_left -= n;
    s->buf_ptr-= 4*(s->bit_left>>5);
    s->bit_left &= 31;
}

/**
 * Change the end of the buffer.
 *
 * @param size the new size in bytes of the buffer where to put bits
 */
static inline void set_put_bits_buffer_size(PutBitContext *s, int size)
{
    s->buf_end= s->buf + size;
}

#define INVALID_VLC           0x80000000
#define MAX_SPS_COUNT 32
#define MAX_PICTURE_COUNT 32

const uint8_t ff_golomb_vlc_len[512]={
	19,17,15,15,13,13,13,13,11,11,11,11,11,11,11,11,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

const uint8_t ff_ue_golomb_vlc_code[512]={
	32,32,32,32,32,32,32,32,31,32,32,32,32,32,32,32,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,
	7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const int8_t ff_se_golomb_vlc_code[512]={
	17, 17, 17, 17, 17, 17, 17, 17, 16, 17, 17, 17, 17, 17, 17, 17,  8, -8,  9, -9, 10,-10, 11,-11, 12,-12, 13,-13, 14,-14, 15,-15,
	4,  4,  4,  4, -4, -4, -4, -4,  5,  5,  5,  5, -5, -5, -5, -5,  6,  6,  6,  6, -6, -6, -6, -6,  7,  7,  7,  7, -7, -7, -7, -7,
	2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
	3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3, -3,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};


const uint8_t ff_ue_golomb_len[256]={
	1, 3, 3, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,11,
	11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,13,
	13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
	13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,15,
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,17,
};

const uint8_t ff_interleaved_golomb_vlc_len[256]={
	9,9,7,7,9,9,7,7,5,5,5,5,5,5,5,5,
	9,9,7,7,9,9,7,7,5,5,5,5,5,5,5,5,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	9,9,7,7,9,9,7,7,5,5,5,5,5,5,5,5,
	9,9,7,7,9,9,7,7,5,5,5,5,5,5,5,5,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

const uint8_t ff_interleaved_ue_golomb_vlc_code[256]={
	15,16,7, 7, 17,18,8, 8, 3, 3, 3, 3, 3, 3, 3, 3,
	19,20,9, 9, 21,22,10,10,4, 4, 4, 4, 4, 4, 4, 4,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	23,24,11,11,25,26,12,12,5, 5, 5, 5, 5, 5, 5, 5,
	27,28,13,13,29,30,14,14,6, 6, 6, 6, 6, 6, 6, 6,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

const int8_t ff_interleaved_se_golomb_vlc_code[256]={
	8, -8,  4,  4,  9, -9, -4, -4,  2,  2,  2,  2,  2,  2,  2,  2,
	10,-10,  5,  5, 11,-11, -5, -5, -2, -2, -2, -2, -2, -2, -2, -2,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	12,-12,  6,  6, 13,-13, -6, -6,  3,  3,  3,  3,  3,  3,  3,  3,
	14,-14,  7,  7, 15,-15, -7, -7, -3, -3, -3, -3, -3, -3, -3, -3,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

const uint8_t ff_interleaved_dirac_golomb_vlc_code[256]={
	0, 1, 0, 0, 2, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
	4, 5, 2, 2, 6, 7, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	8, 9, 4, 4, 10,11,5, 5, 2, 2, 2, 2, 2, 2, 2, 2,
	12,13,6, 6, 14,15,7, 7, 3, 3, 3, 3, 3, 3, 3, 3,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


static const uint8_t zigzag_scan[16]={
	0+0*4, 1+0*4, 0+1*4, 0+2*4,
	1+1*4, 2+0*4, 3+0*4, 2+1*4,
	1+2*4, 0+3*4, 1+3*4, 2+2*4,
	3+1*4, 3+2*4, 2+3*4, 3+3*4,
};

const uint8_t ff_zigzag_direct[64] = {
	0,   1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

static const uint8_t default_scaling4[2][16]={
	{
		 6, 13, 20, 28,
	    13, 20, 28, 32,
	    20, 28, 32, 37,
	    28, 32, 37, 42
	},{
		10, 14, 20, 24,
		14, 20, 24, 27,
		20, 24, 27, 30,
		24, 27, 30, 34
	}
};

static const uint8_t default_scaling8[2][64]={
	{  
		6,10,13,16,18,23,25,27,
		10,11,16,18,23,25,27,29,
		13,16,18,23,25,27,29,31,
		16,18,23,25,27,29,31,33,
		18,23,25,27,29,31,33,36,
		23,25,27,29,31,33,36,38,
		25,27,29,31,33,36,38,40,
		27,29,31,33,36,38,40,42
	},{
		9,13,15,17,19,21,22,24,
		13,13,17,19,21,22,24,25,
		15,17,19,21,22,24,25,27,
		17,19,21,22,24,25,27,28,
		19,21,22,24,25,27,28,30,
		21,22,24,25,27,28,30,32,
		22,24,25,27,28,30,32,33,
		24,25,27,28,30,32,33,35
	}
};


 /**
 * read unsigned exp golomb code.
 */
static inline int get_ue_golomb(GetBitContext *gb){
    unsigned int buf;
    int log;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf=GET_CACHE(re, gb);

    if(buf >= (1<<27)){
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);

        return ff_ue_golomb_vlc_code[buf];
    }else{
        log= 2*av_log2(buf) - 31;
        buf>>= log;
        buf--;
        LAST_SKIP_BITS(re, gb, 32 - log);
        CLOSE_READER(re, gb);

        return buf;
    }
}

/**
 * Read an unsigned Exp-Golomb code in the range 0 to UINT32_MAX-1.
 */
static inline unsigned get_ue_golomb_long(GetBitContext *gb)
{
    unsigned buf, log;

    buf = show_bits_long(gb, 32);
    log = 31 - av_log2(buf);
    skip_bits_long(gb, log);

    return get_bits_long(gb, log + 1) - 1;
}

 /**
 * read unsigned exp golomb code, constraint to a max of 31.
 * the return value is undefined if the stored value exceeds 31.
 */
static inline int get_ue_golomb_31(GetBitContext *gb){
    unsigned int buf;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf=GET_CACHE(re, gb);

    buf >>= 32 - 9;
    LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
    CLOSE_READER(re, gb);

    return ff_ue_golomb_vlc_code[buf];
}

/**
 * read unsigned truncated exp golomb code.
 */
static inline int get_te0_golomb(GetBitContext *gb, int range){
    assert(range >= 1);

    if(range==1)      return 0;
    else if(range==2) return get_bits1(gb)^1;
    else              return get_ue_golomb(gb);
}

/**
 * read unsigned truncated exp golomb code.
 */
static inline int get_te_golomb(GetBitContext *gb, int range){
    assert(range >= 1);

    if(range==2) return get_bits1(gb)^1;
    else         return get_ue_golomb(gb);
}


/**
 * read signed exp golomb code.
 */
static inline int get_se_golomb(GetBitContext *gb){
    unsigned int buf;
    int log;

    OPEN_READER(re, gb);
    UPDATE_CACHE(re, gb);
    buf=GET_CACHE(re, gb);

    if(buf >= (1<<27)){
        buf >>= 32 - 9;
        LAST_SKIP_BITS(re, gb, ff_golomb_vlc_len[buf]);
        CLOSE_READER(re, gb);

        return ff_se_golomb_vlc_code[buf];
    }else{
        log= 2*av_log2(buf) - 31;
        buf>>= log;

        LAST_SKIP_BITS(re, gb, 32 - log);
        CLOSE_READER(re, gb);

        if(buf&1) buf= -(buf>>1);
        else      buf=  (buf>>1);

        return buf;
    }
}

/**
 * write unsigned exp golomb code.
 */
static inline void set_ue_golomb(PutBitContext *pb, int i){
    int e;

    assert(i>=0);

#if 0
    if(i=0){
        put_bits(pb, 1, 1);
        return;
    }
#endif
    if(i<256)
        put_bits(pb, ff_ue_golomb_len[i], i+1);
    else{
        e= av_log2(i+1);

        put_bits(pb, 2*e+1, i+1);
    }
}

/**
 * write truncated unsigned exp golomb code.
 */
static inline void set_te_golomb(PutBitContext *pb, int i, int range){
    assert(range >= 1);
    assert(i<=range);

    if(range==2) put_bits(pb, 1, i^1);
    else         set_ue_golomb(pb, i);
}

/**
 * write signed exp golomb code. 16 bits at most.
 */
static inline void set_se_golomb(PutBitContext *pb, int i){
//    if (i>32767 || i<-32767)
//        av_log(NULL,AV_LOG_ERROR,"value out of range %d\n", i);
#if 0
    if(i<=0) i= -2*i;
    else     i=  2*i-1;
#elif 1
    i= 2*i-1;
    if(i<0) i^= -1; //FIXME check if gcc does the right thing
#else
    i= 2*i-1;
    i^= (i>>31);
#endif
    set_ue_golomb(pb, i);
}


#define EXTENDED_SAR          255
static const AVRational pixel_aspect[17]={
	{0, 1},{1, 1},{12, 11},{10, 11},{16, 11},{40, 33},{24, 11},{20, 11},{32, 11},{80, 33},{18, 11},{15, 11},{64, 33},{160,99},{4, 3},{3, 2},{2, 1},
};

static inline int decode_hrd_parameters(GetBitContext *gb, SPS *sps){
	int cpb_count, i;
	cpb_count = get_ue_golomb_31(gb) + 1;

	if(cpb_count > 32U){
		return -1;
	}

	get_bits(gb, 4); /* bit_rate_scale */
	get_bits(gb, 4); /* cpb_size_scale */
	for(i=0; i<cpb_count; i++){
		get_ue_golomb_long(gb); /* bit_rate_value_minus1 */
		get_ue_golomb_long(gb); /* cpb_size_value_minus1 */
		get_bits1(gb);     /* cbr_flag */
	}
	sps->initial_cpb_removal_delay_length = get_bits(gb, 5) + 1;
	sps->cpb_removal_delay_length = get_bits(gb, 5) + 1;
	sps->dpb_output_delay_length = get_bits(gb, 5) + 1;
	sps->time_offset_length = get_bits(gb, 5);
	sps->cpb_cnt = cpb_count;
	return 0;
}

static void decode_scaling_list(GetBitContext *gb, uint8_t *factors, int size,
								const uint8_t *jvt_list, const uint8_t *fallback_list)
{
	int i, last = 8, next = 8;
	const uint8_t *scan = size == 16 ? zigzag_scan : ff_zigzag_direct;
	if(!get_bits1(gb)) /* matrix not written, we use the predicted one */
		memcpy(factors, fallback_list, size*sizeof(uint8_t));
	else
		for(i=0;i<size;i++){
			if(next)
				next = (last + get_se_golomb(gb)) & 0xff;
			if(!i && !next){ /* matrix not written, we use the preset one */
				memcpy(factors, jvt_list, size*sizeof(uint8_t));
				break;
			}
			last = factors[scan[i]] = next ? next : last;
		}
}

static void decode_scaling_matrices(GetBitContext *gb, SPS *sps, PPS *pps, int is_sps,
									uint8_t (*scaling_matrix4)[16], uint8_t (*scaling_matrix8)[64])
{
	int fallback_sps = !is_sps && sps->scaling_matrix_present;
	const uint8_t *fallback[4] = {
		fallback_sps ? sps->scaling_matrix4[0] : default_scaling4[0],
		fallback_sps ? sps->scaling_matrix4[3] : default_scaling4[1],
		fallback_sps ? sps->scaling_matrix8[0] : default_scaling8[0],
		fallback_sps ? sps->scaling_matrix8[3] : default_scaling8[1]
	};
	if(get_bits1(gb)){    //	seq_scaling_matrix_present_flag
		sps->scaling_matrix_present |= is_sps;
		decode_scaling_list(gb,scaling_matrix4[0],16,default_scaling4[0],fallback[0]); // Intra, Y
		decode_scaling_list(gb,scaling_matrix4[1],16,default_scaling4[0],scaling_matrix4[0]); // Intra, Cr
		decode_scaling_list(gb,scaling_matrix4[2],16,default_scaling4[0],scaling_matrix4[1]); // Intra, Cb
		decode_scaling_list(gb,scaling_matrix4[3],16,default_scaling4[1],fallback[1]); // Inter, Y
		decode_scaling_list(gb,scaling_matrix4[4],16,default_scaling4[1],scaling_matrix4[3]); // Inter, Cr
		decode_scaling_list(gb,scaling_matrix4[5],16,default_scaling4[1],scaling_matrix4[4]); // Inter, Cb
		if(is_sps || pps->transform_8x8_mode){
			decode_scaling_list(gb,scaling_matrix8[0],64,default_scaling8[0],fallback[2]);  // Intra, Y
			if(sps->chroma_format_idc == 3){
				decode_scaling_list(gb,scaling_matrix8[1],64,default_scaling8[0],scaling_matrix8[0]);  // Intra, Cr
				decode_scaling_list(gb,scaling_matrix8[2],64,default_scaling8[0],scaling_matrix8[1]);  // Intra, Cb
			}
			decode_scaling_list(gb,scaling_matrix8[3],64,default_scaling8[1],fallback[3]);  // Inter, Y
			if(sps->chroma_format_idc == 3){
				decode_scaling_list(gb,scaling_matrix8[4],64,default_scaling8[1],scaling_matrix8[3]);  // Inter, Cr
				decode_scaling_list(gb,scaling_matrix8[5],64,default_scaling8[1],scaling_matrix8[4]);  // Inter, Cb
			}
		}
	}
}

static inline int decode_vui_parameters(GetBitContext *gb, SPS *sps)
{
	int aspect_ratio_info_present_flag;
	unsigned int aspect_ratio_idc;

	aspect_ratio_info_present_flag= get_bits1(gb);

	if( aspect_ratio_info_present_flag ) {
		aspect_ratio_idc= get_bits(gb, 8);
		if( aspect_ratio_idc == EXTENDED_SAR ) {
			sps->sar.num= get_bits(gb, 16);
			sps->sar.den= get_bits(gb, 16);
		}else if(aspect_ratio_idc < FF_ARRAY_ELEMS(pixel_aspect)){
			sps->sar=  pixel_aspect[aspect_ratio_idc];
		}else{
			return -1;
		}
	}else{
		sps->sar.num=
			sps->sar.den= 0;
	}
	//            s->avctx->aspect_ratio= sar_width*s->width / (float)(s->height*sar_height);

	if(get_bits1(gb)){      /* overscan_info_present_flag */
		get_bits1(gb);      /* overscan_appropriate_flag */
	}

	sps->video_signal_type_present_flag = get_bits1(gb);
	if(sps->video_signal_type_present_flag){
		get_bits(gb, 3);    /* video_format */
		sps->full_range = get_bits1(gb); /* video_full_range_flag */

		sps->colour_description_present_flag = get_bits1(gb);
		if(sps->colour_description_present_flag){
			uint32_t color_primaries = get_bits(gb, 8); /* colour_primaries */
			switch(color_primaries)
			{
			case AVCOL_PRI_BT709:
				sps->color_primaries =AVCOL_PRI_BT709;
				break;
			case AVCOL_PRI_BT470M:
				sps->color_primaries =AVCOL_PRI_BT470M;
				break;
			case AVCOL_PRI_BT470BG:
				sps->color_primaries =AVCOL_PRI_BT470BG;
				break;
			case AVCOL_PRI_SMPTE170M:
				sps->color_primaries =AVCOL_PRI_SMPTE170M;
				break;
			case AVCOL_PRI_SMPTE240M:
				sps->color_primaries =AVCOL_PRI_SMPTE240M;
				break;
			case AVCOL_PRI_FILM:
				sps->color_primaries =AVCOL_PRI_FILM;
				break;
			default:
				sps->color_primaries =AVCOL_PRI_UNSPECIFIED;
			}

			uint32_t color_trc       = get_bits(gb, 8); /* transfer_characteristics */
			switch(color_trc)
			{
			case AVCOL_TRC_BT709:
				sps->color_trc =AVCOL_TRC_BT709;
				break;
				break;
			case AVCOL_TRC_GAMMA22:
				sps->color_trc =AVCOL_TRC_GAMMA22;
				break;
			case AVCOL_TRC_GAMMA28:
				sps->color_trc =AVCOL_TRC_GAMMA28;
				break;
			default:
				sps->color_trc =AVCOL_TRC_UNSPECIFIED;
			}

			uint32_t colorspace      = get_bits(gb, 8); /* matrix_coefficients */
			switch(colorspace)
			{
			case AVCOL_SPC_RGB:
				sps->colorspace =AVCOL_SPC_RGB;
				break;
			case AVCOL_SPC_BT709:
				sps->colorspace =AVCOL_SPC_BT709;
				break;
			case AVCOL_SPC_FCC:
				sps->colorspace =AVCOL_SPC_FCC;
				break;
			case AVCOL_SPC_BT470BG:
				sps->colorspace =AVCOL_SPC_BT470BG;
				break;
			case AVCOL_SPC_SMPTE170M:
				sps->colorspace =AVCOL_SPC_SMPTE170M;
				break;
			case AVCOL_SPC_SMPTE240M:
				sps->colorspace =AVCOL_SPC_SMPTE240M;
				break;
			default:
				sps->colorspace =AVCOL_SPC_UNSPECIFIED;
			}
		}
	}

	if(get_bits1(gb)){      /* chroma_location_info_present_flag */
		uint32_t chroma_sample_location = get_ue_golomb(gb)+1;  /* chroma_sample_location_type_top_field */
		get_ue_golomb(gb);  /* chroma_sample_location_type_bottom_field */
	}

	sps->timing_info_present_flag = get_bits1(gb);
	if(sps->timing_info_present_flag){
		sps->num_units_in_tick = get_bits_long(gb, 32);
		sps->time_scale = get_bits_long(gb, 32);
		if(!sps->num_units_in_tick || !sps->time_scale){
			return false;
		}
		sps->fixed_frame_rate_flag = get_bits1(gb);
	}

	sps->nal_hrd_parameters_present_flag = get_bits1(gb);
	if(sps->nal_hrd_parameters_present_flag)
		if(decode_hrd_parameters(gb, sps) < 0)
			return -1;
	sps->vcl_hrd_parameters_present_flag = get_bits1(gb);
	if(sps->vcl_hrd_parameters_present_flag)
		if(decode_hrd_parameters(gb, sps) < 0)
			return -1;
	if(sps->nal_hrd_parameters_present_flag || sps->vcl_hrd_parameters_present_flag)
		get_bits1(gb);     /* low_delay_hrd_flag */
	sps->pic_struct_present_flag = get_bits1(gb);

	sps->bitstream_restriction_flag = get_bits1(gb);
	if(sps->bitstream_restriction_flag){
		get_bits1(gb);     /* motion_vectors_over_pic_boundaries_flag */
		get_ue_golomb(gb); /* max_bytes_per_pic_denom */
		get_ue_golomb(gb); /* max_bits_per_mb_denom */
		get_ue_golomb(gb); /* log2_max_mv_length_horizontal */
		get_ue_golomb(gb); /* log2_max_mv_length_vertical */
		sps->num_reorder_frames= get_ue_golomb(gb);
		get_ue_golomb(gb); /*max_dec_frame_buffering*/

		if(gb->size_in_bits < get_bits_count(gb)){
			get_bits_count(gb);
			sps->num_reorder_frames=0;
			sps->bitstream_restriction_flag= 0;
		}

		if(sps->num_reorder_frames > 16U /*max_dec_frame_buffering || max_dec_frame_buffering > 16*/){
			return -1;
		}
	}

	return 0;
}

#ifdef __cplusplus
}
#endif


bool H264Parse::CheckNal(uint8_t * buffer, uint32_t buffer_length, NALHeader * header)
{
	if (!buffer || buffer_length < 4 || !header){
		return false;
	}

	uint32_t offset =3;		// 消费字节游标
	//				 offset
	//  0    1    2    3    4    5    6    7   |   8    9    10   11   12   13      
	// 0x00 0x00 0x01 0xXX 0xXX 0xXX 0xXX 0x00 |  0x00 0x00 0x01 0xXX 0xXX 0xXX 

	// 需找起始标记
	while (offset < buffer_length){
		if      (buffer[offset-1] > 1      ) offset += 3;
		else if (buffer[offset-2]          ) offset += 2;
		else if (buffer[offset-3]|(buffer[offset-1]-1)) offset++;
		else {break;}
	}

	// 寻找不到开始标记
	if (buffer_length <= offset){
		offset =buffer_length -1;
		return false;
	}

	uint8_t nal_head =buffer[offset++];
	header->forbidden_bit =(nal_head & 0x80) >> 7;
	header->nal_reference_bit =(nal_head & 0x60) >> 5;
	header->nal_unit_type =(nal_head & 0x1f) >> 0;
	header->nal_ptr =buffer +offset -4;
	header->nal_length = buffer_length - (offset -4);


	offset += 4;
	// 需找下一个NALU的起始标记
	while (offset < buffer_length){
		if      (buffer[offset-1] > 1      ) offset += 3;
		else if (buffer[offset-2]          ) offset += 2;
		else if (buffer[offset-3]|(buffer[offset-1]-1)) offset++;
		else {break;}
	}
	// 寻找到下一NALU的开始标记
	if (buffer_length > offset){
		header->nal_length = buffer +offset -4 - header->nal_ptr;
	}

	return true;
}

bool H264Parse::FindNAL(uint8_t * buffer, uint32_t buffer_length, NALHeader * header, uint32_t *use_length)
{
	*use_length =0;
	if (!buffer || buffer_length < 4 || !header){
		return false;
	}

	uint32_t offset =3;		// 消费字节游标
	//				 offset
	//  0    1    2    3    4    5    6    7   |   8    9    10   11   12   13      
	// 0x00 0x00 0x01 0xXX 0xXX 0xXX 0xXX 0x00 |  0x00 0x00 0x01 0xXX 0xXX 0xXX 

	// 需找起始标记
	while (offset < buffer_length){
		if      (buffer[offset-1] > 1      ) offset += 3;
		else if (buffer[offset-2]          ) offset += 2;
		else if (buffer[offset-3]|(buffer[offset-1]-1)) offset++;
		else {break;}
	}

	// 寻找不到开始标记
	if (buffer_length <= offset){
		offset =buffer_length -1;
		*use_length =offset +1;
		return false;
	}

	uint8_t nal_head =buffer[offset++];
	header->forbidden_bit =(nal_head & 0x80) >> 7;
	header->nal_reference_bit =(nal_head & 0x60) >> 5;
	header->nal_unit_type =(nal_head & 0x1f) >> 0;
	header->nal_ptr =buffer +offset -4;

	// 需找结束标记
	while (offset < buffer_length){
		if      (buffer[offset-1] > 1      ) offset += 3;
		else if (buffer[offset-2]          ) offset += 2;
		else if (buffer[offset-3]|(buffer[offset-1]-1)) offset++;
		else {break;}
	}

	// 码流结束
	if (buffer_length <= offset){
		offset =buffer_length -1;
	}else{
		offset -=4;
	}

	header->nal_length =buffer +offset -header->nal_ptr +1;
	*use_length =offset +1;
	return true;
}

// 去除0x03特殊标记
bool H264Parse::GetRBSP(NALHeader * header, NalRbsp * rbsp)
{
	if (NULL == rbsp){
		return false;
	}

	// 排除00 00 01 标记及nal头部分
	uint8_t *ptr =header->nal_ptr +4;
	uint32_t length =header->nal_length -4;

	uint32_t offset =0, offset2 =0;
	uint8_t state =0;
	while (offset < length){
		uint8_t temp =ptr[offset++];
		if(0 == state){if (0 == temp){state =1;}else{state =0;}
		rbsp->rbsp_[offset2++] =temp;
		}else if (1 == state){if (0 == temp){state =2;}else{state =0;}
		rbsp->rbsp_[offset2++] =temp;
		}else if (2 == state){
			if (3 != temp){
				rbsp->rbsp_[offset2++] =temp;
			}
			state =0;
		}
	}
	rbsp->length_ =offset2;
	return true;
}

bool H264Parse::GetSeqParameterSet(NalRbsp * rbsp, SPS * sps)
{
	if(NULL == rbsp || NULL == sps) {
		return false;
	}

	GetBitContext gb;
	init_get_bits(&gb, rbsp->rbsp_, 8 *rbsp->length_);

	unsigned int sps_id;

	sps->profile_idc= get_bits(&gb, 8);				    // u(8) 
	sps->constraint_set_flags |= get_bits1(&gb) << 0;   // u(1)
	sps->constraint_set_flags |= get_bits1(&gb) << 1;   // u(1)
	sps->constraint_set_flags |= get_bits1(&gb) << 2;   // u(1)
	sps->constraint_set_flags |= get_bits1(&gb) << 3;   // u(1)
	get_bits(&gb, 4); // reserved				        // u(4) 
	sps->level_idc= get_bits(&gb, 8);				    // u(8) 
	sps_id= get_ue_golomb_31(&gb);				        // ue(v) 

	if(sps_id >= MAX_SPS_COUNT) {
		return false;
	}

	sps->time_offset_length = 24;

	memset(sps->scaling_matrix4, 16, sizeof(sps->scaling_matrix4));
	memset(sps->scaling_matrix8, 16, sizeof(sps->scaling_matrix8));
	sps->scaling_matrix_present = 0;

	if(sps->profile_idc == 100 || sps->profile_idc == 110 || 
		sps->profile_idc == 122 || sps->profile_idc == 144){ //high profile
		sps->chroma_format_idc= get_ue_golomb_31(&gb);
		if(sps->chroma_format_idc == 3)
			sps->residual_color_transform_flag = get_bits1(&gb);
		sps->bit_depth_luma   = get_ue_golomb(&gb) + 8;
		sps->bit_depth_chroma = get_ue_golomb(&gb) + 8;
		sps->transform_bypass = get_bits1(&gb);
		decode_scaling_matrices(&gb, sps, NULL, 1, sps->scaling_matrix4, sps->scaling_matrix8);
	}else{
		sps->chroma_format_idc= 1;
		sps->bit_depth_luma   = 8;
		sps->bit_depth_chroma = 8;
	}

	sps->log2_max_frame_num= get_ue_golomb(&gb) + 4;
	sps->poc_type= get_ue_golomb_31(&gb);

	if(sps->poc_type == 0){ //FIXME #define
		sps->log2_max_poc_lsb= get_ue_golomb(&gb) + 4;
	} else if(sps->poc_type == 1){//FIXME #define
		sps->delta_pic_order_always_zero_flag= get_bits1(&gb);
		sps->offset_for_non_ref_pic= get_se_golomb(&gb);
		sps->offset_for_top_to_bottom_field= get_se_golomb(&gb);
		sps->poc_cycle_length                = get_ue_golomb(&gb);

		if((unsigned)sps->poc_cycle_length >= FF_ARRAY_ELEMS(sps->offset_for_ref_frame)){
			return false;
		}

		for(int i=0; i<sps->poc_cycle_length; i++)
			sps->offset_for_ref_frame[i]= get_se_golomb(&gb);
	}else if(sps->poc_type != 2){
		return false;
	}

	sps->ref_frame_count= get_ue_golomb_31(&gb);
	if(sps->ref_frame_count > MAX_PICTURE_COUNT-2 || sps->ref_frame_count >= 32U){
		return false;
	}
	sps->gaps_in_frame_num_allowed_flag= get_bits1(&gb);
	sps->mb_width = get_ue_golomb(&gb) + 1;
	sps->mb_height= get_ue_golomb(&gb) + 1;
	if((unsigned)sps->mb_width >= INT_MAX/16 || (unsigned)sps->mb_height >= INT_MAX/16){
			return false;
	}

	sps->frame_mbs_only_flag= get_bits1(&gb);
	if(!sps->frame_mbs_only_flag)
		sps->mb_aff= get_bits1(&gb);
	else
		sps->mb_aff= 0;

	sps->direct_8x8_inference_flag= get_bits1(&gb);
	if(!sps->frame_mbs_only_flag && !sps->direct_8x8_inference_flag){
		return false;
	}

	sps->crop= get_bits1(&gb);
	if(sps->crop){
		int crop_vertical_limit   = sps->chroma_format_idc  & 2 ? 16 : 8;
		int crop_horizontal_limit = sps->chroma_format_idc == 3 ? 16 : 8;
		sps->crop_left  = get_ue_golomb(&gb);
		sps->crop_right = get_ue_golomb(&gb);
		sps->crop_top   = get_ue_golomb(&gb);
		sps->crop_bottom= get_ue_golomb(&gb);
	}else{
		sps->crop_left  =
			sps->crop_right =
			sps->crop_top   =
			sps->crop_bottom= 0;
	}

	sps->vui_parameters_present_flag= get_bits1(&gb);
	if( sps->vui_parameters_present_flag )
		if (decode_vui_parameters(&gb, sps) < 0)
			return false;

	if(!sps->sar.den)
		sps->sar.den= 1;
	return true;
}

// 获取帧尺寸
bool H264Parse::GetFrameSize(SPS sps, uint16_t *width, uint16_t *height)
{
	uint32_t dwCorpUnitX,dwCropUnitY;
	if(0 == sps.chroma_format_idc){
		dwCorpUnitX =1;dwCropUnitY =2-sps.frame_mbs_only_flag;
	}else if(1 == sps.chroma_format_idc){
		dwCorpUnitX =2;dwCropUnitY =2*(2-sps.frame_mbs_only_flag);
	}else if(2 == sps.chroma_format_idc){
		dwCorpUnitX =2;dwCropUnitY =1*(2-sps.frame_mbs_only_flag);
	}else if(3 == sps.chroma_format_idc){
		dwCorpUnitX =1;dwCropUnitY =1*(2-sps.frame_mbs_only_flag);
	}else{
		return false;
	}
	*width =((sps.mb_width*16-(dwCorpUnitX*sps.crop_right+1)) -dwCorpUnitX*sps.crop_left +1);
	*height =(( 16 *  ( 2-sps.frame_mbs_only_flag ) * sps.mb_height )-( dwCropUnitY * sps.crop_bottom + 1 )) -dwCropUnitY * sps.crop_top +1;
	return true;
}


// 解释分辨率
bool H264Parse::GetResolution(uint8_t* buffer, uint32_t buffer_lenght, uint16_t* width, uint16_t* height)
{
	NALHeader head;
	memset(&head, 0, sizeof(head));

	if (!H264Parse::CheckNal((uint8_t*)buffer, buffer_lenght, &head))
	{
		return false;
	}
	
	if (head.nal_unit_type != 7)
	{
		return false;

		/*buffer += head.nal_length;
		buffer_lenght -= head.nal_length;
		return GetResolution(buffer + head.nal_length, buffer_lenght - head.nal_length, width, height);*/
	}

	NalRbsp rbsp(4096);
	if (H264Parse::GetRBSP(&head, &rbsp))
	{
		SPS sps;
		if (H264Parse::GetSeqParameterSet(&rbsp, &sps))
		{
			uint16_t w = 0;
			uint16_t h = 0;
			if (H264Parse::GetFrameSize(sps, &w, &h))
			{
				printf("============================ %dx%d\n", w, h);

				if (width)  *width = w;
				if (height) *height = h;
				return true;
			}
		}
	}

	return false;
}
