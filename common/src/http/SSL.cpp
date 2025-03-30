#include "http/SSL.h"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include <openssl/ssl.h>
#include <spdlog/spdlog.h>

namespace mithril::http {

namespace internal {
SSL_CTX* SSLCtx = nullptr;
}


namespace {

void SSLKeyLogFunction(const SSL* /*ssl*/, const char* line) {
    FILE* fp;
    fp = fopen("key_log.log", "a");
    if (fp == nullptr) {
        spdlog::error("failed to open ssl key log file");
        return;
    }
    fprintf(fp, "%s\n", line);
    fclose(fp);
}

}  // namespace

void InitializeSSL() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    auto* ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == nullptr) {
        throw std::runtime_error("Failed to create SSL context");
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_verify_depth(ctx, 4);
    SSL_CTX_set_default_verify_paths(ctx);

    uint64_t options = 0;
    //options |= SSL_OP_IGNORE_UNEXPECTED_EOF;  // There are many non-compliant servers that will close the connection
                                              // without performing SSL teardown.
    //options |= SSL_OP_ENABLE_KTLS;            // Make an effort to use kTLS offload when possible.

    SSL_CTX_set_options(ctx, options);

#if defined(CRAWLER_DEBUG_SSL)
    // Write key information to a file so we can inspect https requests with
    // Wireshark
    SSL_CTX_set_keylog_callback(ctx, SSLKeyLogFunction);
#endif

    internal::SSLCtx = ctx;
}

void DeinitializeSSL() {
    if (internal::SSLCtx != nullptr) {
        SSL_CTX_free(internal::SSLCtx);
        internal::SSLCtx = nullptr;
    }
}

}  // namespace mithril::http
