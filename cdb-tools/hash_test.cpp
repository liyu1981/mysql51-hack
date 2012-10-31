#include "MurmurHash3.h"
#include "tfc_md5.h"

#include <string.h>
#include <stdio.h>
#include <sys/time.h>

void
test_tfc_md5(int cycles, const char* data, int data_size)
{
    for (int i=0; i<cycles; ++i) {
        char* md5 = tfc::md5::md5_buf((u_char*)data, data_size);
        free(md5);
    }
}

void
test_murmur_hash_32(int cycles, const char* data, int data_size)
{
    for (int i=0; i<cycles; ++i) {
        int h;
        MurmurHash3_x86_32(data, data_size, 0, &h);
    }
}

void
test_murmur_hash_x86_128(int cycles, const char* data, int data_size)
{
    for (int i=0; i<cycles; ++i) {
        int h[4];
        MurmurHash3_x86_128(data, data_size, 0, &h[0]);
    }
}

void
test_murmur_hash_x64_128(int cycles, const char* data, int data_size)
{
    for (int i=0; i<cycles; ++i) {
        int h[4];
        MurmurHash3_x64_128(data, data_size, 0, &h[0]);
    }
}

int
main(int argc, char* argv[])
{
    int i=0;
    int cycles = atoi(argv[1]);
    struct timeval b, e;

    const char* simple_buf = "12345678";
    int simple_buf_size = sizeof(simple_buf)/sizeof(simple_buf[0]);

    const char* complex_buf = "the quick brown fox jumps over the lazy dog";
    int complex_buf_size = sizeof(complex_buf)/sizeof(complex_buf[0]);

    printf("Test tfc md5 with simple_buf start...");
    gettimeofday(&b, NULL);
    test_tfc_md5(cycles, simple_buf, simple_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    printf("Test murmur hash 32 with simple_buf start...");
    gettimeofday(&b, NULL);
    test_murmur_hash_32(cycles, simple_buf, simple_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    printf("Test murmur hash x86 128 with simple_buf start...");
    gettimeofday(&b, NULL);
    test_murmur_hash_x86_128(cycles, simple_buf, simple_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    printf("Test murmur hash x64 128 with simple_buf start...");
    gettimeofday(&b, NULL);
    test_murmur_hash_x64_128(cycles, simple_buf, simple_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    printf("Test tfc md5 with complex_buf start...");
    gettimeofday(&b, NULL);
    test_tfc_md5(cycles, complex_buf, complex_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    printf("Test murmur hash 32 with complex_buf start...");
    gettimeofday(&b, NULL);
    test_murmur_hash_32(cycles, complex_buf, complex_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    printf("Test murmur hash x86 128 with complex_buf start...");
    gettimeofday(&b, NULL);
    test_murmur_hash_x86_128(cycles, complex_buf, complex_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    printf("Test murmur hash x64 128 with complex_buf start...");
    gettimeofday(&b, NULL);
    test_murmur_hash_x64_128(cycles, complex_buf, complex_buf_size);
    gettimeofday(&e, NULL);
    printf(" done, %d sec %d usec.\n", e.tv_sec-b.tv_sec, e.tv_usec-b.tv_usec);

    return 0;
}

