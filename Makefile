#
# Copyright (C) 2011-2016 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#

# SGX settings

SGX_SDK            := /opt/sgxsdk
SGX_INCLUDE_PATH   := $(SGX_SDK)/include
SGX_LIBRARY_PATH   := $(SGX_SDK)/lib64
SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
SGX_EDGER8R        := $(SGX_SDK)/bin/x64/sgx_edger8r

# Enclave settings

ENCLAVE_DIR           := enclave
ENCLAVE_SRC_DIR       := $(ENCLAVE_DIR)/src
ENCLAVE_TRUSTED_DIR   := $(ENCLAVE_DIR)/trusted
ENCLAVE_UNTRUSTED_DIR := $(ENCLAVE_DIR)/untrusted
ENCLAVE_OUT_DIR       := $(ENCLAVE_DIR)/out
ENCLAVE_BIN_DIR       := $(ENCLAVE_DIR)/bin
ENCLAVE_INCLUDES      := -I$(ENCLAVE_DIR)/include \
                         -I$(SGX_SDK)/include \
                         -I$(SGX_SDK)/include/tlibc \
 			 -I$(ENCLAVE_TRUSTED_DIR)
ENCLAVE_CFLAGS        := -nostdinc -fvisibility=hidden -fpie -fstack-protector
ENCLAVE_LDFLAGS       := -L$(SGX_LIBRARY_PATH) \
                         -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles \
                         -Wl,--whole-archive -lsgx_trts_sim -Wl,--no-whole-archive \
                         -Wl,--start-group -lsgx_tstdc -lsgx_tcrypto -lsgx_tkey_exchange -lsgx_tservice_sim -Wl,--end-group \
                         -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
                         -Wl,-pie,-eenclave_entry -Wl,--export-dynamic\
                         -Wl,--defsym,__ImageBase=0

# Protobuf settings

PROTOC         := protoc
PROTO_DIR      := proto
PROTO_OUT_DIR  := $(PROTO_DIR)/out
PROTO_INCLUDES := -I$(PROTO_OUT_DIR)
PROTO_CXXFLAGS := -fPIC -O2
PROTO_LDFLAGS  := -lprotobuf -lgrpc++ -lgrpc++_reflection

# App settings

APP_DIR      := app
APP_SRC_DIR  := $(APP_DIR)/src
APP_OUT_DIR  := $(APP_DIR)/out
APP_BIN_DIR  := $(APP_DIR)/bin
APP_INCLUDES := -I$(APP_DIR)/include \
                -I$(SGX_SDK)/include \
		-I$(ENCLAVE_UNTRUSTED_DIR) \
		$(PROTO_INCLUDES)
APP_CXXFLAGS := -fPIC -std=c++17 -O2
APP_LDFLAGS  := -L$(SGX_LIBRARY_PATH) \
		-lsgx_urts_sim -lsgx_ukey_exchange -lsgx_uae_service_sim \
		-lcryptopp \
		-lpthread \
		$(PROTO_LDFLAGS)

# Build rules

all: $(ENCLAVE_BIN_DIR)/enclave.signed.so $(APP_BIN_DIR)/client $(APP_BIN_DIR)/server

# Build enclave

$(ENCLAVE_TRUSTED_DIR)/enclave_t.c: $(ENCLAVE_DIR)/enclave.edl
	$(SGX_EDGER8R) --trusted $< --trusted-dir $(ENCLAVE_TRUSTED_DIR) --search-path $(SGX_INCLUDE_PATH)

$(ENCLAVE_UNTRUSTED_DIR)/enclave_u.c: $(ENCLAVE_DIR)/enclave.edl
	$(SGX_EDGER8R) --untrusted $< --untrusted-dir $(ENCLAVE_UNTRUSTED_DIR) --search-path $(SGX_SDK)/include

$(ENCLAVE_OUT_DIR)/enclave_t.o: $(ENCLAVE_TRUSTED_DIR)/enclave_t.c
	$(CC) $(ENCLAVE_CFLAGS) $(ENCLAVE_INCLUDES) -c $< -o $@

$(ENCLAVE_OUT_DIR)/enclave_u.o: $(ENCLAVE_UNTRUSTED_DIR)/enclave_u.c
	$(CC) $(ENCLAVE_CFLAGS) $(ENCLAVE_INCLUDES) -c $< -o $@

$(ENCLAVE_OUT_DIR)/enclave.o: $(ENCLAVE_SRC_DIR)/enclave.c $(ENCLAVE_TRUSTED_DIR)/enclave_t.c
	$(CC) $(ENCLAVE_CFLAGS) $(ENCLAVE_INCLUDES) -c $< -o $@

$(ENCLAVE_BIN_DIR)/enclave.so: $(ENCLAVE_OUT_DIR)/enclave.o $(ENCLAVE_OUT_DIR)/enclave_t.o 
	$(CC) $^ -o $@ $(ENCLAVE_LDFLAGS)

$(ENCLAVE_BIN_DIR)/enclave.signed.so: $(ENCLAVE_BIN_DIR)/enclave.so
	$(SGX_ENCLAVE_SIGNER) sign -key $(ENCLAVE_DIR)/enclave_private.pem -enclave $< -out $@ -config $(ENCLAVE_DIR)/enclave.config.xml

# Protobuf files

$(PROTO_OUT_DIR)/sgx_ra.pb.cc: $(PROTO_DIR)/sgx_ra.proto
	$(PROTOC) --cpp_out=$(PROTO_OUT_DIR) --proto_path=$(PROTO_DIR) $<

$(PROTO_OUT_DIR)/sgx_ra.grpc.pb.cc: $(PROTO_DIR)/sgx_ra.proto
	$(PROTOC) --plugin=grpc_cpp_plugin --grpc_out=$(PROTO_OUT_DIR) --proto_path=$(PROTO_DIR) $<

$(PROTO_OUT_DIR)/sgx_ra.pb.o: $(PROTO_OUT_DIR)/sgx_ra.pb.cc
	$(CXX) $(PROTO_CXXFLAGS) $(PROTO_INCLUDES) -c $< -o $@

$(PROTO_OUT_DIR)/sgx_ra.grpc.pb.o: $(PROTO_OUT_DIR)/sgx_ra.grpc.pb.cc
	$(CXX) $(PROTO_CXXFLAGS) $(PROTO_INCLUDES) -c $< -o $@

# Build client

$(APP_OUT_DIR)/client.o: $(APP_SRC_DIR)/client.cpp $(ENCLAVE_UNTRUSTED_DIR)/enclave_u.c $(PROTO_OUT_DIR)/sgx_ra.pb.cc $(PROTO_OUT_DIR)/sgx_ra.grpc.pb.cc
	$(CXX) $(APP_CXXFLAGS) $(APP_INCLUDES) -c $< -o $@

$(APP_BIN_DIR)/client: $(APP_OUT_DIR)/client.o $(ENCLAVE_OUT_DIR)/enclave_u.o $(PROTO_OUT_DIR)/sgx_ra.pb.o $(PROTO_OUT_DIR)/sgx_ra.grpc.pb.o
	$(CXX) $^ -o $@ $(APP_LDFLAGS)

# Build server

$(APP_OUT_DIR)/server.o: $(APP_SRC_DIR)/server.cpp $(PROTO_OUT_DIR)/sgx_ra.pb.cc $(PROTO_OUT_DIR)/sgx_ra.grpc.pb.cc
	$(CXX) $(APP_CXXFLAGS) $(APP_INCLUDES) -c $< -o $@

$(APP_BIN_DIR)/server: $(APP_OUT_DIR)/server.o $(PROTO_OUT_DIR)/sgx_ra.pb.o $(PROTO_OUT_DIR)/sgx_ra.grpc.pb.o
	$(CXX) $^ -o $@ $(APP_LDFLAGS)

# Clean build

.PHONY: clean

clean:
	@$(RM) $(ENCLAVE_OUT_DIR)/* \
	       $(ENCLAVE_TRUSTED_DIR)/* \
	       $(ENCLAVE_UNTRUSTED_DIR)/* \
	       $(ENCLAVE_BIN_DIR)/* \
	       $(PROTO_OUT_DIR)/* \
	       $(APP_OUT_DIR)/* \
	       $(APP_BIN_DIR)/*
