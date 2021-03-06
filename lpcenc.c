/*
  LPC voice codec
*/

#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include "lpc.h"


// 最长周期所占样本数,逆滤波器用
static void lpc_auto_correl1(float *w, int n, float *r)
{
    int i, k;
    
    for (k=0; k <= MAXPER; k++, n--) {
        r[k] = 0.0;
        for (i=0; i < n; i++) {
            r[k] += (w[i] *  w[i+k]);
        }
    }
}


//求预测系数用
static void lpc_auto_correl2(float *w, int n, float *r)
{
    int i, k;
    
    for (k=0; k <= LPC_FILTORDER; k++, n--) {
        r[k] = 0.0;
        for (i=0; i < n; i++) {
            r[k] += (w[i] *  w[i+k]);
        }
    }
}


//求逆滤波器用
static void lpc_auto_correl3(float *w, int n, float *r)
{
    int i, k;
    
    for (k=0; k <= PITCHORDER; k++, n--) {
        r[k] = 0.0;
        for (i=0; i < n; i++) {
            r[k] += (w[i] *  w[i+k]);
        }
    }
}


//相关系数转成k，输出误差，作为解//码的激励源
static void lpc_durbin(float *r, int p, float *k, /*@out@*/float *g)
{
    int i, j;
    float a[LPC_FILTORDER + 1], at[LPC_FILTORDER + 1], e;
    
    for (i = 0; i <= p; i++)
        a[i] = at[i] = 0.0;
    
    e = r[0];
    for (i = 1; i <= p; i++) {
        k[i] = -r[i];
        for (j = 1; j < i; j++) {
            at[j] = a[j];
            k[i] -= a[j] * r[i - j];
        }
        if (fabs(e) < FLT_EPSILON) {
            e = 0.0f;
            break;
        }
        k[i] /= e;
        a[i] = k[i];
        for (j = 1; j < i; j++)
            a[j] = at[j] + k[i] * at[i - j];
        e *= 1.0f - k[i] * k[i];
    }
    
    if (e < FLT_EPSILON) {
        e = 0.0f;
    }
    *g = (float) sqrt(e);
}


//求残差，其结果用来进行基因周期检测
static void lpc_inverse_filter(float *w, float *k)
{
    int i, j;
    float b[PITCHORDER + 1], bp[PITCHORDER + 1], f[PITCHORDER + 1];
    
    for (i = 0; i <= PITCHORDER; i++)
        b[i] = f[i] = bp[i] = 0.0;
    
    for (i = 0; i < BUFLEN / DOWN; i++) {
        f[0] = b[0] = w[i];
        for (j = 1; j <= PITCHORDER; j++) {
            f[j] = f[j - 1] + k[j] * bp[j - 1];
            b[j] = k[j] * f[j - 1] + bp[j - 1];
            bp[j - 1] = b[j - 1];
        }
        w[i] = f[PITCHORDER];
    }
}


static void lpc_calc_pitch(float *w, /*@out@*/float *per, int *vuv)
{
    int i, j, rpos;
    float d[BUFLEN / DOWN]; /* changed MAXWINDOW to BUFLEN*/
    float k[PITCHORDER + 1];
    //float r[MAXPER + 1];
    float r[33];
    float g, rmax;
    float rval, rm, rp;
    float a, b, c, x, y;
    
    /* New: average rather than decimating. */
    for (i = 0, j = 0; i < BUFLEN; j++) {
        d[j] = 0;
        for (rpos = 0; rpos < DOWN; rpos++) {
            d[j] += w[i++];
        }
        /* d[j] /- DOWN;	Not actually necessary. */	
    }
    lpc_auto_correl3(d, BUFLEN / DOWN, r);
    lpc_durbin(r, PITCHORDER, k, &g);
    lpc_inverse_filter(d, k);
    lpc_auto_correl1(d, BUFLEN / DOWN, r);
    rpos = 0;
    rmax = 0.0;
    for (i = MINPER; i <= MAXPER; i++) {
        if (r[i] > rmax) {
            rmax = r[i];
            rpos = i;
        }
    }
    
    rm = r[rpos - 1];
    rp = r[rpos + 1];
    
    a = 0.5f * rm - rmax + 0.5f * rp;
    b = -0.5f * rm * (2.0f * rpos + 1.0f) + 
        2.0f * rpos * rmax + 0.5f * rp * (1.0f - 2.0f * rpos);
    c = 0.5f * rm * (rpos * rpos + rpos) +
        rmax * (1.0f - rpos * rpos) + 0.5f * rp * (rpos * rpos - rpos);
    
    x = -b / (2.0f * a);
    y = a * x * x + b * x + c;
    x *= DOWN;
    
    rmax = y;
    if (r[0] == 0.0f) {
        rval = 1.0;
    } else {
        rval = rmax / r[0];
    }
    if ((rval >= 0.4 || (*vuv == 3 && rval >= 0.3)) && (x > 0.0f)) {
        *per = x;
        *vuv = (*vuv & 1) * 2 + 1;
    } else {
        *per = 0.0;
        *vuv = (*vuv & 1) * 2;
    }
}


void init_lpc_encoder_state(lpc_encoder_state *st)
{
    int 	i;
    float	r, v, w, wcT;
    
    for (i = 0; i < BUFLEN; i++) {
        st->w_s[i] = 0.0;
        st->w_h[i] = (float) (WSCALE * (0.54f - 0.46f *
            (float)cos(2.0 * M_PI * i / (BUFLEN - 1.0))));
    }
    wcT = (float) (2 * M_PI * FC / FS);
    r = (float) (0.36891079 * wcT);
    v = (float) (0.18445539 * wcT);
    w = (float) (0.92307712 * wcT);
    st->fa[1] = (float) (-exp(-r));
    st->fa[2] = 1.0f + st->fa[1];
    st->fa[3] = (float) (-2.0 * exp(-v) * cos(w));
    st->fa[4] = (float) exp(-2.0 * v);
    st->fa[5] = 1.0f + st->fa[3] + st->fa[4];
    
    st->u1 = 0.0;
    st->yp1 = 0.0;
    st->yp2 = 0.0;
    st->vuv = 0;
}


int lpc_encode(const short *in, unsigned char *out, lpc_encoder_state *st)
{
    int 	i, j;
    float	r[LPC_FILTORDER + 1];
    float	per, G, k[LPC_FILTORDER + 1];
    
    for (i = 0, j = BUFLEN - LPC_SAMPLES_PER_FRAME; i < LPC_SAMPLES_PER_FRAME; i++, j++) {
        st->w_s[j] = (float) (GAIN_ADJUST * ((*in++) / 32768.0f));
        
        st->u = st->fa[2] * st->w_s[j] - st->fa[1] * st->u1;
        st->w_y[j] = st->fa[5] * st->u1 - st->fa[3] * st->yp1 - st->fa[4] * st->yp2;
        st->u1 = st->u;
        st->yp2 = st->yp1;
        st->yp1 = st->w_y[j];
    }
    
    lpc_calc_pitch(st->w_y, &per, &st->vuv);
    
    for (i = 0; i < BUFLEN; i++)
        st->w_w[i] = st->w_s[i] * st->w_h[i];
    lpc_auto_correl2(st->w_w, BUFLEN, r);
    lpc_durbin(r, LPC_FILTORDER, k, &G);

    per = per  * 2;
    if(per > 255.0f)
    {
        per = 255.0f;
    }
    out[0] = (unsigned char)(per);
    
    i = (int)(G * 256);
    if(i > 255) i = 255;	 /* reached when G = 10 */ 
    out[1] = (unsigned char)i;

    for (i = 0; i < LPC_FILTORDER; i++) {
        float u = k[i + 1];
        
        if (u < -0.9999f) {
            u = -0.9999f;
        } else if (u > 0.9999f) {
            u = 0.9999f;
        }
        out[i + 2] = (unsigned char)((signed char)(127.0f * u));
    }
    
    bcopy(st->w_s + LPC_SAMPLES_PER_FRAME, st->w_s, (BUFLEN - LPC_SAMPLES_PER_FRAME) * sizeof(st->w_s[0]));
    bcopy(st->w_y + LPC_SAMPLES_PER_FRAME, st->w_y, (BUFLEN - LPC_SAMPLES_PER_FRAME) * sizeof(st->w_y[0]));
    
    return 12;
}


void destroy_lpc_encoder_state(lpc_encoder_state *st)
{
    if(st != NULL)
    {
        free(st);
        st = NULL;
    }
}
