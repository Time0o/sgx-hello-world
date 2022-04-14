#pragma once

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "sgx_edger8r.h"
#include "sgx_error.h"
#include "sgx_urts.h"

class SGXEnclave
{
public:
    SGXEnclave(std::string const &name,
               std::string const &file_name,
               std::string const &token_file_name)
      : m_name { name }
    {
        sgx_launch_token_t token = { 0 };
        int token_update { 0 };

        // Try to read launch token from existing file.
        std::fstream token_stream { token_file_name, std::ios::in | std::ios::binary };
        if (token_stream) {
            if (!token_stream.read(reinterpret_cast<char *>(token), sizeof(token))) 
                throw std::runtime_error("Failed to read launch token");
        }

        // Create enclave.
        sgx_check(sgx_create_enclave(file_name.c_str(),
                                     SGX_DEBUG_FLAG,
                                     &token,
                                     &token_update,
                                     &m_id,
                                     nullptr));

        // Try to write updated launch token.
        if (!token_stream || token_update) {
            token_stream.open(token_file_name, std::ios::out | std::ios::binary);
            if (token_stream)
                token_stream.write(reinterpret_cast<char *>(token), sizeof(token));
        }
    }

    std::string name() const
    { return m_name; }

    sgx_enclave_id_t id() const
    { return m_id; }

    template<typename FUNC, typename ...ARGS>
    void call(FUNC &&func, ARGS &&...args) const
    { sgx_check(func(m_id, std::forward<ARGS>(args)...)); }

private:
    std::string m_name;
    sgx_enclave_id_t m_id;
};
