#include "prince.h"

/**
 * Converts a byte array into a 64 bit integer
 * byte at index 0 is placed as the most significant byte.
 */
static uint64_t bytes_to_uint64(const uint8_t in[8]){
  uint64_t out=0;
  unsigned int i;
  for(i=0;i<8;i++) 
    out = (out<<8) | in[i];  
  return out;
}

/**
 * Converts a 64 bit integer into a byte array
 * The most significant byte is placed at index 0.
 */
static void uint64_to_bytes(const uint64_t in, uint8_t out[8]){
  unsigned int i;
  for(i=0;i<8;i++)
    out[i]=in>>((7-i)*8);
}

//Provides bytevec in big-endian format (MSB first as needed by PRINCE)
void uint64_to_bytevec(uint64_t  input, uint8_t output[8]){
  uint8_t* input_le = (uint8_t*)&input;
  for(int i=0; i<8; i++){
output[i] = input_le[7-i];
  }
}
//Provides uint64_t in little-endian format (converting from big-endian output of  PRINCE)
uint64_t bytevec_to_uint64(uint8_t input[8]){
  uint64_t output;
  uint8_t* output_le = (uint8_t*)&output;
  for(int i=0; i<8; i++){
    output_le[i] = input[7-i];
  }
  return output;
}

uint64_t calcPRINCE64(uint64_t phy_line_num,uint64_t seed){

  //Variables for keys, input, output.
  uint8_t plaintext[8];
  uint8_t ciphertext[8];
  uint8_t key[16];

  //Key Values  
  uint64_t k0 = 0x0011223344556677;
  uint64_t k1 = 0x8899AABBCCDDEEFF;
  //Add seed to keys for each skew.
  k0 = k0 + seed;
  k1 = k1 + seed;
  
  //Set up keys, plaintext
  uint64_to_bytevec(k0,key);
  uint64_to_bytevec(k1,key+8);
  uint64_to_bytevec(phy_line_num,plaintext);

  //TEST PLAINTEXT
  //uint64_t test_plaintext = 0x0123456789ABCDEF; 
  //uint64_to_bytevec(test_plaintext,plaintext);

  //Prince encrypt.
  prince_encrypt(plaintext,key,ciphertext);

  //Convert output
  uint64_t enc64_hash = bytevec_to_uint64(ciphertext);
  
  //PRINT TEST
  //cprintf("PT: %llx, K0: %llx, K1 %llx, CT:%llx\n", /*test_plaintext*/phy_line_num, k0,k1,enc64_hash);

  return enc64_hash;    
}

/**
 * Byte oriented top level function for Prince encryption.
 * key_bytes 0 to 7 must contain K0
 * key_bytes 8 to 15 must contain K1
 */
static void prince_encrypt(const uint8_t in_bytes[8],const uint8_t key_bytes[16], uint8_t out_bytes[8]){
  prince_enc_dec(in_bytes,key_bytes,out_bytes,0);
}


/**
 * Byte oriented top level function for Prince encryption/decryption.
 * key_bytes 0 to 7 must contain K0
 * key_bytes 8 to 15 must contain K1
 */
static void prince_enc_dec(const uint8_t in_bytes[8],const uint8_t key_bytes[16], uint8_t out_bytes[8], int decrypt){
  const uint64_t input = bytes_to_uint64(in_bytes);
  const uint64_t enc_k0 = bytes_to_uint64(key_bytes);
  const uint64_t enc_k1 = bytes_to_uint64(key_bytes+8);
  const uint64_t output = prince_enc_dec_uint64(input,enc_k0,enc_k1,decrypt);
  uint64_to_bytes(output,out_bytes);
}


/**
 * Top level function for Prince encryption/decryption.
 * enc_k0 and enc_k1 must be the same for encryption and decryption, the handling of decryption is done internally.
 */
static uint64_t prince_enc_dec_uint64(const uint64_t input,const uint64_t enc_k0, const uint64_t enc_k1, int decrypt){
  const uint64_t prince_alpha = UINT64_C(0xc0ac29b7c97c50dd);
  const uint64_t k1 = enc_k1 ^ (decrypt ? prince_alpha : 0);
  const uint64_t enc_k0_prime = prince_k0_to_k0_prime(enc_k0);
  const uint64_t k0       = decrypt ? enc_k0_prime : enc_k0;
  const uint64_t k0_prime = decrypt ? enc_k0       : enc_k0_prime;
  PRINCE_PRINT(k0);
  PRINCE_PRINT(input); 
  const uint64_t core_input = input ^ k0;
  const uint64_t core_output = prince_core(core_input,k1);
  const uint64_t output = core_output ^ k0_prime; 
  PRINCE_PRINT(k0_prime);
  PRINCE_PRINT(output);
  return output;
}


/**
 * The core function of the Prince cipher.
 */
static uint64_t prince_core(const uint64_t core_input, const uint64_t k1){
  PRINCE_PRINT(core_input);
  PRINCE_PRINT(k1);
  uint64_t round_input = core_input ^ k1 ^ prince_round_constant(0);
  for(unsigned int round = 1; round < 6; round++){
    PRINCE_PRINT(round_input);
    const uint64_t s_out = prince_s_layer(round_input);
    PRINCE_PRINT(s_out);
    const uint64_t m_out = prince_m_layer(s_out);
    PRINCE_PRINT(m_out);
    round_input = m_out ^ k1 ^ prince_round_constant(round);
  }
  const uint64_t middle_round_s_out = prince_s_layer(round_input);
  PRINCE_PRINT(middle_round_s_out);
  const uint64_t m_prime_out = prince_m_prime_layer(middle_round_s_out);
  PRINCE_PRINT(m_prime_out);
  const uint64_t middle_round_s_inv_out = prince_s_inv_layer(m_prime_out);
  round_input = middle_round_s_inv_out;  
  for(unsigned int round = 6; round < 11; round++){
    PRINCE_PRINT(round_input);
    const uint64_t m_inv_in = round_input ^ k1 ^ prince_round_constant(round);
    PRINCE_PRINT(m_inv_in);
    const uint64_t s_inv_in = prince_m_inv_layer(m_inv_in);
    PRINCE_PRINT(s_inv_in);
    const uint64_t s_inv_out = prince_s_inv_layer(s_inv_in);
    round_input = s_inv_out;
  }
  const uint64_t core_output = round_input ^ k1 ^ prince_round_constant(11);
  PRINCE_PRINT(core_output);
  return core_output;
}


/**
 * The M step of the Prince cipher.
 */
static uint64_t prince_m_layer(const uint64_t m_in){
  const uint64_t m_prime_out = prince_m_prime_layer(m_in);
  const uint64_t shift_rows_out = prince_shift_rows(m_prime_out,0);
  return shift_rows_out;
}

/**
 * The M^-1 step of the Prince cipher.
 */
static uint64_t prince_m_inv_layer(const uint64_t m_inv_in){
  const uint64_t shift_rows_out = prince_shift_rows(m_inv_in,1);
  const uint64_t m_prime_out = prince_m_prime_layer(shift_rows_out);
  return m_prime_out;
}


/**
 * The shift row and inverse shift row of the Prince cipher.
 */
static uint64_t prince_shift_rows(const uint64_t in, int inverse){
  const uint64_t row_mask = UINT64_C(0xF000F000F000F000);
  uint64_t shift_rows_out = 0;
  for(unsigned int i=0;i<4;i++){
    const uint64_t row = in & (row_mask>>(4*i));
    const unsigned int shift = inverse ? i*16 : 64-i*16;
    shift_rows_out |= (row>>shift) | (row<<(64-shift));
  }
  return shift_rows_out;
}

/**
 * The M' step of the Prince cipher.
 */
static uint64_t prince_m_prime_layer(const uint64_t m_prime_in){
  //16 bits matrices M0 and M1, generated using the code below
  //uint64_t m16[2][16];prince_m16_matrices(m16);
  //for(unsigned int i=0;i<16;i++) PRINCE_PRINT(m16[0][i]);
  //for(unsigned int i=0;i<16;i++) PRINCE_PRINT(m16[1][i]);
  static const uint64_t m16[2][16] = {
    {   0x0111,                                                                                                                                                                        
        0x2220,                                                                                                                                                                        
        0x4404,                                                                                                                                                                        
        0x8088,                                                                                                                                                                        
        0x1011,                                                                                                                                                                        
        0x0222,                                                                                                                                                                        
        0x4440,                                                                                                                                                                        
        0x8808,                                                                                                                                                                        
        0x1101,                                                                                                                                                                        
        0x2022,                                                                                                                                                                        
        0x0444,                                                                                                                                                                        
        0x8880,                                                                                                                                                                        
        0x1110,                                                                                                                                                                        
        0x2202,                                                                                                                                                                        
        0x4044,                                                                                                                                                                        
        0x0888},
    
    {   0x1110,                                                                                                                                                                        
        0x2202,                                                                                                                                                                        
        0x4044,                                                                                                                                                                        
        0x0888,                                                                                                                                                                        
        0x0111,                                                                                                                                                                        
        0x2220,                                                                                                                                                                        
        0x4404,                                                                                                                                                                        
        0x8088,                                                                                                                                                                        
        0x1011,                                                                                                                                                                        
        0x0222,                                                                                                                                                                        
        0x4440,                                                                                                                                                                        
        0x8808,                                                                                                                                                                        
        0x1101,                                                                                                                                                                        
        0x2022,                                                                                                                                                                        
        0x0444,                                                                                                                                                                        
        0x8880} 
  };
  const uint64_t chunk0 = gf2_mat_mult16_1(m_prime_in>>(0*16),m16[0]);
  const uint64_t chunk1 = gf2_mat_mult16_1(m_prime_in>>(1*16),m16[1]);
  const uint64_t chunk2 = gf2_mat_mult16_1(m_prime_in>>(2*16),m16[1]);
  const uint64_t chunk3 = gf2_mat_mult16_1(m_prime_in>>(3*16),m16[0]);
  const uint64_t m_prime_out = (chunk3<<(3*16)) | (chunk2<<(2*16)) | (chunk1<<(1*16)) | (chunk0<<(0*16));
  return m_prime_out;
}

/**
 * The S^-1 step of the Prince cipher.
 */
static uint64_t prince_s_inv_layer(const uint64_t s_inv_in){
  uint64_t s_inv_out = 0;
  for(unsigned int i=0;i<16;i++){
    const unsigned int shift = i*4;
    const unsigned int sbox_in = s_inv_in>>shift;
    const uint64_t sbox_out = prince_sbox_inv(sbox_in);
    s_inv_out |= sbox_out<<shift;
  }
  return s_inv_out;
}

static uint64_t gf2_mat_mult16_1(const uint64_t in, const uint64_t mat[16]){
  uint64_t out = 0;
  for(unsigned int i=0;i<16;i++){
    if((in>>i) & 1)
      out ^= mat[i];
  }
  return out;
}

/**
 * The 4 bit Prince inverse sbox. Only the 4 lsb are taken into account.
 */
static unsigned int prince_sbox_inv(unsigned int nibble){
  const unsigned int sbox[] = {
    0xb, 0x7, 0x3, 0x2,
    0xf, 0xd, 0x8, 0x9, 
    0xa, 0x6, 0x4, 0x0,
    0x5, 0xe, 0xc, 0x1
  };
  return sbox[nibble & 0xF];
}

/**
 * The S step of the Prince cipher.
 */
static uint64_t prince_s_layer(const uint64_t s_in){
  uint64_t s_out = 0;
  for(unsigned int i=0;i<16;i++){
    const unsigned int shift = i*4;
    const unsigned int sbox_in = s_in>>shift;
    const uint64_t sbox_out = prince_sbox(sbox_in);
    s_out |= sbox_out<<shift;
  }
  return s_out;
}

/**
 * The 4 bit Prince sbox. Only the 4 lsb are taken into account.
 */
static unsigned int prince_sbox(unsigned int nibble){
  const unsigned int sbox[] = {
    0xb, 0xf, 0x3, 0x2,
    0xa, 0xc, 0x9, 0x1, 
    0x6, 0x7, 0x8, 0x0,
    0xe, 0x5, 0xd, 0x4
  };
  return sbox[nibble & 0xF];
}

/**
 * Compute K0' from K0
 */
static uint64_t prince_k0_to_k0_prime(const uint64_t k0){
  uint64_t k0_ror1 = (k0 >> 1) | (k0 << 63);
  uint64_t k0_prime = k0_ror1 ^ (k0 >> 63);
  return k0_prime;
}

static uint64_t prince_round_constant(const unsigned int round){
  uint64_t rc[] = {
    UINT64_C(0x0000000000000000),
    UINT64_C(0x13198a2e03707344),
    UINT64_C(0xa4093822299f31d0),
    UINT64_C(0x082efa98ec4e6c89),
    UINT64_C(0x452821e638d01377),
    UINT64_C(0xbe5466cf34e90c6c),
    UINT64_C(0x7ef84f78fd955cb1),
    UINT64_C(0x85840851f1ac43aa),
    UINT64_C(0xc882d32f25323c54),
    UINT64_C(0x64a51195e0e3610d),
    UINT64_C(0xd3b5a399ca0c2399),
    UINT64_C(0xc0ac29b7c97c50dd)
  };
  return rc[round];
}

