/* -*- indent-tabs-mode: nil -*- */
#include "log/signer.h"

#include <glog/logging.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <stdint.h>

#include "log/verifier.h"
#include "proto/ct.pb.h"
#include "util/util.h"

#if OPENSSL_VERSION_NUMBER < 0x10000000
# error "Need OpenSSL >= 1.0.0"
#endif

namespace ct {

Signer::Signer(EVP_PKEY *pkey)
    : pkey_(CHECK_NOTNULL(pkey)) {
  switch (pkey_->type) {
    case EVP_PKEY_EC:
      hash_algo_ = DigitallySigned::SHA256;
      sig_algo_ = DigitallySigned::ECDSA;
      break;
    default:
      LOG(FATAL) << "Unsupported key type " << pkey_->type;
  }
  key_id_ = Verifier::ComputeKeyID(pkey_);
}

Signer::~Signer() {
  EVP_PKEY_free(pkey_);
}

std::string Signer::KeyID() const {
  return key_id_;
}

void Signer::Sign(const std::string &data,
                  ct::DigitallySigned *signature) const {
  signature->set_hash_algorithm(hash_algo_);
  signature->set_sig_algorithm(sig_algo_);
  signature->set_signature(RawSign(data));
}

Signer::Signer()
    : pkey_(NULL),
      hash_algo_(DigitallySigned::NONE),
      sig_algo_(DigitallySigned::ANONYMOUS) {}

std::string Signer::RawSign(const std::string &data) const {
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);
  // NOTE: this syntax for setting the hash function requires OpenSSL >= 1.0.0.
  CHECK_EQ(1, EVP_SignInit(&ctx, EVP_sha256()));
  CHECK_EQ(1, EVP_SignUpdate(&ctx, data.data(), data.size()));
  unsigned int sig_size = EVP_PKEY_size(pkey_);
  unsigned char *sig = new unsigned char[sig_size];

  CHECK_EQ(1, EVP_SignFinal(&ctx, sig, &sig_size, pkey_));

  EVP_MD_CTX_cleanup(&ctx);
  std::string ret(reinterpret_cast<char*>(sig), sig_size);

  delete[] sig;
  return ret;
}

}  // namespace ct
