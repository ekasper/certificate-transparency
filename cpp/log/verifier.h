// A base class for verifying signatures of unstructured data.  This class is
// mockable.

#ifndef SRC_LOG_VERIFIER_H_
#define SRC_LOG_VERIFIER_H_

#include <openssl/evp.h>
#include <openssl/x509.h>  // for i2d_PUBKEY
#include <stdint.h>

#include "proto/ct.pb.h"

namespace ct {

class Verifier {
 public:
  enum Status {
    OK,
    HASH_ALGORITHM_MISMATCH,
    SIGNATURE_ALGORITHM_MISMATCH,
    INVALID_SIGNATURE,
  };

  explicit Verifier(EVP_PKEY *pkey);
  virtual ~Verifier();

  virtual std::string KeyID() const;

  virtual Status Verify(const std::string &input,
                        const ct::DigitallySigned &signature) const;

  static std::string ComputeKeyID(EVP_PKEY *pkey);

 protected:
  // A constructor for mocking.
  Verifier();

 private:
  bool RawVerify(const std::string &data, const std::string &sig_string) const;

  EVP_PKEY *pkey_;
  DigitallySigned::HashAlgorithm hash_algo_;
  DigitallySigned::SignatureAlgorithm sig_algo_;
  std::string key_id_;
};

}  // namespace ct

#endif  // SRC_LOG_VERIFIER_H_
