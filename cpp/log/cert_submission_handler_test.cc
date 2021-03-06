#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <string>

#include "log/cert_checker.h"
#include "log/cert_submission_handler.h"
#include "log/ct_extensions.h"
#include "proto/ct.pb.h"
#include "util/testing.h"
#include "util/util.h"

DEFINE_string(test_certs_dir, "test/testdata", "Path to test certificates");

// Valid certificates.
// Self-signed
static const char kCaCert[] = "ca-cert.pem";
// Issued by ca.pem
static const char kLeafCert[] = "test-cert.pem";
// Issued by ca.pem
static const char kCaPreCert[] = "ca-pre-cert.pem";
// Issued by ca-cert.pem
static const char kPreCert[] = "test-embedded-pre-cert.pem";
// Issued by ca-pre-cert.pem
static const char kPreWithPreCaCert[] = "test-embedded-with-preca-pre-cert.pem";
// Issued by ca-cert.pem
static const char kIntermediateCert[] = "intermediate-cert.pem";
// Issued by intermediate-cert.pem
static const char kChainLeafCert[] = "test-intermediate-cert.pem";

namespace {

using ct::LogEntry;
using std::string;
using ct::Cert;
using ct::CertChain;
using ct::PreCertChain;
using ct::CertChecker;

class CertSubmissionHandlerTest : public ::testing::Test {
 protected:
  string ca_;
  string leaf_;
  string ca_precert_;
  string precert_;
  string precert_with_preca_;
  string intermediate_;
  string chain_leaf_;
  string cert_dir_;
  CertSubmissionHandler *handler_;
  CertChecker *checker_;

  CertSubmissionHandlerTest() : handler_(NULL) {}

  void SetUp() {
    cert_dir_ = FLAGS_test_certs_dir;
    checker_ = new CertChecker();
    checker_->LoadTrustedCertificates(cert_dir_ + "/" + kCaCert);
    handler_ = new CertSubmissionHandler(checker_);
    CHECK(util::ReadBinaryFile(cert_dir_ + "/" + kCaCert, &ca_))
        << "Could not read test data from " << cert_dir_
        << ". Wrong --test_certs_dir?";
    CHECK(util::ReadBinaryFile(cert_dir_ + "/" + kLeafCert, &leaf_));
    CHECK(util::ReadBinaryFile(cert_dir_ + "/" + kCaPreCert, &ca_precert_));
    CHECK(util::ReadBinaryFile(cert_dir_ + "/" + kPreCert, &precert_));
    CHECK(util::ReadBinaryFile(cert_dir_ + "/" + kPreWithPreCaCert,
                               &precert_with_preca_));
    CHECK(util::ReadBinaryFile(cert_dir_ + "/" + kIntermediateCert,
                               &intermediate_));
    CHECK(util::ReadBinaryFile(cert_dir_ + "/" + kChainLeafCert,
                               &chain_leaf_));
  }

  ~CertSubmissionHandlerTest() {
    delete checker_;
    delete handler_;
  }
};

TEST_F(CertSubmissionHandlerTest, SubmitCert) {
  LogEntry entry;
  entry.set_type(ct::X509_ENTRY);
  // Submit a leaf cert.
  EXPECT_EQ(CertSubmissionHandler::OK,
            handler_->ProcessSubmission(leaf_, &entry));
  EXPECT_TRUE(entry.has_x509_entry());
  EXPECT_FALSE(entry.has_precert_entry());
  EXPECT_TRUE(entry.x509_entry().has_leaf_certificate());
  // Chain should include the root.
  EXPECT_EQ(1, entry.x509_entry().certificate_chain_size());
}

TEST_F(CertSubmissionHandlerTest, SubmitEmptyCert) {
  LogEntry entry;
  entry.set_type(ct::X509_ENTRY);
  EXPECT_EQ(CertSubmissionHandler::EMPTY_SUBMISSION,
            handler_->ProcessSubmission("", &entry));
}

TEST_F(CertSubmissionHandlerTest, SubmitInvalidCert) {
  LogEntry entry;
  entry.set_type(ct::X509_ENTRY);
  EXPECT_EQ(CertSubmissionHandler::INVALID_PEM_ENCODED_CHAIN,
            handler_->ProcessSubmission("-----BEGIN CERTIFICATE-----\ninvalid"
                                        "\n-----END CERTIFICATE-----", &entry));
}

TEST_F(CertSubmissionHandlerTest, SubmitChain) {
  // Submit a chain.
  string submit = chain_leaf_ + intermediate_;
  LogEntry entry;
  entry.set_type(ct::X509_ENTRY);
  EXPECT_EQ(CertSubmissionHandler::OK,
            handler_->ProcessSubmission(submit, &entry));
  EXPECT_TRUE(entry.x509_entry().has_leaf_certificate());
  EXPECT_EQ(2, entry.x509_entry().certificate_chain_size());
}

TEST_F(CertSubmissionHandlerTest, SubmitPartialChain) {
  LogEntry entry;
  entry.set_type(ct::X509_ENTRY);
  // Submit a leaf cert with a missing intermediate.
  EXPECT_EQ(CertSubmissionHandler::UNKNOWN_ROOT,
            handler_->ProcessSubmission(chain_leaf_, &entry));
}

TEST_F(CertSubmissionHandlerTest, SubmitInvalidChain) {
  string invalid_submit = leaf_;
  invalid_submit.append(leaf_);
  LogEntry entry;
  entry.set_type(ct::X509_ENTRY);
  // An invalid chain with two certs in wrong order.
  EXPECT_EQ(CertSubmissionHandler::INVALID_CERTIFICATE_CHAIN,
            handler_->ProcessSubmission(invalid_submit, &entry));
}

TEST_F(CertSubmissionHandlerTest, SubmitCertAsPreCert) {
  LogEntry entry;
  entry.set_type(ct::PRECERT_ENTRY);
  // Various things are wrong here, so do not expect a specific error.
  EXPECT_NE(CertSubmissionHandler::OK,
            handler_->ProcessSubmission(leaf_, &entry));
}

TEST_F(CertSubmissionHandlerTest, SubmitCertChainAsPreCert) {
  string submit = chain_leaf_ + intermediate_;
  LogEntry entry;
  entry.set_type(ct::PRECERT_ENTRY);
  EXPECT_NE(CertSubmissionHandler::OK,
            handler_->ProcessSubmission(submit, &entry));
}

TEST_F(CertSubmissionHandlerTest, SubmitPreCertChain) {
  string submit = precert_ + ca_;
  LogEntry entry;
  entry.set_type(ct::PRECERT_ENTRY);
  EXPECT_EQ(CertSubmissionHandler::OK,
            handler_->ProcessSubmission(submit, &entry));
  EXPECT_TRUE(entry.has_precert_entry());
  EXPECT_FALSE(entry.has_x509_entry());
  EXPECT_TRUE(entry.precert_entry().has_pre_certificate());
  EXPECT_TRUE(entry.precert_entry().pre_cert().has_issuer_key_hash());
  EXPECT_TRUE(entry.precert_entry().pre_cert().has_tbs_certificate());

  // CA cert
  EXPECT_EQ(1, entry.precert_entry().precertificate_chain_size());
}

TEST_F(CertSubmissionHandlerTest, SubmitPreCertChainUsingPreCA) {
  string submit = precert_with_preca_ + ca_precert_;
  LogEntry entry;
  entry.set_type(ct::PRECERT_ENTRY);
  EXPECT_EQ(CertSubmissionHandler::OK,
            handler_->ProcessSubmission(submit, &entry));
  EXPECT_TRUE(entry.has_precert_entry());
  EXPECT_FALSE(entry.has_x509_entry());
  EXPECT_TRUE(entry.precert_entry().has_pre_certificate());
  EXPECT_TRUE(entry.precert_entry().pre_cert().has_issuer_key_hash());
  EXPECT_TRUE(entry.precert_entry().pre_cert().has_tbs_certificate());

  // Precert Signing Certificate + CA cert
  EXPECT_EQ(2, entry.precert_entry().precertificate_chain_size());
}

TEST_F(CertSubmissionHandlerTest, SubmitInvalidPreCertChain) {
  // Missing issuer.
  string submit = precert_with_preca_;
  LogEntry entry;
  entry.set_type(ct::PRECERT_ENTRY);
  EXPECT_NE(CertSubmissionHandler::OK,
            handler_->ProcessSubmission(submit, &entry));
}

}  // namespace

int main(int argc, char**argv) {
  ct::test::InitTesting(argv[0], &argc, &argv, true);
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  ct::LoadCtExtensions();
  return RUN_ALL_TESTS();
}
