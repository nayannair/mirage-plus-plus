#ifndef PRINCE_H
#define PRINCE_H

#include <stdint.h>

#ifndef PRINCE_PRINT
#define PRINCE_PRINT(a) do{}while(0)
#endif

uint64_t calcPRINCE64(uint64_t phy_line_num, uint64_t seed);

/**
 * Byte oriented top level function for Prince encryption.
 * key_bytes 0 to 7 must contain K0
 * key_bytes 8 to 15 must contain K1
 */
static void prince_encrypt(const uint8_t in_bytes[8],const uint8_t key_bytes[16], uint8_t out_bytes[8]);


/**
 * Top level function for Prince encryption/decryption.
 * enc_k0 and enc_k1 must be the same for encryption and decryption, the handling of decryption is done internally.
 */
static uint64_t prince_enc_dec_uint64(const uint64_t input,const uint64_t enc_k0, const uint64_t enc_k1, int decrypt);

static void prince_enc_dec(const uint8_t in_bytes[8],const uint8_t key_bytes[16], uint8_t out_bytes[8], int decrypt);

/**
 * The core function of the Prince cipher.
 */
static uint64_t prince_core(const uint64_t core_input, const uint64_t k1);

/**
 * The M step of the Prince cipher.
 */
static uint64_t prince_m_layer(const uint64_t m_in);

/**
 * The M^-1 step of the Prince cipher.
 */
static uint64_t prince_m_inv_layer(const uint64_t m_inv_in);

/**
 * The shift row and inverse shift row of the Prince cipher.
 */
static uint64_t prince_shift_rows(const uint64_t in, int inverse);

/**
 * The M' step of the Prince cipher.
 */
static uint64_t prince_m_prime_layer(const uint64_t m_prime_in);

/**
 * The S^-1 step of the Prince cipher.
 */
static uint64_t prince_s_inv_layer(const uint64_t s_inv_in);

static uint64_t gf2_mat_mult16_1(const uint64_t in, const uint64_t mat[16]);

/**
 * The 4 bit Prince inverse sbox. Only the 4 lsb are taken into account.
 */
static unsigned int prince_sbox_inv(unsigned int nibble);

/**
 * The S step of the Prince cipher.
 */
static uint64_t prince_s_layer(const uint64_t s_in);

/**
 * The 4 bit Prince sbox. Only the 4 lsb are taken into account.
 */
static unsigned int prince_sbox(unsigned int nibble);

/**
 * Compute K0' from K0
 */
static uint64_t prince_k0_to_k0_prime(const uint64_t k0);

static uint64_t prince_round_constant(const unsigned int round);

/**
 * Converts a byte array into a 64 bit integer
 * byte at index 0 is placed as the most significant byte.
 */
static uint64_t bytes_to_uint64(const uint8_t in[8]);

/**
 * Converts a 64 bit integer into a byte array
 * The most significant byte is placed at index 0.
 */
static void uint64_to_bytes(const uint64_t in, uint8_t out[8]);


//Provides bytevec in big-endian format (MSB first as needed by PRINCE)
void uint64_to_bytevec(uint64_t  input, uint8_t output[8]);

//Provides uint64_t in little-endian format (converting from big-endian output of  PRINCE)
uint64_t bytevec_to_uint64(uint8_t input[8]);



#endif //PRINCE_H