# Build Secret Provisioning client/server example (backed by RA-TLS) as follows:
#
# - make          -- build SGX version with apps built in release mode and no logs
# - make DEBUG=1  -- build SGX version with apps built in debug mode and with logs
#
# Any of these invocations clones mbedTLS' git repository (version 2.21.0) and
# builds it in default configuration. Also, server and client programs are
# built. See README for details.
#
# Use `make clean` to remove Graphene-generated files and `make distclean` to
# additionally remove the cloned mbedTLS git repository.

# Relative path to Graphene root
GRAPHENEDIR ?= ../..
GRAPHENEKEY ?= $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/enclave-key.pem

# Specify your SPID and linkable/unlinkable attestation policy (only for EPID);
# these variables will not be used if this example is built for ECDSA/DCAP
RA_CLIENT_SPID ?= AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
RA_CLIENT_LINKABLE ?= 0

ifeq ($(DEBUG),1)
GRAPHENEDEBUG=inline
CFLAGS += -O0 -ggdb3
else
GRAPHENEDEBUG=none
CFLAGS += -O2
endif

.PHONY: all
all: app epid  # by default, only build EPID because it doesn't rely on additional (DCAP) libs

.PHONY: app
app: mbedtls/CMakeLists.txt pal_loader \
     secret_prov_min_client.manifest.sgx secret_prov_client.manifest.sgx

.PHONY: epid
epid: secret_prov_server_epid libsecret_prov_attest.so libsecret_prov_verify_epid.so

.PHONY: dcap
dcap: secret_prov_server_dcap libsecret_prov_attest.so libsecret_prov_verify_dcap.so

############################# MBEDTLS DEPENDENCY ##############################

MBEDTLS_VERSION ?= 2.21.0
MBEDTLS_SRC ?= mbedtls-$(MBEDTLS_VERSION).tar.gz
MBEDTLS_URI ?= https://github.com/ARMmbed/mbedtls/archive/
MBEDTLS_CHECKSUM ?= 320e930b7596ade650ae4fc9ba94b510d05e3a7d63520e121d8fdc7a21602db9

# mbedTLS uses a submodule mbedcrypto, need to download it and move under mbedtls/crypto
MBEDCRYPTO_VERSION ?= 3.1.0
MBEDCRYPTO_SRC ?= mbedcrypto-$(MBEDCRYPTO_VERSION).tar.gz
MBEDCRYPTO_URI ?= https://github.com/ARMmbed/mbed-crypto/archive/
MBEDCRYPTO_CHECKSUM ?= 7e171df03560031bc712489930831e70ae4b70ff521a609c6361f36bd5f8b76b

ifeq ($(DEBUG),1)
MBED_BUILD_TYPE=Debug
else
MBED_BUILD_TYPE=Release
endif

$(MBEDTLS_SRC):
	$(GRAPHENEDIR)/Scripts/download --output $@ --url $(MBEDTLS_URI)/$(MBEDTLS_SRC) \
		--sha256 $(MBEDTLS_CHECKSUM)

$(MBEDCRYPTO_SRC):
	$(GRAPHENEDIR)/Scripts/download --output $@ --url $(MBEDCRYPTO_URI)/$(MBEDCRYPTO_SRC) \
		--sha256 $(MBEDCRYPTO_CHECKSUM)

mbedtls/CMakeLists.txt: $(MBEDTLS_SRC) $(MBEDCRYPTO_SRC)
	tar -mxzf $(MBEDTLS_SRC)
	tar -mxzf $(MBEDCRYPTO_SRC)
	mv mbedtls-mbedtls-$(MBEDTLS_VERSION) mbedtls
	$(RM) -r mbedtls/crypto
	mv mbed-crypto-mbedcrypto-$(MBEDCRYPTO_VERSION) mbedtls
	mv mbedtls/mbed-crypto-mbedcrypto-$(MBEDCRYPTO_VERSION) mbedtls/crypto
	mkdir mbedtls/install
	cd mbedtls && ./scripts/config.pl set MBEDTLS_CMAC_C && make SHARED=1 DESTDIR=install install .

######################### CLIENT/SERVER EXECUTABLES ###########################

CFLAGS += -Wall -std=c11 -I$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/tools/ra-tls
LFLAGS += -Wl,-rpath,. -L. -lmbedcrypto -lmbedtls -lmbedx509

secret_prov_server_epid: src/secret_prov_server.c libsecret_prov_verify_epid.so \
                         libmbedcrypto.so libmbedtls.so libmbedx509.so
	$(CC) $< $(CFLAGS) $(LFLAGS) -lsecret_prov_verify_epid -pthread -o $@

# linker option --no-as-needed is required because SGX DCAP library (libsgx_dcap_quoteverify.so)
# does dlopen() instead of directly linking against libsgx_urts.so, and without this option
# compilers remove the "seemingly unused" libsgx_urts.so
secret_prov_server_dcap: src/secret_prov_server.c libsecret_prov_verify_dcap.so \
                         libmbedcrypto.so libmbedtls.so libmbedx509.so
	$(CC) $< $(CFLAGS) $(LFLAGS) -Wl,--no-as-needed -lsgx_urts -lsecret_prov_verify_dcap -pthread -o $@

secret_prov_client: src/secret_prov_client.c libsecret_prov_attest.so \
                    libmbedcrypto.so libmbedtls.so libmbedx509.so
	$(CC) $< $(CFLAGS) $(LFLAGS) -lsecret_prov_attest -o $@

secret_prov_min_client: src/secret_prov_min_client.c libmbedcrypto.so libmbedtls.so libmbedx509.so
	$(CC) $< $(CFLAGS) $(LFLAGS) -o $@

########################### COPIES FOR CONVENIENCE ############################

pal_loader:
	ln -s $(GRAPHENEDIR)/Runtime/$@ .

libmbedcrypto.so: mbedtls/CMakeLists.txt
	cp mbedtls/install/lib/$@.* .
	ln -s $@.* $@

libmbedtls.so: mbedtls/CMakeLists.txt
	cp mbedtls/install/lib/$@.* .
	ln -s $@.* $@

libmbedx509.so: mbedtls/CMakeLists.txt
	cp mbedtls/install/lib/$@.* .
	ln -s $@.* $@

libsgx_util.so:
	cp $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/tools/common/$@ .

libsecret_prov_attest.so:
	cp $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/tools/ra-tls/$@ .

libsecret_prov_verify_epid.so: libmbedcrypto.so libmbedx509.so libmbedtls.so libsgx_util.so
	cp $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/tools/ra-tls/$@ .

libsecret_prov_verify_dcap.so: libmbedcrypto.so libmbedx509.so libmbedtls.so libsgx_util.so
	cp $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/tools/ra-tls/$@ .

############################### CLIENT MANIFEST ###############################

secret_prov_client.manifest: secret_prov_client.manifest.template
	sed -e 's|$$(GRAPHENEDIR)|'"$(GRAPHENEDIR)"'|g' \
		-e 's|$$(GRAPHENEDEBUG)|'"$(GRAPHENEDEBUG)"'|g' \
		-e 's|$$(RA_CLIENT_SPID)|'"$(RA_CLIENT_SPID)"'|g' \
		-e 's|$$(RA_CLIENT_LINKABLE)|'"$(RA_CLIENT_LINKABLE)"'|g' \
		$< > $@

secret_prov_client.manifest.sgx: secret_prov_client.manifest secret_prov_client libsecret_prov_attest.so
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-sign \
		-libpal $(GRAPHENEDIR)/Runtime/libpal-Linux-SGX.so \
		-key $(GRAPHENEKEY) \
		-manifest $< -output $@ \
		-exec secret_prov_client
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-get-token \
		-output secret_prov_client.token -sig secret_prov_client.sig

############################# MIN CLIENT MANIFEST #############################

secret_prov_min_client.manifest: secret_prov_min_client.manifest.template
	sed -e 's|$$(GRAPHENEDIR)|'"$(GRAPHENEDIR)"'|g' \
		-e 's|$$(GRAPHENEDEBUG)|'"$(GRAPHENEDEBUG)"'|g' \
		-e 's|$$(RA_CLIENT_SPID)|'"$(RA_CLIENT_SPID)"'|g' \
		-e 's|$$(RA_CLIENT_LINKABLE)|'"$(RA_CLIENT_LINKABLE)"'|g' \
		$< > $@

secret_prov_min_client.manifest.sgx: secret_prov_min_client.manifest secret_prov_min_client libsecret_prov_attest.so
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-sign \
		-libpal $(GRAPHENEDIR)/Runtime/libpal-Linux-SGX.so \
		-key $(GRAPHENEKEY) \
		-manifest $< -output $@ \
		-exec secret_prov_min_client
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-get-token \
		-output secret_prov_min_client.token -sig secret_prov_min_client.sig

############################# SGX CHECKS FOR CI ###############################

.PHONY: check_epid
check_epid: app epid
	./secret_prov_server_epid >/dev/null & SERVER_ID=$$!; \
	sleep 3; \
	SGX=1 ./pal_loader ./secret_prov_min_client > OUTPUT; \
	SGX=1 ./pal_loader ./secret_prov_client >> OUTPUT; \
	kill -9 $$SERVER_ID;
	@grep "Received secret = 'This is a secret string!'" OUTPUT && echo "[ Success 1/2 ]"
	@grep "Received secret1 = 'This is a secret string!', secret2 = '42'" OUTPUT && echo "[ Success 2/2 ]"
	@rm OUTPUT

.PHONY: check_dcap
check_dcap: app dcap
	./secret_prov_server_dcap >/dev/null & SERVER_ID=$$!; \
	sleep 3; \
	SGX=1 ./pal_loader ./secret_prov_min_client > OUTPUT; \
	SGX=1 ./pal_loader ./secret_prov_client >> OUTPUT; \
	kill -9 $$SERVER_ID;
	@grep "Received secret = 'This is a secret string!'" OUTPUT && echo "[ Success 1/2 ]"
	@grep "Received secret1 = 'This is a secret string!', secret2 = '42'" OUTPUT && echo "[ Success 2/2 ]"
	@rm OUTPUT

################################## CLEANUP ####################################

.PHONY: clean
clean:
	$(RM) *.token *.sig *.manifest.sgx *.manifest *.so *.so.* OUTPUT pal_loader \
          secret_prov_server_epid secret_prov_server_dcap \
          secret_prov_client secret_prov_min_client

.PHONY: distclean
distclean: clean
	$(RM) -r mbedtls *.tar.gz
