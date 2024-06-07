#include "Keyframe.hpp"

#include <cfloat>
#include <math.h>
#include <simd/simd.h>

namespace lottie {

static float cubicRoot(float value) {
    return pow(value, 1.0 / 3.0);
}

static float SolveQuadratic(float a, float b, float c) {
    float result = (-b + sqrt((b * b) - 4 * a * c)) / (2 * a);
    if (isInRangeOrEqual(result, 0.0, 1.0)) {
        return result;
    }
    
    result = (-b - sqrt((b * b) - 4 * a * c)) / (2 * a);
    if (isInRangeOrEqual(result, 0.0, 1.0)) {
        return result;
    }
    
    return -1.0;
}

inline bool isApproximatelyEqual(float value, float other) {
    return std::abs(value - other) <= FLT_EPSILON;
}

static float SolveCubic(float a, float b, float c, float d) {
    if (isApproximatelyEqual(a, 0.0f)) {
        return SolveQuadratic(b, c, d);
    }
    if (isApproximatelyEqual(d, 0.0f)) {
        return 0.0;
    }
    b /= a;
    c /= a;
    d /= a;
    float q = (3.0 * c - (b * b)) / 9.0;
    float r = (-27.0 * d + b * (9.0 * c - 2.0 * (b * b))) / 54.0;
    float disc = (q * q * q) + (r * r);
    float term1 = b / 3.0;
    
    if (disc > 0.0) {
        float s = r + sqrt(disc);
        s = (s < 0) ? -cubicRoot(-s) : cubicRoot(s);
        float t = r - sqrt(disc);
        t = (t < 0) ? -cubicRoot(-t) : cubicRoot(t);
        
        float result = -term1 + s + t;
        if (isInRangeOrEqual(result, 0.0, 1.0)) {
            return result;
        }
    } else if (isApproximatelyEqual(disc, 0.0f)) {
        float r13 = (r < 0) ? -cubicRoot(-r) : cubicRoot(r);
        
        float result = -term1 + 2.0 * r13;
        if (isInRangeOrEqual(result, 0.0, 1.0)) {
            return result;
        }
        
        result = -(r13 + term1);
        if (isInRangeOrEqual(result, 0.0, 1.0)) {
            return result;
        }
    } else {
        q = -q;
        float dum1 = q * q * q;
        dum1 = acos(r / sqrt(dum1));
        float r13 = 2.0 * sqrt(q);
        
        float result = -term1 + r13 * cos(dum1 / 3.0);
        if (isInRangeOrEqual(result, 0.0, 1.0)) {
            return result;
        }
        result = -term1 + r13 * cos((dum1 + 2.0 * M_PI) / 3.0);
        if (isInRangeOrEqual(result, 0.0, 1.0)) {
            return result;
        }
        result = -term1 + r13 * cos((dum1 + 4.0 * M_PI) / 3.0);
        if (isInRangeOrEqual(result, 0.0, 1.0)) {
            return result;
        }
    }
    
    return -1.0;
}

float cubicBezierInterpolate(float value, Vector2D const &P1, Vector2D const &P2) {
    float t = 0.0;
    if (isApproximatelyEqual(value, 0.0f)) {
        // Handle corner cases explicitly to prevent rounding errors
        t = 0.0;
    } else if (isApproximatelyEqual(value, 1.0f)) {
        t = 1.0;
    } else {
        // Calculate t
        float a = 3 * P1.x - 3 * P2.x + 1.0f;
        float b = -6 * P1.x + 3 * P2.x;
        float c = 3 * P1.x;
        float d = -value;
        float tTemp = SolveCubic(a, b, c, d);
        
        if (isApproximatelyEqual(tTemp, -1.0f)) {
            return -1.0;
        }
        t = tTemp;
    }
    
    // Calculate y from t
    float oneMinusT = 1.0 - t;
    float result = 3 * t * (oneMinusT * oneMinusT) * P1.y + 3 * (t * t) * (1 - t) * P2.y + (t * t * t);
    
    return result;
}

}
