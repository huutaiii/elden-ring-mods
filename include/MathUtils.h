#pragma once

#include <glm/glm.hpp>

constexpr float PI = 3.14159265359f;

inline __m128 GLMtoXMM(glm::vec4 v)
{
    return _mm_setr_ps(v.x, v.y, v.z, 0.f);
}

inline __m128 GLMtoXMM(glm::vec3 v)
{
    return GLMtoXMM(glm::vec4(v, static_cast<decltype(v.r)>(0)));
}

inline glm::vec4 XMMtoGLM(__m128 m)
{
    float v[4];
    _mm_storer_ps(v, m); // MSVC specific, keeps components in reverse order
    return glm::vec4(v[3], v[2], v[1], v[0]);
}

inline glm::vec4 MemToGLM(void* p)
{
    float* v = (float*)p;
    return glm::vec4(v[0], v[1], v[2], v[3]);
}

inline float RelativeOffsetAlpha(glm::vec3 offset, float max_distance)
{
    return glm::length(offset) / max_distance;
}

template<typename T>
inline T min(T a, T b) { return a < b ? a : b; }

template<typename T>
inline T max(T a, T b) { return a > b ? a : b; }

template<typename T>
inline T clamp(T x, T a, T b) { return min(b, max(a, x)); }

template<typename T>
inline T saturate(T x) { return clamp(x, T(0), T(1)); }

template<typename Tv, typename Ta>
inline Tv lerp(Tv x, Tv y, Ta a) { a = clamp(a, Ta(0), Ta(1)); return x * ((Ta)1.0 - a) + y * a; }

template<typename T>
inline T smoothstep(T edge0, T edge1, T x) {
    x = clamp((x - edge0) / (edge1 - edge0), (T)0, (T)1);
    return x * x * (3 - 2 * x);
}

template<typename T>
inline T maprange(T x, T in_min, T in_max, T out_min, T out_max) { return out_min + (x - in_min) / (in_max - in_min) * (out_max - out_min); }

template<typename T>
inline T mapclamped(T x, T in_min, T in_max, T out_min, T out_max)
{
    return out_min < out_max ? clamp(maprange(x, in_min, in_max, out_min, out_max)) : clamp(maprange(x, in_min, in_max, out_max, out_min));
}

template<typename T>
inline T oneminus(T x) { return T(1) - x; }

template<typename T>
inline T rcp(T x) { return T(1) / x; }

template<typename T, typename Tresult>
inline Tresult sign(T x) { return Tresult(x >= T(0) ? 1 : -1); }

template<typename T, typename Tresult>
inline Tresult signz(T x) { return x == 0 ? 0 : sign<T, Tresult>(x); }

template<typename T>
inline T safediv(T x, T y) { return x == 0 ? 0 : x / y; }

template<typename T> T EaseInOutSine(T x) {
    return T(-(cos(PI * x) - 1) / 2);
}

template<glm::length_t L, typename T, glm::qualifier Q>
inline glm::vec<L, T, Q> InterpToV(glm::vec<L, T, Q> current, glm::vec<L, T, Q> target, float speed, float deltaTime, float minDistance = 0.0001f)
{
    if (speed <= 0.f)
    {
        return target;
    }

    glm::vec<L, T, Q> delta = (target - current);
    if (glm::length(delta) <= minDistance)
    {
        return target;
    }

    glm::vec<L, T, Q> vel = delta * clamp(deltaTime * speed, 0.f, 1.f);
    return current + vel;
}

template<glm::length_t L, typename T, glm::qualifier Q>
inline glm::vec<L, T, Q> InterpSToV(glm::vec<L, T, Q> current, glm::vec<L, T, Q> target, float speed, float deltaTime, float minDistance = 0.0001f)
{
    if (speed <= 0.f)
    {
        return target;
    }

    glm::vec<L, T, Q> delta = (target - current);
    if (glm::length(delta) <= minDistance)
    {
        return target;
    }

    T deltaL = glm::length(delta);
    glm::vec<L, T, Q> vel = glm::normalize(delta) * clamp(deltaL * deltaL * deltaTime * speed, 0.f, deltaL);
    return current + vel;
}

template <typename T>
inline T InterpToF(T current, T target, double speed, double deltaTime, double minDistance = 1.0 / 10000)
{
    if (speed <= 0.00001)
    {
        return target;
    }

    T delta = target - current;
    if (abs(delta) <= minDistance)
    {
        return target;
    }

    T deltaInterp = T(delta * saturate(deltaTime * speed));
    return current + deltaInterp;
}

template <typename T>
inline T InterpSToF(T current, T target, double speed, double deltaTime, double minDistance = 1.0 / 10000)
{
    if (speed <= 0.00001)
    {
        return target;
    }

    T delta = target - current;
    if (abs(delta) <= minDistance)
    {
        return target;
    }

    T deltaInterp = T(delta * delta * saturate(deltaTime * speed));
    deltaInterp = sign(delta) * min(deltaInterp, abs(delta));
    return current + deltaInterp;
}

template <typename T>
inline T InterpToFConstant(T current, T target, double speed, double deltaTime, double minDistance = 1.0 / 10000)
{
    if (speed <= 0.00001)
    {
        return target;
    }

    T delta = target - current;
    if (abs(delta) <= minDistance)
    {
        return target;
    }

    if (delta >= 0.f)
    {
        return clamp(T(current + deltaTime * speed), current, target);
    }
    else
    {
        return clamp(T(current - deltaTime * speed), target, current);
    }
}

template <glm::length_t L, typename T, glm::qualifier Q>
inline glm::vec<L, T, Q> ClampVecLength(glm::vec<L, T, Q> vec, T maxlength)
{
    T length = glm::length(vec);
    if (length > 0.0001f)
    {
        return glm::normalize(vec) * min(length, maxlength);
    }
    return glm::vec<L, T, Q>({});
}