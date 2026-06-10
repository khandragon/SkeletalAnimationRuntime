#pragma once

#include <immintrin.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "math/Transform.h"

inline glm::vec3 LerpVec3Simd(
    const glm::vec3 &a,
    const glm::vec3 &b,
    float weight)
{
    const __m128 av =
        _mm_set_ps(0.0f, a.z, a.y, a.x);

    const __m128 bv =
        _mm_set_ps(0.0f, b.z, b.y, b.x);

    const __m128 wv =
        _mm_set1_ps(weight);

    const __m128 result =
        _mm_add_ps(
            av,
            _mm_mul_ps(
                _mm_sub_ps(bv, av),
                wv));

    alignas(16) float out[4];
    _mm_store_ps(out, result);

    return glm::vec3(out[0], out[1], out[2]);
}

inline glm::quat NlerpQuatSimd(
    const glm::quat &a,
    const glm::quat &b,
    float weight)
{
    glm::quat correctedB = b;

    if (glm::dot(a, b) < 0.0f)
    {
        correctedB = -b;
    }

    const __m128 av =
        _mm_set_ps(a.w, a.z, a.y, a.x);

    const __m128 bv =
        _mm_set_ps(
            correctedB.w,
            correctedB.z,
            correctedB.y,
            correctedB.x);

    const __m128 wv =
        _mm_set1_ps(weight);

    const __m128 blended =
        _mm_add_ps(
            av,
            _mm_mul_ps(
                _mm_sub_ps(bv, av),
                wv));

    const __m128 squared =
        _mm_mul_ps(blended, blended);

    __m128 sum =
        _mm_add_ps(
            squared,
            _mm_movehl_ps(squared, squared));

    sum =
        _mm_add_ss(
            sum,
            _mm_shuffle_ps(sum, sum, 1));

    const __m128 invLength =
        _mm_rsqrt_ss(sum);

    const __m128 normalized =
        _mm_mul_ps(
            blended,
            _mm_shuffle_ps(invLength, invLength, 0));

    alignas(16) float out[4];
    _mm_store_ps(out, normalized);

    return glm::quat(
        out[3],
        out[0],
        out[1],
        out[2]);
}

inline Transform BlendTransformSimd(
    const Transform &a,
    const Transform &b,
    float weight)
{
    Transform result{};

    result.translation =
        LerpVec3Simd(a.translation, b.translation, weight);

    result.rotation =
        NlerpQuatSimd(a.rotation, b.rotation, weight);

    result.scale =
        LerpVec3Simd(a.scale, b.scale, weight);

    return result;
}