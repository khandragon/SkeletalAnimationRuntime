#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "math/Transform.h"

// Check if SSE is available and enabled
#if defined(ENGINE_ENABLE_SSE)
// Check for SSE support on x86/x64 platforms
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
// True, then include SSE intrinsics and enable SSE math
#include <immintrin.h>
#define ENGINE_MATH_HAS_SSE 1
// False, SSE not supported or enabled, fall back to scalar math
#else
#define ENGINE_MATH_HAS_SSE 0
#endif
#else
// SSE not enabled, fall back to scalar math
#define ENGINE_MATH_HAS_SSE 0
#endif

namespace Engine::Math
{
    namespace Detail
    {
        // Blends two vec3s using linear interpolation.
        // how much of b to blend into a based on weight.
        [[nodiscard]] inline glm::vec3 LerpVec3Scalar(
            const glm::vec3 &a,
            const glm::vec3 &b,
            float weight)
        {
            return a + (b - a) * weight;
        }

        // Blends two quaternions using normalized linear interpolation (Nlerp).
        [[nodiscard]] inline glm::quat NlerpQuatScalar(
            const glm::quat &a,
            const glm::quat &b,
            float weight)
        {
            // Double cover the long way to ensure we get the correct sign for the quaternion.
            const glm::quat correctedB =
                (glm::dot(a, b) < 0.0f) ? -b : b;

            // blended = a + (b - a) * weight then normalize the result so we get valid rotation.
            return glm::normalize(a + (correctedB - a) * weight);
        }

#if ENGINE_MATH_HAS_SSE

        [[nodiscard]] inline glm::quat NlerpQuatSse(
            const glm::quat &a,
            const glm::quat &b,
            float weight)
        {
            const glm::quat correctedB =
                (glm::dot(a, b) < 0.0f) ? -b : b;

            const __m128 av =
                _mm_set_ps(a.w, a.z, a.y, a.x);

            const __m128 bv =
                _mm_set_ps(
                    correctedB.w,
                    correctedB.z,
                    correctedB.y,
                    correctedB.x);

            const __m128 weightV =
                _mm_set1_ps(weight);

            // blended = a + (b - a) * weight
            const __m128 blended =
                _mm_add_ps(
                    av,
                    _mm_mul_ps(
                        _mm_sub_ps(bv, av),
                        weightV));

            // squared = [x*x, y*y, z*z, w*w]
            const __m128 squared =
                _mm_mul_ps(blended, blended);

            // First horizontal reduction step.
            // Low lanes become:
            // [x*x + z*z, y*y + w*w, ...]
            __m128 lengthSquared =
                _mm_add_ps(
                    squared,
                    _mm_movehl_ps(squared, squared));

            // Final horizontal reduction.
            // Lowest lane becomes:
            // x*x + y*y + z*z + w*w
            lengthSquared =
                _mm_add_ss(
                    lengthSquared,
                    _mm_shuffle_ps(lengthSquared, lengthSquared, 1));

            // Approximate reciprocal square root.
            __m128 inverseLength =
                _mm_rsqrt_ss(lengthSquared);

            // One Newton-Raphson refinement step.
            //
            // inverseLength =
            //     inverseLength * (1.5 - 0.5 * lengthSquared * inverseLength * inverseLength)
            //
            // This improves accuracy while still keeping the SSE path explicit.
            const __m128 half =
                _mm_set_ss(0.5f);

            const __m128 threeHalves =
                _mm_set_ss(1.5f);

            inverseLength =
                _mm_mul_ss(
                    inverseLength,
                    _mm_sub_ss(
                        threeHalves,
                        _mm_mul_ss(
                            _mm_mul_ss(half, lengthSquared),
                            _mm_mul_ss(inverseLength, inverseLength))));

            // Broadcast inverseLength to every lane and normalize.
            const __m128 normalized =
                _mm_mul_ps(
                    blended,
                    _mm_shuffle_ps(inverseLength, inverseLength, 0));

            alignas(16) float out[4];
            _mm_store_ps(out, normalized);

            return glm::quat(
                out[3], // w
                out[0], // x
                out[1], // y
                out[2]  // z
            );
        }

#endif
    }

    [[nodiscard]] inline glm::vec3 LerpVec3(
        const glm::vec3 &a,
        const glm::vec3 &b,
        float weight)
    {
        return Detail::LerpVec3Scalar(a, b, weight);
    }

    [[nodiscard]] inline glm::quat NlerpQuat(
        const glm::quat &a,
        const glm::quat &b,
        float weight)
    {
#if ENGINE_MATH_HAS_SSE
        return Detail::NlerpQuatSse(a, b, weight);
#else
        return Detail::NlerpQuatScalar(a, b, weight);
#endif
    }

    // Blends two transforms using linear interpolation for translation and scale, and normalized linear interpolation for rotation.
    [[nodiscard]] inline Transform BlendTransform(
        const Transform &a,
        const Transform &b,
        float weight)
    {
        Transform result{};

        result.translation = LerpVec3(a.translation, b.translation, weight);
        result.rotation = NlerpQuat(a.rotation, b.rotation, weight);
        result.scale = LerpVec3(a.scale, b.scale, weight);

        return result;
    }
}