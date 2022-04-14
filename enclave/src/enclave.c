#include "sgx_trts.h"

#include "enclave_t.h"

int generate_random_number()
{
    int random_number;

    sgx_read_rand((uint8_t *)&random_number, sizeof(int));

    return random_number;
}
