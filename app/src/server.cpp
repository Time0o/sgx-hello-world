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
        if (msg0->extended_epid_group_id() == 0) {
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
