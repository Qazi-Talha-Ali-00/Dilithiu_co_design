/*
 * Dilithium Key Generation - Educational Implementation
 * This is a simplified version for learning purposes
 * For production, use the official NIST PQC Dilithium library
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// PARAMETERS (Dilithium2 variant)
// ============================================================================
#define Q 8380417          // Prime modulus
#define N 256              // Polynomial degree
#define K 4                // Matrix height
#define L 4                // Matrix width
#define ETA 2              // Secret coefficient bound
#define D 13               // Dropped bits from t
#define SEEDBYTES 32       // Seed size
#define POLYBYTES 32       // Bytes per polynomial coefficient range

// ============================================================================
// POLYNOMIAL STRUCTURE
// ============================================================================
typedef struct {
    int32_t coeffs[N];     // 256 coefficients, each mod Q
} poly;

typedef struct {
    poly vec[K];           // Vector of K polynomials
} polyveck;

typedef struct {
    poly vec[L];           // Vector of L polynomials
} polyvecl;

// ============================================================================
// KEY STRUCTURES
// ============================================================================
typedef struct {
    uint8_t seed[SEEDBYTES];  // Seed for generating A
    polyveck t1;               // High bits of t
} public_key;

typedef struct {
    uint8_t seed[SEEDBYTES];  // Seed for generating A
    polyvecl s1;               // Secret vector 1
    polyveck s2;               // Secret vector 2
    polyveck t0;               // Low bits of t
} secret_key;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/* Reduce coefficient modulo Q */
int32_t reduce_mod_q(int64_t a) {
    int32_t result = a % Q;
    if (result < 0) result += Q;
    return result;
}

/* Initialize polynomial with zeros */
void poly_zero(poly *p) {
    memset(p->coeffs, 0, N * sizeof(int32_t));
}

/* Copy polynomial */
void poly_copy(poly *dst, const poly *src) {
    memcpy(dst->coeffs, src->coeffs, N * sizeof(int32_t));
}

/* Polynomial addition: r = a + b (mod Q) */
void poly_add(poly *r, const poly *a, const poly *b) {
    for (int i = 0; i < N; i++) {
        r->coeffs[i] = reduce_mod_q((int64_t)a->coeffs[i] + b->coeffs[i]);
    }
}

/* Polynomial multiplication (NTT would be used in real implementation) */
void poly_multiply(poly *r, const poly *a, const poly *b) {
    // Simplified convolution (real implementation uses NTT for speed)
    poly_zero(r);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int k = i + j;
            int64_t product = (int64_t)a->coeffs[i] * b->coeffs[j];
            
            // Handle reduction modulo (x^N + 1)
            if (k >= N) {
                k -= N;
                product = -product;
            }
            r->coeffs[k] = reduce_mod_q(r->coeffs[k] + product);
        }
    }
}

/* Split polynomial into high and low bits */
void poly_power2round(poly *t1, poly *t0, const poly *t) {
    for (int i = 0; i < N; i++) {
        int32_t c = t->coeffs[i];
        int32_t mask = (1 << D) - 1;
        t0->coeffs[i] = c & mask;
        t1->coeffs[i] = (c - t0->coeffs[i]) >> D;
    }
}

// ============================================================================
// CRYPTOGRAPHIC PRIMITIVES (Simplified)
// ============================================================================

/* 
 * In real Dilithium, this uses SHAKE-128/256
 * Here we use a simplified pseudo-random generator for illustration
 */
void shake256(uint8_t *output, size_t outlen, const uint8_t *input, size_t inlen) {
    // This is a PLACEHOLDER - use a real SHAKE implementation
    // For education: just XOR with counter
    for (size_t i = 0; i < outlen; i++) {
        output[i] = input[i % inlen] ^ (i & 0xFF);
    }
}

/* Generate random seed */
void random_seed(uint8_t *seed) {
    // In production: use OS random number generator
    // For demo: simplified
    for (int i = 0; i < SEEDBYTES; i++) {
        seed[i] = rand() & 0xFF;
    }
}

/* Sample small polynomial with coefficients in [-ETA, ETA] */
void sample_small_poly(poly *p, const uint8_t *seed, uint16_t nonce) {
    // Simplified sampling - real version uses rejection sampling
    uint8_t buf[N * 2];
    uint8_t expanded_seed[SEEDBYTES + 2];
    
    memcpy(expanded_seed, seed, SEEDBYTES);
    expanded_seed[SEEDBYTES] = nonce & 0xFF;
    expanded_seed[SEEDBYTES + 1] = nonce >> 8;
    
    shake256(buf, N * 2, expanded_seed, SEEDBYTES + 2);
    
    for (int i = 0; i < N; i++) {
        // Map to [-ETA, ETA]
        int32_t val = (buf[i] % (2 * ETA + 1)) - ETA;
        p->coeffs[i] = val;
    }
}

/* Expand seed into matrix A (k x l matrix of polynomials) */
void expand_matrix_a(poly A[K][L], const uint8_t *seed) {
    for (int i = 0; i < K; i++) {
        for (int j = 0; j < L; j++) {
            uint8_t expanded[SEEDBYTES + 2];
            memcpy(expanded, seed, SEEDBYTES);
            expanded[SEEDBYTES] = i;
            expanded[SEEDBYTES + 1] = j;
            
            uint8_t poly_seed[N * 4];
            shake256(poly_seed, N * 4, expanded, SEEDBYTES + 2);
            
            // Convert bytes to polynomial coefficients
            for (int k = 0; k < N; k++) {
                uint32_t val = 0;
                for (int b = 0; b < 3; b++) {
                    val |= ((uint32_t)poly_seed[k * 3 + b]) << (8 * b);
                }
                A[i][j].coeffs[k] = val % Q;
            }
        }
    }
}

// ============================================================================
// MATRIX-VECTOR OPERATIONS
// ============================================================================

/* Matrix-vector multiplication: result = A * s1 (k x l matrix times l vector) */
void matrix_vector_multiply(polyveck *result, poly A[K][L], const polyvecl *s1) {
    for (int i = 0; i < K; i++) {
        poly_zero(&result->vec[i]);
        for (int j = 0; j < L; j++) {
            poly temp;
            poly_multiply(&temp, &A[i][j], &s1->vec[j]);
            poly_add(&result->vec[i], &result->vec[i], &temp);
        }
    }
}

// ============================================================================
// KEY GENERATION - MAIN ALGORITHM
// ============================================================================

void dilithium_keygen(public_key *pk, secret_key *sk) {
    poly A[K][L];              // Public matrix
    polyveck t;                // t = A*s1 + s2
    uint8_t secret_seed[SEEDBYTES];
    
    printf("Step 1: Generating random seed...\n");
    random_seed(pk->seed);
    random_seed(secret_seed);
    
    printf("Step 2: Expanding seed into matrix A (%dx%d)...\n", K, L);
    expand_matrix_a(A, pk->seed);
    
    printf("Step 3: Sampling secret vector s1 (length %d)...\n", L);
    for (int i = 0; i < L; i++) {
        sample_small_poly(&sk->s1.vec[i], secret_seed, i);
    }
    
    printf("Step 4: Sampling secret vector s2 (length %d)...\n", K);
    for (int i = 0; i < K; i++) {
        sample_small_poly(&sk->s2.vec[i], secret_seed, L + i);
    }
    
    printf("Step 5: Computing t = A * s1 + s2...\n");
    matrix_vector_multiply(&t, A, &sk->s1);
    for (int i = 0; i < K; i++) {
        poly_add(&t.vec[i], &t.vec[i], &sk->s2.vec[i]);
    }
    
    printf("Step 6: Splitting t into high (t1) and low (t0) bits...\n");
    for (int i = 0; i < K; i++) {
        poly_power2round(&pk->t1.vec[i], &sk->t0.vec[i], &t.vec[i]);
    }
    
    printf("Step 7: Packaging keys...\n");
    memcpy(sk->seed, pk->seed, SEEDBYTES);
    
    printf("\n✓ Key generation complete!\n");
    printf("  Public key size: ~%zu bytes\n", 
           SEEDBYTES + K * N * sizeof(int32_t) / 8);
    printf("  Secret key size: ~%zu bytes\n", 
           SEEDBYTES + (L + 2*K) * N * sizeof(int32_t) / 8);
}

// ============================================================================
// DEMO MAIN FUNCTION
// ============================================================================

int main() {
    public_key pk;
    secret_key sk;
    
    printf("=== Dilithium Key Generation Demo ===\n\n");
    printf("Parameters:\n");
    printf("  Prime modulus Q = %d\n", Q);
    printf("  Polynomial degree N = %d\n", N);
    printf("  Matrix dimensions = %dx%d\n", K, L);
    printf("  Secret bound η = %d\n\n", ETA);
    
    dilithium_keygen(&pk, &sk);
    
    printf("\nSample secret coefficients (s1[0], first 8):\n  ");
    for (int i = 0; i < 8; i++) {
        printf("%d ", sk.s1.vec[0].coeffs[i]);
    }
    
    printf("\n\nSample public coefficients (t1[0], first 8):\n  ");
    for (int i = 0; i < 8; i++) {
        printf("%d ", pk.t1.vec[0].coeffs[i]);
    }
    printf("\n");
    
    return 0;
}
