#ifndef BF_H
#define BF_H

#include <cstdint>

#include <arpa/inet.h>

namespace ss::bf {

typedef union {
	std::uint8_t inb[8];
	struct {
		std::uint32_t L;
		std::uint32_t R;
	};
} block_t;

class block {
	// original contents of P/S boxes
	static const std::uint32_t ORIG_P[16 + 2];
	static const std::uint32_t ORIG_S[4][256];

	// P/S boxes
	std::uint32_t P[16 + 2];
	std::uint32_t S[4][256];

	// 16 rounds is standard
	static const int ROUNDS = 16;

	// private internal routines
	std::uint32_t F(std::uint32_t x);
	void Blowfish_Encrypt(std::uint32_t *xl, std::uint32_t *xr);
	void Blowfish_Decrypt(std::uint32_t *xl, std::uint32_t *xr);
	void Blowfish_Init(const std::uint8_t *key, int keyLen);

public:
	block(const std::uint8_t *a_buffer, std::uint8_t *a_key, std::size_t a_key_len);
	const uint8_t *get_blockdata() const { return (const uint8_t *)m_block.inb; }

	void encrypt();
	void decrypt();

protected:
	block_t m_block;
};
	
}; // namespace ss::bf

#endif // BF_H
