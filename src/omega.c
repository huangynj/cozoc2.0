#include "defs.h"
#include "constants.h"
#include "context.h"
#include "omega.h"
#include "io.h"
#include "ops.h"
#include <petscdmda.h>
#include <petscksp.h>

#include "spherical_omega_stencil.inc"


char omega_component_id_string[NUM_GENERALIZED_OMEGA_COMPONENTS][NC_MAX_NAME] = {
    "ome_v", "ome_t", "ome_f", "ome_q", "ome_a" };

PetscErrorCode (*omega_compute_rhs[NUM_GENERALIZED_OMEGA_COMPONENTS]) (
    KSP ksp, Vec b, void* ctx_p) = {omega_compute_rhs_F_V,
                                    omega_compute_rhs_F_T,
                                    omega_compute_rhs_F_F,
                                    omega_compute_rhs_F_Q,
                                    omega_compute_rhs_F_A };


/* *
 * The LHS of the generalized omega equation
 *
 */

extern PetscErrorCode omega_compute_operator (
    KSP ksp, Mat A, Mat B, void* ctx_p) {

    Context*             ctx = (Context*) ctx_p;
    DM                   da  = ctx->da;
    DM                   da2 = ctx->da2;
    PetscScalar          hx  = ctx->hx;
    PetscScalar          hy  = ctx->hy;
    PetscScalar          hz  = ctx->hz;
    PetscScalar*         f   = ctx->Coriolis_parameter;
    PetscScalar*         lat   = ctx->Latitude;
    PetscScalar*         p   = ctx->Pressure;
    Vec                  sigmavec, zetavec, Vvec;
    const PetscScalar ***sigma, ***zeta, ****V;
    PetscInt             my = ctx->my;
    PetscInt             mz = ctx->mz;
    PetscInt             n;
    MatStencil           row, col[15];
    PetscScalar          w[15];
    PetscInt             xs, ys, zs, xm, ym, zm;
    const double         r = earth_radius;

    DMGetLocalVector (da, &sigmavec);
    DMGetLocalVector (da, &zetavec);
    DMGetLocalVector (da2, &Vvec);

    DMGlobalToLocalBegin (
        da, ctx->Sigma_parameter, INSERT_VALUES, sigmavec);
    DMGlobalToLocalEnd (
        da, ctx->Sigma_parameter, INSERT_VALUES, sigmavec);
    DMGlobalToLocalBegin (da, ctx->Vorticity, INSERT_VALUES, zetavec);
    DMGlobalToLocalEnd (da, ctx->Vorticity, INSERT_VALUES, zetavec);
    DMGlobalToLocalBegin (
        da2, ctx->Horizontal_wind, INSERT_VALUES, Vvec);
    DMGlobalToLocalEnd (da2, ctx->Horizontal_wind, INSERT_VALUES, Vvec);

    ellipticity_sigma_vorticity(ctx,mz, p, f, sigmavec, zetavec, Vvec);

    DMDAVecGetArrayRead (da, sigmavec, &sigma);
    DMDAVecGetArrayRead (da, zetavec, &zeta);
    DMDAVecGetArrayDOFRead (da2, Vvec, &V);

    DMDAGetCorners (da, &xs, &ys, &zs, &xm, &ym, &zm);

    info("In omega_compute_operator()\n");

    for (int k = zs; k < zs + zm; k++) {
        for (int j = ys; j < ys + ym; j++) {
            for (int i = xs; i < xs + xm; i++) {
                row.i = i;
                row.j = j;
                row.k = k;

                if (k == 0 || k == mz - 1 || j == 0 || j == my - 1) {
                    w[0] = hx * hy * hz;
                    MatSetValuesStencil (
                        B, 1, &row, 1, &row, w, INSERT_VALUES); }
                else {
                    omega_stencil (
                        i,
                        j,
                        k,
                        hx,
                        hy,
                        hz,
                        r,
                        f,
                        lat,
                        sigma,
                        zeta,
                        V,
                        &n,
                        col,
                        w);
                    MatSetValuesStencil (
                        B, 1, &row, n, col, w, INSERT_VALUES); } } } }


    MatAssemblyBegin (B, MAT_FINAL_ASSEMBLY);
    MatAssemblyEnd (B, MAT_FINAL_ASSEMBLY);

    DMDAVecRestoreArrayRead (da, sigmavec, &sigma);
    DMDAVecRestoreArrayRead (da, zetavec, &zeta);
    DMDAVecRestoreArrayDOFRead (da2, Vvec, &V);

    DMRestoreLocalVector (da, &sigmavec);
    DMRestoreLocalVector (da, &zetavec);
    DMRestoreLocalVector (da2, &Vvec);

    return (0); }


/* *
 * Vorticity advection forcing
 *
 * F_V = f \frac{\partial}{\partial p}
 *               \left[
 *                       \mathbf{V} \cdot \nabla \left( \zeta +f \right)
 *               \right]
 */

extern PetscErrorCode omega_compute_rhs_F_V (
    KSP ksp, Vec b, void* ctx_p) {

    Context*     ctx  = (Context*) ctx_p;
    DM           da   = ctx->da;
    size_t       mz   = ctx->mz;
    PetscScalar* p    = ctx->Pressure;
    Vec          zeta = ctx->Vorticity;
    Vec          V    = ctx->Horizontal_wind;
    Vec          s    = ctx->Surface_attennuation;
    PetscScalar* f    = ctx->Coriolis_parameter;
    Vec          vadv = ctx->Vorticity_advection;
    PetscScalar  hx   = ctx->hx;
    PetscScalar  hy   = ctx->hy;
    PetscScalar  hz   = ctx->hz;
    PetscScalar* lat  = ctx->Latitude;
    PetscScalar*** a;
    PetscInt       i, j, k, zs, ys, xs, zm, ym, xm;
    const double r = earth_radius;

/*    VecCopy (zeta, b);

    field_array1d_add (b, f, DMDA_Y);


    horizontal_advection (b, V, ctx);
    VecCopy (b, vadv);
    VecScale(vadv,-1.0);
*/
    VecCopy(vadv,b);
    VecScale(b,-1.0);
    VecPointwiseMult(b, s, b);
    fpder (da, mz, f, p, b);

    DMDAGetCorners (da, &xs, &ys, &zs, &xm, &ym, &zm);
    DMDAVecGetArray(da, b, &a);
    for (k = zs; k < zs + zm; k++) {
        for (j = ys; j < ys + ym; j++) {
            for (i = xs; i < xs + xm; i++) {
                a[k][j][i] *= r * r * hx * hy * hz * cos(lat[j]); } } }

    DMDAVecRestoreArray (da, b, &a);

    return (0); }


/* *
 * Temperature advection forcing
 *
 * F_T = \frac{R}{p} \nabla^2 ( \mathbf{V} \cdot \nabla \mathbf{T} )
 *
 */

extern PetscErrorCode omega_compute_rhs_F_T (
    KSP ksp, Vec b, void* ctx_p) {

    Context*    ctx = (Context*) ctx_p;
    DM           da   = ctx->da;
    Vec         T   = ctx->Temperature;
    Vec         V   = ctx->Horizontal_wind;
    Vec          s    = ctx->Surface_attennuation;
    PetscScalar hx  = ctx->hx;
    PetscScalar hy  = ctx->hy;
    PetscScalar hz  = ctx->hz;
    PetscScalar* lat  = ctx->Latitude;
    PetscScalar*** a;
    PetscInt       i, j, k, zs, ys, xs, zm, ym, xm;
    const double r = earth_radius;

    VecCopy (T, b);
    horizontal_advection (b, V, ctx);

    VecPointwiseMult(b, s, b);
    plaplace (b, ctx);

    DMDAGetCorners (da, &xs, &ys, &zs, &xm, &ym, &zm);
    DMDAVecGetArray(da, b, &a);
    for (k = zs; k < zs + zm; k++) {
        for (j = ys; j < ys + ym; j++) {
            for (i = xs; i < xs + xm; i++) {
                a[k][j][i] *= r * r * hx * hy * hz * cos(lat[j]); } } }

    DMDAVecRestoreArray (da, b, &a);

    return (0); }


/* *
 * Friction forcing
 *
 * F_F = - f \frac{\partial}{\partial p}
 *               \left(
 *                       \mathbf{k} \cdot \nabla \times \mathbf{F}
 *               \right)
 */

extern PetscErrorCode omega_compute_rhs_F_F (
    KSP ksp, Vec b, void* ctx_p) {

    Context*     ctx = (Context*) ctx_p;
    DM           da  = ctx->da;
    DM           da2 = ctx->da2;
    size_t       my  = ctx->my;
    size_t       mz  = ctx->mz;
    PetscScalar* p   = ctx->Pressure;
    PetscScalar* f   = ctx->Coriolis_parameter;
    PetscScalar* latitude = ctx->Latitude;
    Vec          F   = ctx->Friction;
    Vec          s    = ctx->Surface_attennuation;
    PetscScalar  hx  = ctx->hx;
    PetscScalar  hy  = ctx->hy;
    PetscScalar  hz  = ctx->hz;
    PetscScalar* lat  = ctx->Latitude;
    PetscScalar*** a;
    PetscInt       i, j, k, zs, ys, xs, zm, ym, xm;
    const double r = earth_radius;

    horizontal_rotor (da, da2, my, hx, hy, latitude, F, b);

    VecPointwiseMult(b, s, b);
    fpder (da, mz, f, p, b);

    DMDAGetCorners (da, &xs, &ys, &zs, &xm, &ym, &zm);
    DMDAVecGetArray(da, b, &a);
    for (k = zs; k < zs + zm; k++) {
        for (j = ys; j < ys + ym; j++) {
            for (i = xs; i < xs + xm; i++) {
                a[k][j][i] *= -r * r * hx * hy * hz * cos(lat[j]); } } }

    DMDAVecRestoreArray (da, b, &a);

    return (0); }


/* *
 * Diabatic heating forcing
 *
 * F_Q = -\frac{R}{c_p p} \nabla^2 ( \mathbf{V} \cdot \nabla \mathbf{T}
 * )
 *
 */

extern PetscErrorCode omega_compute_rhs_F_Q (
    KSP ksp, Vec b, void* ctx_p) {

    Context*    ctx = (Context*) ctx_p;
    DM          da  = ctx->da;
    Vec         Qatt   = ctx->Diabatic_heating_tendency;
    Vec          s    = ctx->Surface_attennuation;
    PetscScalar hx  = ctx->hx;
    PetscScalar hy  = ctx->hy;
    PetscScalar hz  = ctx->hz;
    PetscScalar* lat  = ctx->Latitude;
    PetscScalar*** a;
    PetscInt       i, j, k, zs, ys, xs, zm, ym, xm;
    const double r = earth_radius;

    VecCopy (Qatt, b);

    VecPointwiseMult(b, s, b);
    plaplace (b, ctx);

    DMDAGetCorners (da, &xs, &ys, &zs, &xm, &ym, &zm);
    DMDAVecGetArray(da, b, &a);
    for (k = zs; k < zs + zm; k++) {
        for (j = ys; j < ys + ym; j++) {
            for (i = xs; i < xs + xm; i++) {
                a[k][j][i] *= -r * r * hx * hy * hz * cos(lat[j]); } } }

    DMDAVecRestoreArray (da, b, &a);
//    VecScale (b, -hx * hy * hz);

    return (0); }


/* *
 * Ageostrophic vorticity tendency forcing
 *
 * F_A = \left[
 *           f \frac{\partial}{\partial p} \left(
 *               \frac{\partial \zeta}{\partial t} \right)
 *           + \frac{R}{p} \nabla^2 \frac{\partial T}{\partial t}
 *       \right]
 */

extern PetscErrorCode omega_compute_rhs_F_A (
    KSP ksp, Vec b, void* ctx_p) {

    Context*     ctx     = (Context*) ctx_p;
    DM           da      = ctx->da;
    size_t       mz      = ctx->mz;
    PetscScalar* p       = ctx->Pressure;
    PetscScalar* f       = ctx->Coriolis_parameter;
    Vec          dzetadt = ctx->Vorticity_tendency;
    Vec          dTdt    = ctx->Temperature_tendency;
    Vec          s    = ctx->Surface_attennuation;
    PetscScalar  hx      = ctx->hx;
    PetscScalar  hy      = ctx->hy;
    PetscScalar  hz      = ctx->hz;
    Vec          tmpvec;
    PetscScalar* lat  = ctx->Latitude;
    PetscScalar*** a;
    PetscInt       i, j, k, zs, ys, xs, zm, ym, xm;
    const double r = earth_radius;

    DMGetGlobalVector (da, &tmpvec);
    VecCopy (dTdt, b);

    VecPointwiseMult (b, s, b);

    plaplace (b, ctx);
    VecCopy (dzetadt, tmpvec);

    VecPointwiseMult (tmpvec, s, tmpvec);

    fpder (da, mz, f, p, tmpvec);
    VecAXPY (b, 1.0, tmpvec);


    DMRestoreGlobalVector (da, &tmpvec);
    DMDAGetCorners (da, &xs, &ys, &zs, &xm, &ym, &zm);
    DMDAVecGetArray(da, b, &a);
    for (k = zs; k < zs + zm; k++) {
        for (j = ys; j < ys + ym; j++) {
            for (i = xs; i < xs + xm; i++) {
                a[k][j][i] *= r * r * hx * hy * hz * cos(lat[j]); } } }

    DMDAVecRestoreArray (da, b, &a);

//    VecScale (b, hx * hy * hz);

    return (0); }
