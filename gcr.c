/*
 * Copyright (c) 2006, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Implementation of GCR coding/decoding
 * \author
 *         Joakim Eriksson <joakime@sics.se>
 *
 */

/* GCR conversion table - used for converting ordinary byte to 10-bits */
/* (or 4 bits to 5) */
static const unsigned char GCR_encode[16] = {
  0x0a, 0x0b, 0x12, 0x13,
  0x0e, 0x0f, 0x16, 0x17,
  0x09, 0x19, 0x1a, 0x1b,
  0x0d, 0x1d, 0x1e, 0x15
  };

static const unsigned char GCR_encodeMac[64] = {
  0x96,0x97,0x9a,0x9b,0x9d,0x9e,0x9f,0xa6,			// http://bitsavers.informatik.uni-stuttgart.de/pdf/apple/disk/sony/MP-F51_specs/669-0452-A_800K_Double-Sided_ERS_Sep86.pdf
  0xa7,0xab,0xac,0xad,0xae,0xaf,0xb2,0xb3,
  0xb4,0xb5,0xb6,0xb7,0xb9,0xba,0xbb,0xbc,
  0xbd /*DB??*/,0xbe,0xbf,0xcb,0xcd,0xce,0xcf,0xd3,
  0xd6,0xd7,0xd9,0xda,0xdb,0xdc,0xdd,0xde,
  0xdf,0xe5,0xe6,0xe7,0xe9,0xea,0xeb,0xec,
  0xed,0xee,0xef,0xf2,0xf3,0xf4,0xf5,0xf6,
  0xf7,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
  };


/*  5 bits > 4 bits (0xff => invalid) */
static const unsigned char GCR_decode[32] = {
  0xff, 0xff, 0xff, 0xff, // 0 - 3invalid...
  0xff, 0xff, 0xff, 0xff, // 4 - 7 invalid...
  0xff, 0x08, 0x00, 0x01, // 8 invalid... 9 = 8, a = 0, b = 1
  0xff, 0x0c, 0x04, 0x05, // c invalid... d = c, e = 4, f = 5

  0xff, 0xff, 0x02, 0x03, // 10-11 invalid...
  0xff, 0x0f, 0x06, 0x07, // 14 invalid...
  0xff, 0x09, 0x0a, 0x0b, // 18 invalid...
  0xff, 0x0d, 0x0e, 0xff, // 1c, 1f invalid...
  };

static const unsigned char GCR_decodeMac[256-0x96] = {    // parte da 0x96, giustamente - v.encode
  0x00,0x01,0xff,0xff,0x02,0x03,0xff,0x04,			// 
  0x05,0x06,0xff,0xff,0xff,0xff,0xff,0xff,
  0x07,0x08,0xff,0xff,0xff,0x09,0x0a,0x0b,
  0x0c,0x0d,0xff,0xff,0x0e,0x0f,0x10,0x11,
  0x12,0x13,0xff,0x14,0x15,0x16,0x17,0x18,
  0x19,0x1a,0xff,0xff,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0x1b,0xff,0x1c,
  0x1d,0x1e,0xff,0xff,0xff,0x1f,0xff,0xff,
  0x20,0x21,0xff,0x22,0x23,0x24,0x25,0x26,
  0x27,0x28,0xff,0xff,0xff,0xff,0xff,0x29,
  0x2a,0x2b,0xff,0x2c,0x2d,0x2e,0x2f,0x30,
  0x31,0x32,0xff,0xff,0x33,0x34,0x35,0x36,
  0x37,0x38,0xff,0x39,0x3a,0x3b,0x3c,0x3d,
  0x3e,0x3f
  };

unsigned char gcr_bits = 0;
unsigned short gcr_val = 0;

// Call before starting encoding or decoding 
void gcr_init(void) {
  
  gcr_val = 0;
  gcr_bits = 0;
  }

// Use this to check if encoding / decoding is complete for now 
unsigned char gcr_finished(void) {
  
  return gcr_bits == 0;
  }

// Encode one character - and store in bits - get encoded with get_encoded 
void gcr_encode2(unsigned char raw_data) {
  
  gcr_val |= ((GCR_encode[raw_data >> 4u] << 5) |
    GCR_encode[raw_data & 0xf]) << gcr_bits;
  gcr_bits += 10;
  }

unsigned char gcr_encodebyte(unsigned char raw_data) {
  
  return GCR_encodeMac[raw_data & 0x3f];
  }

unsigned char gcr_decodebyte(unsigned char raw_data) {
  
  return GCR_decodeMac[raw_data - 0x96];
  }

// Gets the current char of the encoded stream 
unsigned char gcr_get_encoded(unsigned char *raw_data) {
  
  if(gcr_bits >= 8) {
    *raw_data = (unsigned char)(gcr_val & 0xff);
    gcr_val >>= 8;
    gcr_bits -= 8;
    return 1;
    }
  return 0;
  }


// Decode one char - result can be get from get_decoded 
void gcr_decode(unsigned char gcr_data) {
  
  gcr_val |= gcr_data << gcr_bits;
  gcr_bits += 8;
  }

// check if the current decoded stream is correct 
unsigned char gcr_valid(void) {
  
  if(gcr_bits >= 10) {
    unsigned short val = gcr_val & 0x3ff;
    if((GCR_decode[val >> 5u] << 4u) == 0xff ||
      (GCR_decode[val & 0x1f]) == 0xff) {
      return 0;
      }
    }
  return 1;
  }

// gets the decoded stream - if any char is available 
unsigned char gcr_get_decoded(unsigned char *raw_data) {
  
  if(gcr_bits >= 10) {
    unsigned short val = gcr_val & 0x3ff;
    *raw_data = (unsigned char) ((GCR_decode[val >> 5] << 4) |
				 (GCR_decode[val & 0x1f]));
    gcr_val >>= 10;
    gcr_bits -= 10;
    return 1;
    }
  return 0;
  }

/*
static const char encoded[] = {
  0x4a, 0x25, 0xa5, 0xfc, 0x96, 0xff, 0xff, 0xb5, 0xd4, 0x5a, 0xea, 0xff, 0xff, 0xaa, 0xd3, 0xff
};

int main(int argc, char **argv) {
  // unsigned char c[] = "testing gcr 1 2 3 4 5 6...";
  unsigned char c[] = { 0, 8, 0xe0, 0x2b, 0xac, 0x10, 0x01, 0x11, 0x50, 0xff, 0xf4, 0xa4, 0x00 };
  unsigned char c2[200];
  int pos=0, pos2=0, i=0;

  printf("Testing GCR on: %s \n", c);

  gcr_init();
  for(i=0; i < sizeof(c); i++) {
    gcr_encode(c[i]);
    while(gcr_get_encoded(&c2[pos])) {
      printf("%02x=>%02x ", c[i], c2[pos]);
      pos++;
    }
  }
  printf("\n");
  printf("Encoded result %d chars (from %d) \n", pos, i);
  gcr_init();
  for (i = 0; i < pos; i++) {
    gcr_decode(c2[i]);
    if(!gcr_valid()) {
      printf("GCR: not valid\n");
    }
    while(gcr_get_decoded(&c[pos2])) {
      pos2++;
    }
  }
  printf("GCR: %s\n",c);
}
*/
