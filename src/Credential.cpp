#include "Credential.hpp"

#include <memory>

#include <QTextStream>

#include <KWallet/KWallet>

#include "debug.hpp"

using KWallet::Wallet;

namespace {
// Reflection
template <QString Credential::*member>
const auto field_name = QStringLiteral("");

template <>
const auto field_name<&Credential::protocol> = QStringLiteral("protocol");
template <>
const auto field_name<&Credential::host> = QStringLiteral("host");
template <>
const auto field_name<&Credential::username> = QStringLiteral("username");
template <>
const auto field_name<&Credential::password> = QStringLiteral("password");

template <QString Credential::*member>
auto field_member() {
  return std::make_pair(field_name<member>, member);
}

const std::map<QString, QString Credential::*, std::less<>> fieldMapping = {
    field_member<&Credential::protocol>(),
    field_member<&Credential::host>(),
    field_member<&Credential::username>(),
    field_member<&Credential::password>()
};

} // namespace

namespace {
// Helpers
template <QString Credential::*member>
void printField(QTextStream& out, const Credential& cred) {
  auto&& value = cred.*member;
  if (!value.isEmpty()) {
    out << field_name<member> << '=' << value << '\n';
  }
}

QString composeKeyName(const Credential& credential) {
  QString result;
  if(!credential.protocol.isEmpty()) {
    result += credential.protocol + "://";
  }
  
  if(!credential.username.isEmpty()) {
    result += credential.username + '@';
  }
  
  if(!credential.host.isEmpty()) {
    result += credential.host + '/';
  }
  
  return result;
}

QString composeKeyNameNoUsername(const Credential& credential) {
  QString result;
  if(!credential.protocol.isEmpty()) {
    result += credential.protocol + "://";
  }
  
  if(!credential.host.isEmpty()) {
    result += credential.host + '/';
  }
  
  return result;
}

std::unique_ptr<QString> getPassword(QString& key, WalletSettings& settings) {
  if(Wallet::keyDoesNotExist(settings.wallet, settings.folder, key)) {
    debugStream() << "credentials not found";
    return std::make_unique<QString>();
  }
  
  auto wallet = std::unique_ptr< Wallet >(Wallet::openWallet(settings.wallet, 0));
  
  if(wallet == nullptr) {
    debugStream() << "couldn't open wallet";
    return std::make_unique<QString>();
  }
  
  if(!wallet->setFolder(settings.folder)) {
    debugStream() << "couldn't open folder";
    return std::make_unique<QString>();
  }
  
  auto buffer = std::make_unique<QString>();
  if(wallet->readPassword(key, *buffer) != 0) {
    debugStream() << "couldn't read password";
    return std::make_unique<QString>();
  }
  
  return buffer;
}

} // namespace

Credential read() {
  QTextStream in(stdin, QIODevice::ReadOnly);
  Credential result;
  QString line;
  while (in.readLineInto(&line)) {
    auto splitPosition = line.indexOf('=');
    if (splitPosition == -1) {
      continue;
    }
    auto fieldName = line.left(splitPosition);
    auto it = fieldMapping.find(fieldName);
    if (it == fieldMapping.end()) {
      continue;
    }
    auto fieldPtr = it->second;
    result.*fieldPtr = line.mid(splitPosition + 1);
  }
  return result;
}

void write(Credential&& cred) {
  QTextStream out(stdout, QIODevice::WriteOnly);
  printField<&Credential::username>(out, cred);
  printField<&Credential::password>(out, cred);
}

Credential get(Credential&& credential, WalletSettings settings) {
  if (Wallet::folderDoesNotExist(settings.wallet, settings.folder)) {
    debugStream() << "no such folder";
    return {};
  }
  
  if(credential.username.isEmpty()) {
    auto key = composeKeyName(credential);
    std::unique_ptr<QString> username = getPassword(key, settings);
    if(username->isEmpty()) {
      return {};
    }
    
    credential.username = *username;
  }
  
  auto key = composeKeyName(credential);
  credential.password = *getPassword(key, settings);
  
  return std::move(credential);
}

void store(Credential&& credential, WalletSettings settings) {
  auto wallet = std::unique_ptr<Wallet>(Wallet::openWallet(settings.wallet, 0));
  if (wallet == nullptr) {
    debugStream() << "couldn't open wallet";
    return;
  }
  if (!wallet->hasFolder(settings.folder)) {
    if (!wallet->createFolder(settings.folder)) {
      debugStream() << "couldn't create folder";
      return;
    }
  }
  if (!wallet->setFolder(settings.folder)) {
    debugStream() << "couldn't open folder";
    return;
  }
  
  if(credential.username.isEmpty()) {
    debugStream() << "no username specified";
    return;
  }
  
  if (credential.password.isEmpty()) {
    debugStream() << "no password specified";
    return;
  }
  
  auto key = composeKeyNameNoUsername(credential);
  if (wallet->writePassword(key, credential.username) != 0) {
    debugStream() << "couldn't write no username correspondence";
  }
  
  auto keyName = composeKeyName(credential);
  if (wallet->writePassword(keyName, credential.password) != 0) {
    debugStream() << "couldn't write password";
  }
}

void erase(Credential&& credential, WalletSettings settings) {
  if (Wallet::folderDoesNotExist(settings.wallet, settings.folder)) {
    debugStream() << "no such folder";
    return;
  }
  if (credential.username.isEmpty()) {
    auto key = composeKeyName(credential);
    std::unique_ptr<QString> username = getPassword(key, settings);
    if(username->isEmpty()) {
      debugStream() << "no username specified";
      return;
    }
    
    credential.username = *username;
  }
  auto keyName = composeKeyName(credential);
  if (Wallet::keyDoesNotExist(settings.wallet, settings.folder, keyName)) {
    debugStream() << "credentials not found";
    return;
  }
  auto wallet = std::unique_ptr<Wallet>(Wallet::openWallet(settings.wallet, 0));
  if (wallet == nullptr) {
    debugStream() << "couldn't open wallet";
    return;
  }
  if (!wallet->setFolder(settings.folder)) {
    debugStream() << "couldn't open folder";
    return;
  }
  if (wallet->removeEntry(keyName) != 0) {
    debugStream() << "couldn't delete entry";
  }
  
  auto key = composeKeyNameNoUsername(credential);
  if (wallet->removeEntry(key) != 0) {
    debugStream() << "couldn't delete no username entry";
  }
}
