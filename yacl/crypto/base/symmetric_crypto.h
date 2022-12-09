// Copyright 2019 Ant Group Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

#include "absl/types/span.h"
#include "openssl/evp.h"

#include "yacl/base/byte_container_view.h"
#include "yacl/base/int128.h"

namespace yacl {
namespace internal {

inline void EcbMakeContentBlocks(uint128_t count, absl::Span<uint128_t> buf) {
  std::iota(buf.begin(), buf.end(), count);
}

}  // namespace internal

// This class implements Symmetric- crypto.
class SymmetricCrypto {
 public:
  enum class CryptoType : int {
    AES128_ECB,
    AES128_CBC,
    AES128_CTR,
    SM4_ECB,
    SM4_CBC,
    SM4_CTR,
  };

  SymmetricCrypto(CryptoType type, uint128_t key, uint128_t iv = 0);
  SymmetricCrypto(CryptoType type, ByteContainerView key, ByteContainerView iv);

  ~SymmetricCrypto() {
    EVP_CIPHER_CTX_free(enc_ctx_);
    EVP_CIPHER_CTX_free(dec_ctx_);
  }

  // CBC Block Size.
  static constexpr int BlockSize() { return 128 / 8; }

  // Encrypts `plaintext` into `ciphertext`.
  // Note the ciphertext/plaintext size must be `N * kBlockSize`.
  void Encrypt(absl::Span<const uint8_t> plaintext,
               absl::Span<uint8_t> ciphertext) const;

  // Decrypts `ciphertext` into `plaintext`.
  // Note the ciphertext/plaintext size must be `N * kBlockSize`.
  void Decrypt(absl::Span<const uint8_t> ciphertext,
               absl::Span<uint8_t> plaintext) const;

  // Wrapper for uint128.
  uint128_t Encrypt(uint128_t input) const;
  uint128_t Decrypt(uint128_t input) const;

  // Wrapper for span<uint128>.
  void Encrypt(absl::Span<const uint128_t> plaintext,
               absl::Span<uint128_t> ciphertext) const;
  void Decrypt(absl::Span<const uint128_t> ciphertext,
               absl::Span<uint128_t> plaintext) const;

  // Getter
  CryptoType GetType() const { return type_; }

 private:
  // Crypto type
  const CryptoType type_;
  // Symmetric key, 128 bits
  const uint128_t key_;

  // Initial vector cbc mode need
  const uint128_t initial_vector_;

  EVP_CIPHER_CTX* enc_ctx_;
  EVP_CIPHER_CTX* dec_ctx_;
};

class AesCbcCrypto : public SymmetricCrypto {
 public:
  AesCbcCrypto(uint128_t key, uint128_t iv)
      : SymmetricCrypto(SymmetricCrypto::CryptoType::AES128_CBC, key, iv) {}
};

class Sm4CbcCrypto : public SymmetricCrypto {
 public:
  Sm4CbcCrypto(uint128_t key, uint128_t iv)
      : SymmetricCrypto(SymmetricCrypto::CryptoType::SM4_CBC, key, iv) {}
};

// FillAesRandom generate pseudo random bytes and fill the `out`.
// The pseudo random bytes are generated by using AES-CBC to encrypt a
// continuous buffer with incremental counters as contents.
template <typename T,
          std::enable_if_t<std::is_standard_layout<T>::value, int> = 0>
inline uint64_t FillPseudoRandom(SymmetricCrypto::CryptoType crypto_type,
                                 uint128_t seed, uint128_t iv, uint64_t count,
                                 absl::Span<T> out) {
  constexpr size_t block_size = SymmetricCrypto::BlockSize();
  const size_t nbytes = out.size() * sizeof(T);
  const size_t nblock = (nbytes + block_size - 1) / block_size;
  const size_t padding_bytes = nbytes % block_size;

  bool isCTR = (crypto_type == SymmetricCrypto::CryptoType::AES128_CTR ||
                crypto_type == SymmetricCrypto::CryptoType::SM4_CTR);

  std::unique_ptr<SymmetricCrypto> crypto;
  if (isCTR) {
    // CTR mode does not requires padding or manully build counter...
    crypto = std::make_unique<SymmetricCrypto>(crypto_type, seed, count);
    std::memset(out.data(), 0, nbytes);
    auto bv = absl::MakeSpan(reinterpret_cast<uint8_t*>(out.data()), nbytes);
    crypto->Encrypt(bv, bv);
  } else {
    crypto = std::make_unique<SymmetricCrypto>(crypto_type, seed, iv);
    if (padding_bytes == 0) {
      // No padding, fast path
      auto s = absl::MakeSpan(reinterpret_cast<uint128_t*>(out.data()), nblock);
      internal::EcbMakeContentBlocks(count, s);
      crypto->Encrypt(s, s);
    } else {
      if (crypto_type == SymmetricCrypto::CryptoType::AES128_ECB ||
          crypto_type == SymmetricCrypto::CryptoType::SM4_ECB) {
        if (nblock > 1) {
          // first n-1 block
          auto s = absl::MakeSpan(reinterpret_cast<uint128_t*>(out.data()),
                                  nblock - 1);
          internal::EcbMakeContentBlocks(count, s);
          crypto->Encrypt(s, s);
        }
        // last padding block
        uint128_t padding = count + nblock - 1;
        padding = crypto->Encrypt(padding);
        std::memcpy(reinterpret_cast<uint128_t*>(out.data()) + (nblock - 1),
                    &padding, padding_bytes);
      } else {
        std::vector<uint128_t> cipher(nblock);
        auto s = absl::MakeSpan(cipher);
        internal::EcbMakeContentBlocks(count, s);
        crypto->Encrypt(s, s);
        std::memcpy(out.data(), cipher.data(), nbytes);
      }
    }
  }

  return count + nblock;
}

template <typename T,
          std::enable_if_t<std::is_standard_layout<T>::value, int> = 0>
inline uint64_t FillAesRandom(uint128_t seed, uint128_t iv, uint64_t count,
                              absl::Span<T> out) {
  // TODO(shuyan.ycf):
  // We keep this as AES_CBC due to SGX-Beaver is using AES_CBC now. After
  // SGX-Beaver has transferred to AES_ECB, we should use AES_ECB instead.
  return FillPseudoRandom<T>(SymmetricCrypto::CryptoType::AES128_CBC, seed, iv,
                             count, out);
}

// in some asymmetric scene
// may exist parties only need update count by buffer size.
template <typename T,
          std::enable_if_t<std::is_standard_layout<T>::value, int> = 0>
inline uint64_t DummyUpdateRandomCount(uint64_t count, absl::Span<T> out) {
  constexpr size_t block_size = SymmetricCrypto::BlockSize();
  const size_t nbytes = out.size() * sizeof(T);
  const size_t nblock = (nbytes + block_size - 1) / block_size;
  return count + nblock;
}

template <typename T,
          std::enable_if_t<std::is_standard_layout<T>::value, int> = 0>
inline uint64_t FillSm4Random(uint128_t seed, uint64_t count,
                              absl::Span<T> out) {
  return FillPseudoRandom<T>(SymmetricCrypto::CryptoType::SM4_CBC, seed, count,
                             out);
}

}  // namespace yacl