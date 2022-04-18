#pragma once

#include <stdexcept>
#include <string>

#include "sgx_edger8r.h"

class SGXError : public std::runtime_error
{
public:
    SGXError(std::string const &what, sgx_status_t err)
        : std::runtime_error { what + " (" + str(err) + ")" }
    {}

private:
    static std::string str(sgx_status_t err)
    {
        switch(err)
        {
        case SGX_ERROR_UNEXPECTED:
            return "unexpected error";
        case SGX_ERROR_INVALID_PARAMETER:
            return "invalid parameter";
        case SGX_ERROR_OUT_OF_MEMORY:
            return "out of memory";
        case SGX_ERROR_ENCLAVE_LOST:
            return "power transition occurred";
        case SGX_ERROR_INVALID_ENCLAVE:
            return "invalid enclave image";
        case SGX_ERROR_INVALID_ENCLAVE_ID:
            return "invalid enclave identification";
        case SGX_ERROR_INVALID_SIGNATURE:
            return "invalid enclave signature";
        case SGX_ERROR_OUT_OF_EPC:
            return "out of EPC memory";
        case SGX_ERROR_NO_DEVICE:
            return "invalid SGX device";
        case SGX_ERROR_MEMORY_MAP_CONFLICT:
            return "memory map conflicted";
        case SGX_ERROR_INVALID_METADATA:
            return "invalid enclave metadata";
        case SGX_ERROR_DEVICE_BUSY:
            return "SGX device was busy";
        case SGX_ERROR_INVALID_VERSION:
            return "enclave version was invalid";
        case SGX_ERROR_INVALID_ATTRIBUTE:
            return "enclave was not authorized";
	case SGX_ERROR_ENCLAVE_FILE_ACCESS:
            return "can't open enclave file";
        default:
            return "Unknown error";
        }
    }
};

inline void sgx_check(std::string const &what, sgx_status_t err)
{
    if (err != SGX_SUCCESS)
        throw SGXError { "SGX error: failed to " + what, err };
}
