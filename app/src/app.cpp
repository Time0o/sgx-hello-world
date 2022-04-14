#include <iostream>

#include "sgx_enclave.h"

#include "enclave_u.h"

int main()
{
    SGXEnclave e {
        "enclave",
        "enclave/bin/enclave.signed.so",
        "enclave/enclave.token"
    };

    int random_number;
    e.call(generate_random_number, &random_number);

    std::cout << random_number << std::endl;

    return 0;
}
