/*
 * SHAKE-128/256 Implementation
 * Educational implementation of the Keccak sponge construction
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// ============================================================================
// KECCAK PARAMETERS
// ============================================================================
#define KECCAK_ROUNDS 24
#define STATE_SIZE 25      // 5x5 array of 64-bit lanes

// SHAKE-128: rate = 168 bytes (1344 bits), capacity = 256 bits
// SHAKE-256: rate = 136 bytes (1088 bits), capacity = 512 bits
#define SHAKE128_RATE 168
#define SHAKE256_RATE 136

// ============================================================================
// KECCAK STATE
// ============================================================================
typedef struct {
    uint64_t state[STATE_SIZE];  // 5x5x64 = 1600 bits
    size_t rate;                  // Rate in bytes
    size_t absorb_pos;           // Current position in absorbing
} keccak_state;

// ============================================================================
// ROTATION OFFSETS (for ρ step)
// ============================================================================
static const int keccak_rotations[25] = {
    0,  1,  62, 28, 27,
    36, 44, 6,  55, 20,
    3,  10, 43, 25, 39,
    41, 45, 15, 21, 8,
    18, 2,  61, 56, 14
};

// ============================================================================
// ROUND CONSTANTS (for ι step)
// ============================================================================
static const uint64_t keccak_round_constants[KECCAK_ROUNDS] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808AULL, 0x8000000080008000ULL,
    0x000000000000808BULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008AULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800AULL, 0x800000008000000AULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/* Rotate left */
static inline uint64_t rotl64(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

/* Convert 5x5 index to linear index */
static inline int idx(int x, int y) {
    return y * 5 + x;
}

/* Print state (for debugging) */
void print_state(const keccak_state *ctx, const char *label) {
    printf("\n%s:\n", label);
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            printf("%016llx ", (unsigned long long)ctx->state[idx(x, y)]);
        }
        printf("\n");
    }
}

// ============================================================================
// KECCAK-f[1600] PERMUTATION (The Core Scrambling Function)
// ============================================================================

void keccak_f1600(uint64_t state[STATE_SIZE]) {
    uint64_t C[5], D[5], B[25];
    
    // Apply 24 rounds of permutation
    for (int round = 0; round < KECCAK_ROUNDS; round++) {
        
        // ----------------------------------------------------------------
        // θ (THETA) - Column parity mixing
        // ----------------------------------------------------------------
        // Compute parity of columns
        for (int x = 0; x < 5; x++) {
            C[x] = state[idx(x, 0)] ^ state[idx(x, 1)] ^ 
                   state[idx(x, 2)] ^ state[idx(x, 3)] ^ 
                   state[idx(x, 4)];
        }
        
        // Mix columns
        for (int x = 0; x < 5; x++) {
            D[x] = C[(x + 4) % 5] ^ rotl64(C[(x + 1) % 5], 1);
        }
        
        // Apply to state
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                state[idx(x, y)] ^= D[x];
            }
        }
        
        // ----------------------------------------------------------------
        // ρ (RHO) and π (PI) - Rotation and permutation
        // ----------------------------------------------------------------
        // Combined for efficiency
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                int new_x = y;
                int new_y = (2 * x + 3 * y) % 5;
                B[idx(new_x, new_y)] = rotl64(state[idx(x, y)], 
                                              keccak_rotations[idx(x, y)]);
            }
        }
        
        // ----------------------------------------------------------------
        // χ (CHI) - Non-linear mixing
        // ----------------------------------------------------------------
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                state[idx(x, y)] = B[idx(x, y)] ^ 
                    ((~B[idx((x + 1) % 5, y)]) & B[idx((x + 2) % 5, y)]);
            }
        }
        
        // ----------------------------------------------------------------
        // ι (IOTA) - Add round constant
        // ----------------------------------------------------------------
        state[0] ^= keccak_round_constants[round];
    }
}

// ============================================================================
// SPONGE CONSTRUCTION
// ============================================================================

/* Initialize SHAKE context */
void shake_init(keccak_state *ctx, int shake_bits) {
    memset(ctx, 0, sizeof(keccak_state));
    
    if (shake_bits == 128) {
        ctx->rate = SHAKE128_RATE;
    } else if (shake_bits == 256) {
        ctx->rate = SHAKE256_RATE;
    } else {
        fprintf(stderr, "Invalid SHAKE variant\n");
        ctx->rate = SHAKE128_RATE;
    }
    
    ctx->absorb_pos = 0;
}

/* Absorb input data into the sponge */
void shake_absorb(keccak_state *ctx, const uint8_t *input, size_t inlen) {
    uint8_t *state_bytes = (uint8_t *)ctx->state;
    
    printf("\nAbsorbing %zu bytes...\n", inlen);
    
    for (size_t i = 0; i < inlen; i++) {
        // XOR input byte into state at rate region
        state_bytes[ctx->absorb_pos] ^= input[i];
        ctx->absorb_pos++;
        
        // When rate is full, permute and reset position
        if (ctx->absorb_pos == ctx->rate) {
            printf("  Rate full, applying permutation...\n");
            keccak_f1600(ctx->state);
            ctx->absorb_pos = 0;
        }
    }
}

/* Finalize absorption phase */
void shake_finalize(keccak_state *ctx) {
    uint8_t *state_bytes = (uint8_t *)ctx->state;
    
    printf("\nFinalizing absorption...\n");
    
    // SHAKE domain separation: append 0x1F
    state_bytes[ctx->absorb_pos] ^= 0x1F;
    
    // Padding: set last bit of rate to 1
    state_bytes[ctx->rate - 1] ^= 0x80;
    
    // Final permutation
    keccak_f1600(ctx->state);
    ctx->absorb_pos = 0;  // Reset for squeezing
}

/* Squeeze output data from the sponge */
void shake_squeeze(keccak_state *ctx, uint8_t *output, size_t outlen) {
    uint8_t *state_bytes = (uint8_t *)ctx->state;
    
    printf("\nSqueezing %zu bytes...\n", outlen);
    
    for (size_t i = 0; i < outlen; i++) {
        // If we've used all rate bytes, permute to get more
        if (ctx->absorb_pos == ctx->rate) {
            printf("  Rate exhausted, applying permutation...\n");
            keccak_f1600(ctx->state);
            ctx->absorb_pos = 0;
        }
        
        // Extract byte from state
        output[i] = state_bytes[ctx->absorb_pos];
        ctx->absorb_pos++;
    }
}

// ============================================================================
// HIGH-LEVEL SHAKE API
// ============================================================================

void shake128(uint8_t *output, size_t outlen, 
              const uint8_t *input, size_t inlen) {
    keccak_state ctx;
    
    printf("\n=== SHAKE-128 ===");
    printf("\nInput length: %zu bytes", inlen);
    printf("\nOutput length: %zu bytes\n", outlen);
    
    shake_init(&ctx, 128);
    shake_absorb(&ctx, input, inlen);
    shake_finalize(&ctx);
    shake_squeeze(&ctx, output, outlen);
}

void shake256(uint8_t *output, size_t outlen, 
              const uint8_t *input, size_t inlen) {
    keccak_state ctx;
    
    printf("\n=== SHAKE-256 ===");
    printf("\nInput length: %zu bytes", inlen);
    printf("\nOutput length: %zu bytes\n", inlen);
    
    shake_init(&ctx, 256);
    shake_absorb(&ctx, input, inlen);
    shake_finalize(&ctx);
    shake_squeeze(&ctx, output, outlen);
}

// ============================================================================
// DEMO AND TESTING
// ============================================================================

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if (i < len - 1 && (i + 1) % 32 == 0) printf("\n     ");
    }
    printf("\n");
}

int main() {
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║     SHAKE Algorithm Implementation Demo        ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    // Test input
    const char *message = "Hello, Dilithium!";
    uint8_t output128[64];
    uint8_t output256[64];
    
    printf("\n📝 Input message: \"%s\"\n", message);
    printf("   Length: %zu bytes\n", strlen(message));
    
    // ========================================================================
    // SHAKE-128 Demo
    // ========================================================================
    shake128(output128, sizeof(output128), 
             (const uint8_t *)message, strlen(message));
    
    printf("\n✓ SHAKE-128 complete!\n");
    print_hex("Output (64 bytes)", output128, sizeof(output128));
    
    // ========================================================================
    // SHAKE-256 Demo
    // ========================================================================
    shake256(output256, sizeof(output256), 
             (const uint8_t *)message, strlen(message));
    
    printf("\n✓ SHAKE-256 complete!\n");
    print_hex("Output (64 bytes)", output256, sizeof(output256));
    
    // ========================================================================
    // Demonstrate extendable output
    // ========================================================================
    printf("\n\n╔════════════════════════════════════════════════╗\n");
    printf("║        Extendable Output Feature Demo          ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    uint8_t small_output[16];
    uint8_t large_output[256];
    
    printf("\nSame input, different output lengths:\n");
    
    shake128(small_output, sizeof(small_output), 
             (const uint8_t *)message, strlen(message));
    print_hex("\n16-byte output", small_output, sizeof(small_output));
    
    shake128(large_output, sizeof(large_output), 
             (const uint8_t *)message, strlen(message));
    print_hex("\n256-byte output", large_output, 32);  // Show first 32
    printf("     ... (%zu more bytes)\n", sizeof(large_output) - 32);
    
    // ========================================================================
    // Show consistency
    // ========================================================================
    printf("\n\n╔════════════════════════════════════════════════╗\n");
    printf("║           Consistency Verification             ║\n");
    printf("╚════════════════════════════════════════════════╝\n");
    
    int consistent = 1;
    for (size_t i = 0; i < sizeof(small_output); i++) {
        if (small_output[i] != large_output[i]) {
            consistent = 0;
            break;
        }
    }
    
    printf("\nFirst 16 bytes of 256-byte output match 16-byte output: %s\n",
           consistent ? "✓ YES (Extendable property works!)" : "✗ NO");
    
    return 0;
}