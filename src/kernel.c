#include <math.h> // cos, sin, tan

#if defined OPT1

/* Inversion des boucles i et j pour accès mémoire contigu */

void kernel(unsigned n, float a[n][n], const float b[n]) {
    unsigned i, j;

    for (i = 0; i < n; i++) {
        unsigned mod = i % 4;   
        for (j = 0; j < n; j++) {
            if (mod == 0)
                a[i][j] = sin(b[i]) / cos(b[j]);
            else if (mod == 1)
                a[i][j] = tan(b[i]) / tan(b[j]);
            else if (mod == 2)
                a[i][j] = sin(b[j]) / cos(b[i]);
            else
                a[i][j] = tan(b[j]) / tan(b[i]);
        }
    }
}

#elif defined OPT2

/*   OPT1
   + précalcul des termes dépendant de i (invariants sur j)
   + sinf/cosf/tanf pour float 32 bits
   + mod hors de la boucle j */

void kernel(unsigned n, float a[n][n], const float b[n]) {
    unsigned i, j;

    for (i = 0; i < n; i++) {
        unsigned mod = i % 4;

        float ci;
        if (mod == 0)      ci = sinf(b[i]);
        else if (mod == 1) ci = tanf(b[i]);
        else if (mod == 2) ci = 1.0f / cosf(b[i]);
        else               ci = 1.0f / tanf(b[i]);

        for (j = 0; j < n; j++) {
            if (mod == 0)      a[i][j] = ci / cosf(b[j]);
            else if (mod == 1) a[i][j] = ci / tanf(b[j]);
            else if (mod == 2) a[i][j] = sinf(b[j]) * ci;
            else               a[i][j] = tanf(b[j]) * ci;
        }
    }
}

#elif defined OPT3

/*   OPT2
   + loop unrolling sur j (multiple de 4), boucle i externe pour localité mémoire */

void kernel(unsigned n, float a[n][n], const float b[n]) {
    unsigned i, j;

    for (i = 0; i < n; i++) {
        unsigned mod = i % 4;

        float ci;
        if (mod == 0)      ci = sinf(b[i]);
        else if (mod == 1) ci = tanf(b[i]);
        else if (mod == 2) ci = 1.0f / cosf(b[i]);
        else               ci = 1.0f / tanf(b[i]);

        // Boucle j déroulée par 4 : les 4 cas sont connus à la compilation
        // car mod est constant sur toute la boucle j
        if (mod == 0) {
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = ci / cosf(b[j]);
                a[i][j+1] = ci / cosf(b[j+1]);
                a[i][j+2] = ci / cosf(b[j+2]);
                a[i][j+3] = ci / cosf(b[j+3]);
            }
            for (; j < n; j++) a[i][j] = ci / cosf(b[j]);
        }
        else if (mod == 1) {
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = ci / tanf(b[j]);
                a[i][j+1] = ci / tanf(b[j+1]);
                a[i][j+2] = ci / tanf(b[j+2]);
                a[i][j+3] = ci / tanf(b[j+3]);
            }
            for (; j < n; j++) a[i][j] = ci / tanf(b[j]);
        }
        else if (mod == 2) {
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = sinf(b[j])   * ci;
                a[i][j+1] = sinf(b[j+1]) * ci;
                a[i][j+2] = sinf(b[j+2]) * ci;
                a[i][j+3] = sinf(b[j+3]) * ci;
            }
            for (; j < n; j++) a[i][j] = sinf(b[j]) * ci;
        }
        else {
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = tanf(b[j])   * ci;
                a[i][j+1] = tanf(b[j+1]) * ci;
                a[i][j+2] = tanf(b[j+2]) * ci;
                a[i][j+3] = tanf(b[j+3]) * ci;
            }
            for (; j < n; j++) a[i][j] = tanf(b[j]) * ci;
        }
    }
}

#elif defined OPT4
#include <stdlib.h>

/*   OPT3
   + précalcul complet de b[i] ET b[j] dans des tableaux
   + utilisation de malloc pour éviter le stack overflow */

void kernel(unsigned n, float a[n][n], const float b[n]) {
    unsigned i, j;

    // Précalcul global et malloc pour éviter le stack overflow sur grand n
    float *pre_sin     = malloc(n * sizeof(float));
    float *pre_tan     = malloc(n * sizeof(float));
    float *pre_inv_cos = malloc(n * sizeof(float));
    float *pre_inv_tan = malloc(n * sizeof(float));

    for (i = 0; i < n; i++) {
        pre_sin[i]     = sinf(b[i]);
        pre_tan[i]     = tanf(b[i]);
        pre_inv_cos[i] = 1.0f / cosf(b[i]);
        pre_inv_tan[i] = 1.0f / tanf(b[i]);
    }

    // Boucle i externe (accès a[i][j] contigus), mod constant sur j
    for (i = 0; i < n; i++) {
        unsigned mod = i % 4;

        if (mod == 0) {
            float ci = pre_sin[i];
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = ci * pre_inv_cos[j];
                a[i][j+1] = ci * pre_inv_cos[j+1];
                a[i][j+2] = ci * pre_inv_cos[j+2];
                a[i][j+3] = ci * pre_inv_cos[j+3];
            }
            for (; j < n; j++) a[i][j] = ci * pre_inv_cos[j];
        }
        else if (mod == 1) {
            float ci = pre_tan[i];
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = ci * pre_inv_tan[j];
                a[i][j+1] = ci * pre_inv_tan[j+1];
                a[i][j+2] = ci * pre_inv_tan[j+2];
                a[i][j+3] = ci * pre_inv_tan[j+3];
            }
            for (; j < n; j++) a[i][j] = ci * pre_inv_tan[j];
        }
        else if (mod == 2) {
            float ci = pre_inv_cos[i];
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = pre_sin[j]   * ci;
                a[i][j+1] = pre_sin[j+1] * ci;
                a[i][j+2] = pre_sin[j+2] * ci;
                a[i][j+3] = pre_sin[j+3] * ci;
            }
            for (; j < n; j++) a[i][j] = pre_sin[j] * ci;
        }
        else { // mod == 3
            float ci = pre_inv_tan[i];
            for (j = 0; j + 3 < n; j += 4) {
                a[i][j]   = pre_tan[j]   * ci;
                a[i][j+1] = pre_tan[j+1] * ci;
                a[i][j+2] = pre_tan[j+2] * ci;
                a[i][j+3] = pre_tan[j+3] * ci;
            }
            for (; j < n; j++) a[i][j] = pre_tan[j] * ci;
        }
    }

    free(pre_sin);
    free(pre_tan);
    free(pre_inv_cos);
    free(pre_inv_tan);
}

#else

/* original */
void kernel(unsigned n, float a[n][n],
              const float b[n]){
    unsigned i, j;

    for(j = 0; j < n; j++){
        for(i = 0; i < n; i++){
            if(i % 4 == 0){
                a[i][j] = sin(b[i]) / cos(b[j]);
            }
            else if(i % 4 == 1){
                a[i][j] = tan(b[i]) / tan(b[j]);
            }
            else if(i % 4 == 2){
                a[i][j] = sin(b[j]) / cos(b[i]);
            }
            else{ // i % 4 == 3
                a[i][j] = tan(b[j]) / tan(b[i]);
            }
        }
    }
}

#endif