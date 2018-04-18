#include "vkvg_matrix.h"

#define ISFINITE(x) ((x) * (x) >= 0.) /* check for NaNs */

//matrix computations mainly taken from http://cairographics.org
static void _vkvg_matrix_scalar_multiply (vkvg_matrix_t *matrix, float scalar)
{
    matrix->xx *= scalar;
    matrix->yx *= scalar;

    matrix->xy *= scalar;
    matrix->yy *= scalar;

    matrix->x0 *= scalar;
    matrix->y0 *= scalar;
}
void _vkvg_matrix_get_affine (const vkvg_matrix_t *matrix,
              float *xx, float *yx,
              float *xy, float *yy,
              float *x0, float *y0)
{
    *xx  = matrix->xx;
    *yx  = matrix->yx;

    *xy  = matrix->xy;
    *yy  = matrix->yy;

    if (x0)
    *x0 = matrix->x0;
    if (y0)
    *y0 = matrix->y0;
}
static void _vkvg_matrix_compute_adjoint (vkvg_matrix_t *matrix)
{
    /* adj (A) = transpose (C:cofactor (A,i,j)) */
    float a, b, c, d, tx, ty;

    _vkvg_matrix_get_affine (matrix,
                  &a,  &b,
                  &c,  &d,
                  &tx, &ty);

    vkvg_matrix_init (matrix,
               d, -b,
               -c, a,
               c*ty - d*tx, b*tx - a*ty);
}
float _vkvg_matrix_compute_determinant (const vkvg_matrix_t *matrix)
{
    float a, b, c, d;

    a = matrix->xx; b = matrix->yx;
    c = matrix->xy; d = matrix->yy;

    return a*d - b*c;
}
void vkvg_matrix_invert (vkvg_matrix_t *matrix)
{
    float det;

    /* Simple scaling|translation matrices are quite common... */
    if (matrix->xy == 0. && matrix->yx == 0.) {
        matrix->x0 = -matrix->x0;
        matrix->y0 = -matrix->y0;

        if (matrix->xx != 1.) {
            if (matrix->xx == 0.)
            return;

            matrix->xx = 1. / matrix->xx;
            matrix->x0 *= matrix->xx;
        }

        if (matrix->yy != 1.) {
            if (matrix->yy == 0.)
            return;

            matrix->yy = 1. / matrix->yy;
            matrix->y0 *= matrix->yy;
        }

        return;
    }

    /* inv (A) = 1/det (A) * adj (A) */
    det = _vkvg_matrix_compute_determinant (matrix);

    if (! ISFINITE (det))
        return;

    if (det == 0)
        return;

    _vkvg_matrix_compute_adjoint (matrix);
    _vkvg_matrix_scalar_multiply (matrix, 1 / det);
}
void vkvg_matrix_init_identity (vkvg_matrix_t *matrix)
{
    vkvg_matrix_init (matrix,
               1, 0,
               0, 1,
               0, 0);
}

void vkvg_matrix_init (vkvg_matrix_t *matrix,
           float xx, float yx,
           float xy, float yy,
           float x0, float y0)
{
    matrix->xx = xx; matrix->yx = yx;
    matrix->xy = xy; matrix->yy = yy;
    matrix->x0 = x0; matrix->y0 = y0;
}

void vkvg_matrix_init_translate (vkvg_matrix_t *matrix, float tx, float ty)
{
    vkvg_matrix_init (matrix,
               1, 0,
               0, 1,
               tx, ty);
}
void vkvg_matrix_init_scale (vkvg_matrix_t *matrix, float sx, float sy)
{
    vkvg_matrix_init (matrix,
               sx,  0,
               0, sy,
               0, 0);
}
void vkvg_matrix_init_rotate (vkvg_matrix_t *matrix, float radians)
{
    float  s;
    float  c;

    s = sin (radians);
    c = cos (radians);

    vkvg_matrix_init (matrix,
               c, s,
               -s, c,
               0, 0);
}
void vkvg_matrix_translate (vkvg_matrix_t *matrix, float tx, float ty)
{
    vkvg_matrix_t tmp;

    vkvg_matrix_init_translate (&tmp, tx, ty);

    vkvg_matrix_multiply (matrix, &tmp, matrix);
}
void vkvg_matrix_scale (vkvg_matrix_t *matrix, float sx, float sy)
{
    vkvg_matrix_t tmp;

    vkvg_matrix_init_scale (&tmp, sx, sy);

    vkvg_matrix_multiply (matrix, &tmp, matrix);
}
void vkvg_matrix_rotate (vkvg_matrix_t *matrix, float radians)
{
    vkvg_matrix_t tmp;

    vkvg_matrix_init_rotate (&tmp, radians);

    vkvg_matrix_multiply (matrix, &tmp, matrix);
}
void vkvg_matrix_multiply (vkvg_matrix_t *result, const vkvg_matrix_t *a, const vkvg_matrix_t *b)
{
    vkvg_matrix_t r;

    r.xx = a->xx * b->xx + a->yx * b->xy;
    r.yx = a->xx * b->yx + a->yx * b->yy;

    r.xy = a->xy * b->xx + a->yy * b->xy;
    r.yy = a->xy * b->yx + a->yy * b->yy;

    r.x0 = a->x0 * b->xx + a->y0 * b->xy + b->x0;
    r.y0 = a->x0 * b->yx + a->y0 * b->yy + b->y0;

    *result = r;
}
void vkvg_matrix_transform_distance (const vkvg_matrix_t *matrix, float *dx, float *dy)
{
    float new_x, new_y;

    new_x = (matrix->xx * *dx + matrix->xy * *dy);
    new_y = (matrix->yx * *dx + matrix->yy * *dy);

    *dx = new_x;
    *dy = new_y;
}
void vkvg_matrix_transform_point (const vkvg_matrix_t *matrix, float *x, float *y)
{
    vkvg_matrix_transform_distance (matrix, x, y);

    *x += matrix->x0;
    *y += matrix->y0;
}
