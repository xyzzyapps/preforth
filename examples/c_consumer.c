#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#define PF_IMPORT __declspec(dllimport)
#else
#define PF_IMPORT
#endif

PF_IMPORT int64_t pf_factorial(int64_t n);

int main(void) {
    int64_t input = 10;
    int64_t result = pf_factorial(input);
    printf("C program: called PreForth exported pf_factorial(%lld) = %lld\n",
           (long long)input, (long long)result);
    return (result == 3628800) ? 0 : 1;
}
