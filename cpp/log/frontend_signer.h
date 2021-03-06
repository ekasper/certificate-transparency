#ifndef FRONTEND_SIGNER_H
#define FRONTEND_SIGNER_H

#include <stdint.h>
#include <string>

#include "logged_certificate.h"

template <class Logged> class Database;
class LogSigner;

class FrontendSigner {
 public:
  enum SubmitResult {
    NEW,
    DUPLICATE,
  };

  // Takes ownership of |signer|.
  FrontendSigner(Database<ct::LoggedCertificate> *db, LogSigner *signer);

  ~FrontendSigner();

  // Log the entry if it's not already in the database,
  // and return either a new timestamp-signature pair,
  // or a previously existing one. (Currently also copies the
  // entry to the sct but you shouldn't rely on this.)
  SubmitResult QueueEntry(const ct::LogEntry &entry,
                          ct::SignedCertificateTimestamp *sct);

 private:
  Database<ct::LoggedCertificate> *db_;
  LogSigner *signer_;

  void TimestampAndSign(const ct::LogEntry &entry,
                        ct::SignedCertificateTimestamp *sct) const;
};
#endif
