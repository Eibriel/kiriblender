/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BSDF_MICROFACET_H__
#define __BSDF_MICROFACET_H__

CCL_NAMESPACE_BEGIN

/* GGX */

__device int bsdf_microfacet_ggx_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* m_ag */
	
	sc->type = CLOSURE_BSDF_MICROFACET_GGX_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

__device int bsdf_microfacet_ggx_refraction_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* m_ag */

	sc->type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;

	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

__device void bsdf_microfacet_ggx_blur(ShaderClosure *sc, float roughness)
{
	sc->data0 = fmaxf(roughness, sc->data0); /* m_ag */
}

__device float3 bsdf_microfacet_ggx_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float m_ag = max(sc->data0, 1e-4f);
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
	float3 N = sc->N;

	if(m_refractive || m_ag <= 1e-4f)
		return make_float3 (0, 0, 0);
	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);
	if(cosNI > 0 && cosNO > 0) {
		// get half vector
		float3 Hr = normalize(omega_in + I);
		// eq. 20: (F*G*D)/(4*in*on)
		// eq. 33: first we calculate D(m) with m=Hr:
		float alpha2 = m_ag * m_ag;
		float cosThetaM = dot(N, Hr);
		float cosThetaM2 = cosThetaM * cosThetaM;
		float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
		float cosThetaM4 = cosThetaM2 * cosThetaM2;
		float D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
		// eq. 34: now calculate G1(i,m) and G1(o,m)
		float G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
		float G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 
		float G = G1o * G1i;
		float out = (G * D) * 0.25f / cosNO;
		// eq. 24
		float pm = D * cosThetaM;
		// convert into pdf of the sampled direction
		// eq. 38 - but see also:
		// eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
		*pdf = pm * 0.25f / dot(Hr, I);
		return make_float3 (out, out, out);
	}
	return make_float3 (0, 0, 0);
}

__device float3 bsdf_microfacet_ggx_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float m_ag = max(sc->data0, 1e-4f);
	float m_eta = sc->data1;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
	float3 N = sc->N;

	if(!m_refractive || m_ag <= 1e-4f)
		return make_float3 (0, 0, 0);
	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);
	if(cosNO <= 0 || cosNI >= 0)
		return make_float3 (0, 0, 0); // vectors on same side -- not possible
	// compute half-vector of the refraction (eq. 16)
	float3 ht = -(m_eta * omega_in + I);
	float3 Ht = normalize(ht);
	float cosHO = dot(Ht, I);

	float cosHI = dot(Ht, omega_in);
	// eq. 33: first we calculate D(m) with m=Ht:
	float alpha2 = m_ag * m_ag;
	float cosThetaM = dot(N, Ht);
	float cosThetaM2 = cosThetaM * cosThetaM;
	float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
	float cosThetaM4 = cosThetaM2 * cosThetaM2;
	float D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
	// eq. 34: now calculate G1(i,m) and G1(o,m)
	float G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
	float G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 
	float G = G1o * G1i;
	// probability
	float invHt2 = 1 / dot(ht, ht);
	*pdf = D * fabsf(cosThetaM) * (fabsf(cosHI) * (m_eta * m_eta)) * invHt2;
	float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D) * invHt2) / cosNO;
	return make_float3 (out, out, out);
}

__device int bsdf_microfacet_ggx_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float m_ag = sc->data0;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
	float3 N = sc->N;

	float cosNO = dot(N, I);
	if(cosNO > 0) {
		float3 X, Y, Z = N;
		make_orthonormals(Z, &X, &Y);
		// generate a random microfacet normal m
		// eq. 35,36:
		// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
		//tttt  and sin(atan(x)) == x/sqrt(1+x^2)
		float alpha2 = m_ag * m_ag;
		float tanThetaM2 = alpha2 * randu / (1 - randu);
		float cosThetaM  = 1 / safe_sqrtf(1 + tanThetaM2);
		float sinThetaM  = cosThetaM * safe_sqrtf(tanThetaM2);
		float phiM = M_2PI_F * randv;
		float3 m = (cosf(phiM) * sinThetaM) * X +
				 (sinf(phiM) * sinThetaM) * Y +
							   cosThetaM  * Z;
		if(!m_refractive) {
			float cosMO = dot(m, I);
			if(cosMO > 0) {
				// eq. 39 - compute actual reflected direction
				*omega_in = 2 * cosMO * m - I;
				if(dot(Ng, *omega_in) > 0) {
					if (m_ag <= 1e-4f) {
						// some high number for MIS
						*pdf = 1e6f;
						*eval = make_float3(1e6f, 1e6f, 1e6f);
					}
					else {
						// microfacet normal is visible to this ray
						// eq. 33
						float cosThetaM2 = cosThetaM * cosThetaM;
						float cosThetaM4 = cosThetaM2 * cosThetaM2;
						float D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
						// eq. 24
						float pm = D * cosThetaM;
						// convert into pdf of the sampled direction
						// eq. 38 - but see also:
						// eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
						*pdf = pm * 0.25f / cosMO;
						// eval BRDF*cosNI
						float cosNI = dot(N, *omega_in);
						// eq. 34: now calculate G1(i,m) and G1(o,m)
						float G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
						float G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 
						float G = G1o * G1i;
						// eq. 20: (F*G*D)/(4*in*on)
						float out = (G * D) * 0.25f / cosNO;
						*eval = make_float3(out, out, out);
					}

#ifdef __RAY_DIFFERENTIALS__
					*domega_in_dx = (2 * dot(m, dIdx)) * m - dIdx;
					*domega_in_dy = (2 * dot(m, dIdy)) * m - dIdy;
#endif
				}
			}
		}
		else {
			// CAUTION: the i and o variables are inverted relative to the paper
			// eq. 39 - compute actual refractive direction
			float3 R, T;
#ifdef __RAY_DIFFERENTIALS__
			float3 dRdx, dRdy, dTdx, dTdy;
#endif
			float m_eta = sc->data1;
			bool inside;
			fresnel_dielectric(m_eta, m, I, &R, &T,
#ifdef __RAY_DIFFERENTIALS__
				dIdx, dIdy, &dRdx, &dRdy, &dTdx, &dTdy,
#endif
				&inside);
			
			if(!inside) {
				*omega_in = T;
#ifdef __RAY_DIFFERENTIALS__
				*domega_in_dx = dTdx;
				*domega_in_dy = dTdy;
#endif

				if (m_ag <= 1e-4f) {
					// some high number for MIS
					*pdf = 1e6f;
					*eval = make_float3(1e6f, 1e6f, 1e6f);
				}
				else {
					// eq. 33
					float cosThetaM2 = cosThetaM * cosThetaM;
					float cosThetaM4 = cosThetaM2 * cosThetaM2;
					float D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
					// eq. 24
					float pm = D * cosThetaM;
					// eval BRDF*cosNI
					float cosNI = dot(N, *omega_in);
					// eq. 34: now calculate G1(i,m) and G1(o,m)
					float G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
					float G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI))); 
					float G = G1o * G1i;
					// eq. 21
					float cosHI = dot(m, *omega_in);
					float cosHO = dot(m, I);
					float Ht2 = m_eta * cosHI + cosHO;
					Ht2 *= Ht2;
					float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D)) / (cosNO * Ht2);
					// eq. 38 and eq. 17
					*pdf = pm * (m_eta * m_eta) * fabsf(cosHI) / Ht2;
					*eval = make_float3(out, out, out);
				}
			}
		}
	}
	return (m_refractive) ? LABEL_TRANSMIT|LABEL_GLOSSY : LABEL_REFLECT|LABEL_GLOSSY;
}

/* BECKMANN */

__device int bsdf_microfacet_beckmann_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* m_ab */

	sc->type = CLOSURE_BSDF_MICROFACET_BECKMANN_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

__device int bsdf_microfacet_beckmann_refraction_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 0.0f, 1.0f); /* m_ab */

	sc->type = CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

__device void bsdf_microfacet_beckmann_blur(ShaderClosure *sc, float roughness)
{
	sc->data0 = fmaxf(roughness, sc->data0); /* m_ab */
}

__device float3 bsdf_microfacet_beckmann_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float m_ab = max(sc->data0, 1e-4f);
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	float3 N = sc->N;

	if(m_refractive || m_ab <= 1e-4f)
		return make_float3 (0, 0, 0);
	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);
	if(cosNO > 0 && cosNI > 0) {
	   // get half vector
	   float3 Hr = normalize(omega_in + I);
	   // eq. 20: (F*G*D)/(4*in*on)
	   // eq. 25: first we calculate D(m) with m=Hr:
	   float alpha2 = m_ab * m_ab;
	   float cosThetaM = dot(N, Hr);
	   float cosThetaM2 = cosThetaM * cosThetaM;
	   float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
	   float cosThetaM4 = cosThetaM2 * cosThetaM2;
	   float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 *  cosThetaM4);
	   // eq. 26, 27: now calculate G1(i,m) and G1(o,m)
	   float ao = 1 / (m_ab * safe_sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
	   float ai = 1 / (m_ab * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
	   float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
	   float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
	   float G = G1o * G1i;
	   float out = (G * D) * 0.25f / cosNO;
	   // eq. 24
	   float pm = D * cosThetaM;
	   // convert into pdf of the sampled direction
	   // eq. 38 - but see also:
	   // eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
	   *pdf = pm * 0.25f / dot(Hr, I);
	   return make_float3 (out, out, out);
	}
	return make_float3 (0, 0, 0);
}

__device float3 bsdf_microfacet_beckmann_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float m_ab = max(sc->data0, 1e-4f);
	float m_eta = sc->data1;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	float3 N = sc->N;

	if(!m_refractive || m_ab <= 1e-4f)
		return make_float3 (0, 0, 0);
	float cosNO = dot(N, I);
	float cosNI = dot(N, omega_in);
	if(cosNO <= 0 || cosNI >= 0)
		return make_float3 (0, 0, 0);
	// compute half-vector of the refraction (eq. 16)
	float3 ht = -(m_eta * omega_in + I);
	float3 Ht = normalize(ht);
	float cosHO = dot(Ht, I);

	float cosHI = dot(Ht, omega_in);
	// eq. 33: first we calculate D(m) with m=Ht:
	float alpha2 = m_ab * m_ab;
	float cosThetaM = min(dot(N, Ht), 1.0f);
	float cosThetaM2 = cosThetaM * cosThetaM;
	float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
	float cosThetaM4 = cosThetaM2 * cosThetaM2;
	float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 *  cosThetaM4);
	// eq. 26, 27: now calculate G1(i,m) and G1(o,m)
	float ao = 1 / (m_ab * safe_sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
	float ai = 1 / (m_ab * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
	float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
	float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
	float G = G1o * G1i;
	// probability
	float invHt2 = 1 / dot(ht, ht);
	*pdf = D * fabsf(cosThetaM) * (fabsf(cosHI) * (m_eta * m_eta)) * invHt2;
	float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D) * invHt2) / cosNO;
	return make_float3 (out, out, out);
}

__device int bsdf_microfacet_beckmann_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float m_ab = sc->data0;
	int m_refractive = sc->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
	float3 N = sc->N;

	float cosNO = dot(N, I);
	if(cosNO > 0) {
		float3 X, Y, Z = N;
		make_orthonormals(Z, &X, &Y);
		// generate a random microfacet normal m
		// eq. 35,36:
		// we take advantage of cos(atan(x)) == 1/sqrt(1+x^2)
		//tttt  and sin(atan(x)) == x/sqrt(1+x^2)
		float alpha2 = m_ab * m_ab;
		float tanThetaM, cosThetaM;

		if(alpha2 == 0.0f) {
			tanThetaM = 0.0f;
			cosThetaM = 1.0f;
		}
		else {
			tanThetaM = safe_sqrtf(-alpha2 * logf(1 - randu));
			cosThetaM = 1 / safe_sqrtf(1 + tanThetaM * tanThetaM);
		}

		float sinThetaM = cosThetaM * tanThetaM;
		float phiM = M_2PI_F * randv;
		float3 m = (cosf(phiM) * sinThetaM) * X +
				 (sinf(phiM) * sinThetaM) * Y +
							   cosThetaM  * Z;

		if(!m_refractive) {
			float cosMO = dot(m, I);
			if(cosMO > 0) {
				// eq. 39 - compute actual reflected direction
				*omega_in = 2 * cosMO * m - I;
				if(dot(Ng, *omega_in) > 0) {
					if (m_ab <= 1e-4f) {
						// some high number for MIS
						*pdf = 1e6f;
						*eval = make_float3(1e6f, 1e6f, 1e6f);
					}
					else {
						// microfacet normal is visible to this ray
						// eq. 25
						float cosThetaM2 = cosThetaM * cosThetaM;
						float tanThetaM2 = tanThetaM * tanThetaM;
						float cosThetaM4 = cosThetaM2 * cosThetaM2;
						float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 *  cosThetaM4);
						// eq. 24
						float pm = D * cosThetaM;
						// convert into pdf of the sampled direction
						// eq. 38 - but see also:
						// eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
						*pdf = pm * 0.25f / cosMO;
						// Eval BRDF*cosNI
						float cosNI = dot(N, *omega_in);
						// eq. 26, 27: now calculate G1(i,m) and G1(o,m)
						float ao = 1 / (m_ab * safe_sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
						float ai = 1 / (m_ab * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
						float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
						float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
						float G = G1o * G1i;
						// eq. 20: (F*G*D)/(4*in*on)
						float out = (G * D) * 0.25f / cosNO;
						*eval = make_float3(out, out, out);
					}
#ifdef __RAY_DIFFERENTIALS__
					*domega_in_dx = (2 * dot(m, dIdx)) * m - dIdx;
					*domega_in_dy = (2 * dot(m, dIdy)) * m - dIdy;
#endif
				}
			}
		}
		else {
			// CAUTION: the i and o variables are inverted relative to the paper
			// eq. 39 - compute actual refractive direction
			float3 R, T;
#ifdef __RAY_DIFFERENTIALS__
			float3 dRdx, dRdy, dTdx, dTdy;
#endif
			float m_eta = sc->data1;
			bool inside;
			fresnel_dielectric(m_eta, m, I, &R, &T,
#ifdef __RAY_DIFFERENTIALS__
				dIdx, dIdy, &dRdx, &dRdy, &dTdx, &dTdy,
#endif
				&inside);

			if(!inside) {
				*omega_in = T;
#ifdef __RAY_DIFFERENTIALS__
				*domega_in_dx = dTdx;
				*domega_in_dy = dTdy;
#endif
				if (m_ab <= 1e-4f) {
					// some high number for MIS
					*pdf = 1e6f;
					*eval = make_float3(1e6f, 1e6f, 1e6f);
				}
				else {
					// eq. 33
					float cosThetaM2 = cosThetaM * cosThetaM;
					float tanThetaM2 = tanThetaM * tanThetaM;
					float cosThetaM4 = cosThetaM2 * cosThetaM2;
					float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 *  cosThetaM4);
					// eq. 24
					float pm = D * cosThetaM;
					// eval BRDF*cosNI
					float cosNI = dot(N, *omega_in);
					// eq. 26, 27: now calculate G1(i,m) and G1(o,m)
					float ao = 1 / (m_ab * safe_sqrtf((1 - cosNO * cosNO) / (cosNO * cosNO)));
					float ai = 1 / (m_ab * safe_sqrtf((1 - cosNI * cosNI) / (cosNI * cosNI)));
					float G1o = ao < 1.6f ? (3.535f * ao + 2.181f * ao * ao) / (1 + 2.276f * ao + 2.577f * ao * ao) : 1.0f;
					float G1i = ai < 1.6f ? (3.535f * ai + 2.181f * ai * ai) / (1 + 2.276f * ai + 2.577f * ai * ai) : 1.0f;
					float G = G1o * G1i;
					// eq. 21
					float cosHI = dot(m, *omega_in);
					float cosHO = dot(m, I);
					float Ht2 = m_eta * cosHI + cosHO;
					Ht2 *= Ht2;
					float out = (fabsf(cosHI * cosHO) * (m_eta * m_eta) * (G * D)) / (cosNO * Ht2);
					// eq. 38 and eq. 17
					*pdf = pm * (m_eta * m_eta) * fabsf(cosHI) / Ht2;
					*eval = make_float3(out, out, out);
				}
			}
		}
	}
	return (m_refractive) ? LABEL_TRANSMIT|LABEL_GLOSSY : LABEL_REFLECT|LABEL_GLOSSY;
}

CCL_NAMESPACE_END

#endif /* __BSDF_MICROFACET_H__ */

