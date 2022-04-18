#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <sgx_enclave.h>
#include <sgx_error.h>
#include <sgx_uae_epid.h>
#include <sgx_ukey_exchange.h>

#include "enclave_u.h"

#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

#include "sgx_ra.grpc.pb.h"

using namespace sgx_ra;

#include <spdlog/spdlog.h>

struct SGXRaClient
{
public:
    SGXRaClient(std::shared_ptr<SGXEnclave> enclave,
		std::unique_ptr<SGXRaServer::Stub> &&server)
      : m_enclave { enclave }
      , m_server { std::move(server) }
    {
        ra_init();
    }

    ~SGXRaClient()
    {
        ra_close();
    }

    void attest() const
    {
        ra_attest();
    }

private:
    void ra_init()
    {
        sgx_check("init remote attestation",
                  m_enclave->call(enclave_ra_init, &m_context));
    }

    void ra_close() const
    {
        m_enclave->call(enclave_ra_close, m_context);
    }

    void ra_attest() const
    {
        spdlog::info("Performing remote attestation");

        // Send message 0.
        uint32_t msg0_extended_epid_group_id;
        sgx_check("get extended EPID group ID",
                  sgx_get_extended_epid_group_id(&msg0_extended_epid_group_id));

        Msg0 msg0;
        msg0.set_extended_epid_group_id(msg0_extended_epid_group_id);

        spdlog::info("Sending message 0 to server");

        rpc("send Msg0",
            &SGXRaServer::Stub::SendMsg0, msg0, &rpc_empty);
    }

    template<typename FUNC, typename REQUEST, typename RESPONSE>
    void rpc(std::string const &what,
             FUNC &&func,
             REQUEST const &request,
             RESPONSE *response) const
    {
        grpc::ClientContext context;

        auto status = ((*m_server).*func)(&context, request, response);

        rpc_check(what, status);
    }

    static inline google::protobuf::Empty rpc_empty;

    static void rpc_check(std::string const &what, grpc::Status const &status)
    {
        if (status.ok())
            return;

        auto error_code = status.error_code();
        auto error_message = status.error_message();

        std::stringstream ss;
        ss << "RPC error: failed to " << what << ": status code " << error_code;

        if (!error_message.empty())
            ss << " (" << error_message << ")";

        throw std::runtime_error { ss.str() };
    }

    std::shared_ptr<SGXEnclave> m_enclave;
    std::unique_ptr<SGXRaServer::Stub> m_server;
    sgx_ra_context_t m_context;
};

int main()
{
    // Initialize enclave.
    std::shared_ptr<SGXEnclave> enclave;
    try {
        enclave = std::make_shared<SGXEnclave>(
            "enclave",
            "enclave/bin/enclave.signed.so",
            "enclave/enclave.token");

    } catch (std::exception const &e) {
        spdlog::error("Failed to initialize enclave: {}", e.what());
        return EXIT_FAILURE;
    }

    // Initialize remote attestation server stub.
    std::unique_ptr<SGXRaServer::Stub> server;

    try {
        auto server_channel {
            grpc::CreateChannel("0.0.0.0:8888", // XXX
                                grpc::InsecureChannelCredentials()) // XXX
        };

        server = SGXRaServer::NewStub(server_channel);

    } catch (std::exception const &e) {
        spdlog::error("Failed to initialize remote attestation server stub: {}", e.what());
        return EXIT_FAILURE;
    }

    // Perform remote attestation.
    try {
        SGXRaClient client { enclave, std::move(server) };

        client.attest();

    } catch (std::exception const &e) {
        spdlog::error("Remote attestation failed: {}", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
