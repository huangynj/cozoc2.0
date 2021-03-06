#include "equation.h"
#include "context.h"
#include "grids.h"
#include "io.h"
#include "omega.h"
#include "omegaQG.h"
#include "options.h"

Equations new_equations (Options const options) {
    Equations eqs = {.num_eq = 0, .L = {0}, .a = {0}, .id_string = {""}};
    if (options.compute_omega_quasi_geostrophic) {
        eqs.L[eqs.num_eq] = omega_qg_compute_operator;
        eqs.a[eqs.num_eq] = omega_qg_compute_rhs;
        strcpy (eqs.id_string[eqs.num_eq], OMEGA_QG_ID_STRING);
        eqs.num_eq++;
    }
    if (options.compute_omega_generalized) {
        for (size_t i = 0; i < NUM_GENERALIZED_OMEGA_COMPONENTS; i++) {
            eqs.L[eqs.num_eq] = omega_compute_operator;
            eqs.a[eqs.num_eq] = omega_compute_rhs[i];
            strcpy (eqs.id_string[eqs.num_eq], omega_component_id_string[i]);
            eqs.num_eq++;
        }
    }
    return eqs;
}

Vec solution (
    ComputeLHSOperatorFunction L, ComputeRHSVectorFunction a, Context ctx) {
    Vec x;
    KSPSetComputeOperators (ctx.ksp, L, &ctx);
    KSPSetComputeRHS (ctx.ksp, a, &ctx);
    KSPSolve (ctx.ksp, 0, 0);
    KSPGetSolution (ctx.ksp, &x);
    return x;
}
