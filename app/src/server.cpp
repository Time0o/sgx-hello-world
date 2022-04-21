#include <algorithm>
#include <cstdint>
#include <cstring>

#include <cryptopp/algparam.h>
#include <cryptopp/asn.h>
#include <cryptopp/cmac.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/rijndael.h>
#include <cryptopp/secblock.h>

using namespace CryptoPP;

namespace {
    AutoSeededRandomPool rng;
}

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/grpcpp.h>

#include "sgx_ra.grpc.pb.h"

using namespace sgx_ra;

#include <spdlog/spdlog.h>

class SGXRaServerService : public SGXRaServer::Service
{
private:
    grpc::Status SendMsg0(grpc::ServerContext *context,
                          Msg0 const *msg0,
                          google::protobuf::Empty *) override
    {
        spdlog::info("Received message 0 from client");

        // Assert that extended EPID group ID is zero.
        if (msg0->egid() == 0) {
            spdlog::info("Message 0 contains valid extended EPID group ID");
            return grpc::Status::OK;
        } else {
            spdlog::info("Message 0 contains invalid extended EPID group ID");
            return grpc::Status {
                grpc::StatusCode::INVALID_ARGUMENT,
                "Invalid extended group ID"
            };
        }
    }

    grpc::Status SendMsg1(grpc::ServerContext *context,
                          Msg1 const *msg1,
                          google::protobuf::Empty *) override
    {
        spdlog::info("Received message 1 from client");

        ECDH<ECP>::Domain d { ASN1::secp256r1() };

        // Client's public session key.
        auto g_a_gx { msg1->g_a().gx() };
        std::reverse(g_a_gx.begin(), g_a_gx.end());

        auto g_a_gy { msg1->g_a().gy() };
        std::reverse(g_a_gy.begin(), g_a_gy.end());

        ECP::Point g_a_point {
            Integer { reinterpret_cast<uint8_t const *>(g_a_gx.data()), g_a_gx.size() },
            Integer { reinterpret_cast<uint8_t const *>(g_a_gy.data()), g_a_gy.size() },
        };

        SecByteBlock g_a { d.PublicKeyLength() };

        d.GetGroupParameters().EncodeElement(true, g_a_point, g_a);

        // Server's private session key.
        SecByteBlock g_b { d.PrivateKeyLength() };

        d.GeneratePrivateKey(rng, g_b);

        // Compute shared secret.
        SecByteBlock g_ss { d.AgreedValueLength() };

        if (!d.Agree(g_ss, g_a, g_a)) {
            return grpc::Status {
                grpc::StatusCode::INTERNAL,
                "Failed to compute shared secret"
            };
        }

        std::reverse(g_ss.begin(), g_ss.end());

        // Compute KDK.
        SecByteBlock cmac_kdk_key { nullptr, AES::DEFAULT_KEYLENGTH };

        CMAC<AES> cmac_kdk { cmac_kdk_key };

        SecByteBlock kdk { cmac_kdk.DigestSize() };
        cmac_kdk.CalculateDigest(kdk, g_ss.BytePtr(), g_ss.SizeInBytes());

        // Compute SMK.
        CMAC<AES> cmac_smk { kdk };

        uint8_t cmac_smk_input[] = "\x01SMK\x00\x80\x00";

        SecByteBlock smk { cmac_smk.DigestSize() };
        cmac_smk.CalculateDigest(smk, cmac_smk_input, sizeof(cmac_smk_input));

        return grpc::Status::OK;
    }
};

int main()
{
    grpc::ServerBuilder server_builder;

    server_builder.AddListeningPort("0.0.0.0:8888", // XXX
                                    grpc::InsecureServerCredentials()); // XXX

    SGXRaServerService service;
    server_builder.RegisterService(&service);

    auto server { server_builder.BuildAndStart() };

    server->Wait();
}
