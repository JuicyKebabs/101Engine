#include "Math.h"
#include <algorithm>

using namespace DirectX;

//===============================
// Vector2 Implementation
//===============================
float Vector2::Length() const
{
    return sqrtf(x * x + y * y);
}

float Vector2::LengthSq() const
{
    return x * x + y * y;
}

float Vector2::DistanceTo(const Vector2& other) const
{
    return sqrtf(DistanceSqTo(other));
}

float Vector2::DistanceSqTo(const Vector2& other) const
{
    float dx = other.x - x;
    float dy = other.y - y;
    return dx * dx + dy * dy;
}

Vector2 Vector2::Lerp(const Vector2& target, float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);
    return Vector2(
        x + (target.x - x) * t,
        y + (target.y - y) * t
    );
}

Vector2 Vector2::Min(const Vector2& other) const
{
    return Vector2(
        std::min(x, other.x),
        std::min(y, other.y)
    );
}

Vector2 Vector2::Max(const Vector2& other) const
{
    return Vector2(
        std::max(x, other.x),
        std::max(y, other.y)
    );
}

Vector2 Vector2::Clamp(const Vector2& min, const Vector2& max) const
{
    return Vector2(
        std::clamp(x, min.x, max.x),
        std::clamp(y, min.y, max.y)
    );
}

Vector2 Vector2::Abs() const
{
    return Vector2(
        std::fabs(x),
        std::fabs(y)
    );
}

Vector2 Vector2::Normalized() const
{
    float length = sqrtf(x * x + y * y);
    if (length > 0.0f)
    {
        return Vector2(x / length, y / length);
    }
    else
    {
        return Vector2(0.0f, 0.0f);
    }
}

float Vector2::Dot(const Vector2& other) const
{
    return x * other.x + y * other.y;
}

bool Vector2::NearEqual(const Vector2& other, float epsilon) const
{
	return (std::fabs(x - other.x) <= epsilon) && (std::fabs(y - other.y) <= epsilon);
}

float Vector2::Distance(const Vector2& a, const Vector2& b)
{
	return a.DistanceTo(b);
}

float Vector2::DistanceSq(const Vector2& a, const Vector2& b)
{
    return a.DistanceSqTo(b);
}

Vector2 Vector2::Lerp(const Vector2& a, const Vector2& b, float t)
{
    return a.Lerp(b, t);
}

Vector2 Vector2::Min(const Vector2& a, const Vector2& b)
{
    return a.Min(b);
}

Vector2 Vector2::Max(const Vector2& a, const Vector2& b)
{
    return a.Max(b);
}

Vector2 Vector2::Clamp(const Vector2& value, const Vector2& min, const Vector2& max)
{
    return value.Clamp(min, max);
}

Vector2& Vector2::operator=(const Vector2& rhs)
{
    if (this != &rhs)
    {
        x = rhs.x;
        y = rhs.y;
    }
    return *this;
}

Vector2 Vector2::operator+() const
{
    return *this;
}

Vector2 Vector2::operator-() const
{
    return Vector2(-x, -y);
}

Vector2 Vector2::operator+(const Vector2& rhs) const
{
	return Vector2(x + rhs.x, y + rhs.y);
}

Vector2 Vector2::operator-(const Vector2& rhs) const
{
    return Vector2(x - rhs.x, y - rhs.y);
}

Vector2 Vector2::operator*(float scalar) const
{
    return Vector2(x * scalar, y * scalar);
}

Vector2 Vector2::operator/(float scalar) const
{
    if (scalar != 0.0f)
    {
		return Vector2(x / scalar, y / scalar);
    }
    else
    {
#ifdef _DEBUG
		// Assert to catch division by zero during development
		assert(scalar != 0.0f && "Division by zero in Vector2::operator/.");
#endif // _DEBUG
		return Vector2::Zero();
    }
}

Vector2& Vector2::operator+=(const Vector2& rhs)
{
    x += rhs.x;
    y += rhs.y;
    return *this;
}

Vector2& Vector2::operator-=(const Vector2& rhs)
{
    x -= rhs.x;
    y -= rhs.y;
    return *this;
}

Vector2& Vector2::operator*=(float scalar)
{
    x *= scalar;
    y *= scalar;
    return *this;
}

Vector2& Vector2::operator/=(float scalar)
{
    if (scalar != 0.0f)
    {
        x /= scalar;
        y /= scalar;
    }
    else
    {
#ifdef _DEBUG
		// Assert to catch division by zero during development
		assert(scalar != 0.0f && "Division by zero in Vector2::operator/=.");
#endif // _DEBUG
		x = 0.0f;
		y = 0.0f;
    }
    return *this;
}

//===============================
// Vector3 Implementation
//===============================
float Vector3::Length() const
{
    return sqrtf(x * x + y * y + z * z);
}

float Vector3::LengthSq() const
{
    return x * x + y * y + z * z;
}

float Vector3::DistanceTo(const Vector3& other) const
{
	return sqrtf(DistanceSqTo(other));
}

float Vector3::DistanceSqTo(const Vector3& other) const
{
    float dx = other.x - x;
    float dy = other.y - y;
    float dz = other.z - z;
    return dx * dx + dy * dy + dz * dz;
}

Vector3 Vector3::Lerp(const Vector3& target, float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);
    return Vector3(
        x + (target.x - x) * t,
        y + (target.y - y) * t,
        z + (target.z - z) * t
    );
}

Vector3 Vector3::Min(const Vector3& other) const
{
    return Vector3(
        std::min(x, other.x),
        std::min(y, other.y),
        std::min(z, other.z)
	);
}

Vector3 Vector3::Max(const Vector3& other) const
{
    return Vector3(
        std::max(x, other.x),
        std::max(y, other.y),
        std::max(z, other.z)
    );
}

Vector3 Vector3::Clamp(const Vector3& min, const Vector3& max) const
{
    return Vector3(
        std::clamp(x, min.x, max.x),
        std::clamp(y, min.y, max.y),
        std::clamp(z, min.z, max.z)
	);
}

Vector3 Vector3::Abs() const
{
    return Vector3(
        std::fabs(x),
        std::fabs(y),
        std::fabs(z)
    );
}

Vector3 Vector3::Normalized() const
{
    float length = sqrtf(x * x + y * y + z * z);
    if (length > 0.0f)
    {
        return Vector3(x / length, y / length, z / length);
    }
    else
    {
        return Vector3(0.0f, 0.0f, 0.0f);
    }
}

Vector3 Vector3::Cross(const Vector3& other) const
{
    return Vector3{
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    };
}

float Vector3::Dot(const Vector3& other) const
{
    return x * other.x + y * other.y + z * other.z;
}

Vector3 Vector3::Reflect(const Vector3& normal) const
{
    return *this - normal * (2.0f * Dot(normal));
}

Vector3 Vector3::Refract(const Vector3& normal, float eta) const
{
    float cosi = std::clamp(Dot(normal), -1.0f, 1.0f);
    float etai = 1.0f, etat = eta;
    Vector3 n = normal;
    if (cosi < 0)
    {
        cosi = -cosi;
    }
    else
    {
        std::swap(etai, etat);
        n = -normal;
    }
    float etaRatio = etai / etat;
    float k = 1.0f - etaRatio * etaRatio * (1.0f - cosi * cosi);
    if (k < 0.0f)
    {
        return Vector3(0.0f, 0.0f, 0.0f); // Total internal reflection
    }
    else
    {
        return *this * etaRatio + n * (etaRatio * cosi - sqrtf(k));
	}
}

bool Vector3::NearEqual(const Vector3& other, float epsilon) const
{
    return std::fabs(x - other.x) <= epsilon && std::fabs(y - other.y) <= epsilon && std::fabs(z - other.z) <= epsilon;
}

Vector3 Vector3::Project(const Vector3& onto) const
{
    float dotProduct = Dot(onto);
    float ontoLengthSq = onto.LengthSq();
    if (ontoLengthSq > 0.0f)
    {
        return onto * (dotProduct / ontoLengthSq);
    }
    else
    {
        return Vector3(0.0f, 0.0f, 0.0f);
    }
}

Vector3 Vector3::Reject(const Vector3& onto) const
{
    return *this - Project(onto);
}

float Vector3::Angle(const Vector3& other) const
{
    float dotProduct = Dot(other);
    float lengthsProduct = Length() * other.Length();
    if (lengthsProduct > 0.0f)
    {
        float cosine = std::clamp(dotProduct / lengthsProduct, -1.0f, 1.0f);
		return acosf(cosine);
    }
    else
    {
		return 0.0f; // Undefined angle if either vector is zero-length
    }
}

float Vector3::Distance(const Vector3& a, const Vector3& b)
{
	return a.DistanceTo(b);
}

float Vector3::DistanceSq(const Vector3& a, const Vector3& b)
{
    return a.DistanceSqTo(b);
}

Vector3 Vector3::Lerp(const Vector3& a, const Vector3& b, float t)
{
    return a.Lerp(b, t);
}

Vector3 Vector3::Min(const Vector3& a, const Vector3& b)
{
    return a.Min(b);
}

Vector3 Vector3::Max(const Vector3& a, const Vector3& b)
{
    return a.Max(b);
}

Vector3 Vector3::Clamp(const Vector3& value, const Vector3& min, const Vector3& max)
{
    return value.Clamp(min, max);
}

Vector3 Vector3::Transform(const Vector3& v, const Matrix4x4& m)
{
	float x = v.x * m._11 + v.y * m._21 + v.z * m._31 + m._41;
	float y = v.x * m._12 + v.y * m._22 + v.z * m._32 + m._42;
	float z = v.x * m._13 + v.y * m._23 + v.z * m._33 + m._43;
	float w = v.x * m._14 + v.y * m._24 + v.z * m._34 + m._44;

	if (w != 0.0f)
	{
		return Vector3(x / w, y / w, z / w);
	}
	else
	{
		return Vector3(x, y, z); // If w is zero, return the unnormalized result
	}
}

Vector3& Vector3::operator=(const Vector3& rhs)
{
    if (this != &rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
    }
    return *this;
}

Vector3 Vector3::operator+() const
{
    return *this;
}

Vector3 Vector3::operator-() const
{
    return Vector3(-x, -y, -z);
}

Vector3 Vector3::operator+(const Vector3& rhs) const
{
	return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
}

Vector3 Vector3::operator-(const Vector3& rhs) const
{
    return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
}

Vector3 Vector3::operator*(float scalar) const
{
    return Vector3(x * scalar, y * scalar, z * scalar);
}

Vector3 Vector3::operator/(float scalar) const
{
    if (scalar != 0.0f)
    {
        return Vector3(x / scalar, y / scalar, z / scalar);
    }
    else
    {
#ifdef _DEBUG
		// Assert to catch division by zero during development
		assert(scalar != 0.0f && "Division by zero in Vector3::operator/.");
#endif // _DEBUG
		return Vector3::Zero();
	}
}

Vector3 & Vector3::operator+=(const Vector3& rhs)
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}

Vector3 & Vector3::operator-=(const Vector3& rhs)
{
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
}

Vector3 & Vector3::operator*=(float scalar)
{
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

Vector3 & Vector3::operator/=(float scalar)
{
    if (scalar != 0.0f)
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
    }
    else
    {
#ifdef _DEBUG
		// Assert to catch division by zero during development
		assert(scalar != 0.0f && "Division by zero in Vector3::operator/=.");
#endif // _DEBUG
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
    }
    return *this;
}

Vector3::operator DirectX::XMVECTOR() const
{
    return XMLoadFloat3(this);
}

//===============================
// Vector4 Implementation
//===============================
float Vector4::Length() const
{
    return sqrtf(x * x + y * y + z * z + w * w);
}

float Vector4::LengthSq() const
{
    return x * x + y * y + z * z + w * w;
}

float Vector4::DistanceTo(const Vector4& other) const
{
    return sqrtf(DistanceSqTo(other));
}

float Vector4::DistanceSqTo(const Vector4& other) const
{
    float dx = other.x - x;
    float dy = other.y - y;
    float dz = other.z - z;
    float dw = other.w - w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}

Vector4 Vector4::Lerp(const Vector4& target, float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);
    return Vector4(
        x + (target.x - x) * t,
        y + (target.y - y) * t,
        z + (target.z - z) * t,
        w + (target.w - w) * t
    );
}

Vector4 Vector4::Min(const Vector4& other) const
{
    return Vector4(
        std::min(x, other.x),
        std::min(y, other.y),
        std::min(z, other.z),
        std::min(w, other.w)
    );
}

Vector4 Vector4::Max(const Vector4& other) const
{
    return Vector4(
        std::max(x, other.x),
        std::max(y, other.y),
        std::max(z, other.z),
        std::max(w, other.w)
    );
}

Vector4 Vector4::Clamp(const Vector4& min, const Vector4& max) const
{
    return Vector4(
        std::clamp(x, min.x, max.x),
        std::clamp(y, min.y, max.y),
        std::clamp(z, min.z, max.z),
        std::clamp(w, min.w, max.w)
    );
}

Vector4 Vector4::Abs() const
{
    return Vector4(
        std::fabs(x),
        std::fabs(y),
        std::fabs(z),
        std::fabs(w)
    );
}

Vector4 Vector4::Normalized() const
{
    float length = sqrtf(x * x + y * y + z * z + w * w);
    if (length > 0.0f)
    {
        return Vector4(x / length, y / length, z / length, w / length);
    }
    else
    {
        return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

float Vector4::Dot(const Vector4& other) const
{
    return x * other.x + y * other.y + z * other.z + w * other.w;
}

bool Vector4::NearEqual(const Vector4& other, float epsilon) const
{
    return (std::fabs(x - other.x) <= epsilon) && (std::fabs(y - other.y) <= epsilon) && (std::fabs(z - other.z) <= epsilon) && (std::fabs(w - other.w) <= epsilon);
}

float Vector4::Distance(const Vector4& a, const Vector4& b)
{
	return a.DistanceTo(b);
}

float Vector4::DistanceSq(const Vector4& a, const Vector4& b)
{
    return a.DistanceSqTo(b);
}

Vector4 Vector4::Lerp(const Vector4& a, const Vector4& b, float t)
{
    return a.Lerp(b, t);
}

Vector4 Vector4::Min(const Vector4& a, const Vector4& b)
{
    return a.Min(b);
}

Vector4 Vector4::Max(const Vector4& a, const Vector4& b)
{
    return a.Max(b);
}

Vector4 Vector4::Clamp(const Vector4& value, const Vector4& min, const Vector4& max)
{
    return value.Clamp(min, max);
}

Vector4& Vector4::operator=(const Vector4& rhs)
{
    if (this != &rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        w = rhs.w;
    }
    return *this;
}

Vector4 Vector4::operator+() const
{
    return *this;
}

Vector4 Vector4::operator-() const
{
    return Vector4(-x, -y, -z, -w);
}

Vector4 Vector4::operator+(const Vector4& rhs) const
{
	return Vector4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
}

Vector4 Vector4::operator-(const Vector4& rhs) const
{
    return Vector4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
}

Vector4 Vector4::operator*(float scalar) const
{
    return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
}

Vector4 Vector4::operator*(const Vector4& rhs) const
{
	return Vector4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
}

Vector4 Vector4::operator/(float scalar) const
{
    if (scalar != 0.0f)
    {
        return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
    }
    else
    {
#ifdef _DEBUG
		// Assert to catch division by zero during development
		assert(scalar != 0.0f && "Division by zero in Vector4::operator/.");
#endif // _DEBUG
		return Vector4::Zero();
    }
}

Vector4 & Vector4::operator+=(const Vector4& rhs)
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    w += rhs.w;
    return *this;
}

Vector4 & Vector4::operator-=(const Vector4& rhs)
{
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    w -= rhs.w;
    return *this;
}

Vector4 & Vector4::operator*=(float scalar)
{
    x *= scalar;
    y *= scalar;
    z *= scalar;
    w *= scalar;
    return *this;
}

Vector4& Vector4::operator/=(float scalar)
{
    if (scalar != 0.0f)
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
    }
    else
    {
#ifdef _DEBUG
        // Assert to catch division by zero during development
        assert(scalar != 0.0f && "Division by zero in Vector4::operator/.");
#endif // _DEBUG
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
		w = 0.0f;
    }
    return *this;
}

//===============================
// Quaternion Implementation
//===============================
float Quaternion::Length() const
{
    return sqrtf(x * x + y * y + z * z + w * w);
}

float Quaternion::LengthSq() const
{
    return x * x + y * y + z * z + w * w;
}

Quaternion Quaternion::Normalized() const
{
    float length = sqrtf(x * x + y * y + z * z + w * w);
    if (length > 0.0f)
    {
        return Quaternion(x / length, y / length, z / length, w / length);
    }
    else
    {
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

Quaternion Quaternion::Conjugate() const
{
    return Quaternion(-x, -y, -z, w);
}

Quaternion Quaternion::Inverse() const
{
    using namespace DirectX;
    XMVECTOR q = ToXMVECTOR();
    XMVECTOR inv = XMQuaternionInverse(q);
    return FromXMVECTOR(inv);
}

float Quaternion::Dot(const Quaternion& other) const
{
	return x * other.x + y * other.y + z * other.z + w * other.w;
}

Vector3 Quaternion::RotateVector3(const Vector3& vector) const
{
    XMVECTOR v = XMLoadFloat3(&vector);
    XMVECTOR q = ToXMVECTOR();
    XMVECTOR rotated = XMVector3Rotate(v, q);
    Vector3 result;
    XMStoreFloat3(&result, rotated);

	return result;
}

Vector3 Quaternion::ToEulerDeg() const
{
	// Rotation order : Roll (Z) -> Pitch (X) -> Yaw (Y)
    float pitch = atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y)) * (180.0f / XM_PI);
    float yaw = asinf(std::clamp(2.0f * (w * y - z * x), -1.0f, 1.0f)) * (180.0f / XM_PI);
    float roll = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z)) * (180.0f / XM_PI);

	return Vector3(pitch, yaw, roll);
}

Vector3 Quaternion::ToEulerRad() const
{
	// Rotation order : Roll (Z) -> Pitch (X) -> Yaw (Y)
    float pitch = atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y));
    float yaw = asinf(std::clamp(2.0f * (w * y - z * x), -1.0f, 1.0f));
    float roll = atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));

    return Vector3(pitch, yaw, roll);
}

Matrix4x4 Quaternion::ToRotationMatrix() const
{
    XMVECTOR q = ToXMVECTOR();
    XMMATRIX rot = XMMatrixRotationQuaternion(q);
    Matrix4x4 result;
    XMStoreFloat4x4(&result, rot);
	return result;
}

Quaternion Quaternion::Lerp(const Quaternion& target, float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);

    Quaternion to = target;
    if (Dot(target) < 0.0f) {
		to = Quaternion(-target.x, -target.y, -target.z, -target.w);
    }

    return Quaternion(
        x + (to.x - x) * t,
        y + (to.y - y) * t,
        z + (to.z - z) * t,
        w + (to.w - w) * t
    ).Normalized();
}

Quaternion Quaternion::Slerp(const Quaternion& target, float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);

    Quaternion to = target;
    if (Dot(target) < 0.0f) {
        to = Quaternion(-target.x, -target.y, -target.z, -target.w);
    }

    using namespace DirectX;
    XMVECTOR q1 = ToXMVECTOR();
    XMVECTOR q2 = to.ToXMVECTOR();
    XMVECTOR result = XMQuaternionSlerp(q1, q2, t);
    return FromXMVECTOR(result);
}

bool Quaternion::NearEqual(const Quaternion& other, float epsilon) const
{
	return (std::fabs(x - other.x) <= epsilon) && (std::fabs(y - other.y) <= epsilon) && (std::fabs(z - other.z) <= epsilon) && (std::fabs(w - other.w) <= epsilon);
}

Quaternion Quaternion::CreateFromAxisAngle(const Vector3& axis, float angleRad)
{
    constexpr float kEpsilon = 1e-6f;

    if (axis.LengthSq() <= kEpsilon || std::fabs(angleRad) <= kEpsilon)
    {
        return Quaternion::Identity();
    }

    Vector3 normalized = axis.Normalized();
    float halfAngleRad = angleRad * 0.5f;
    float s = sinf(halfAngleRad);

    return Quaternion(
        normalized.x * s,
        normalized.y * s,
        normalized.z * s,
        cosf(halfAngleRad)
    );
}

Quaternion Quaternion::CreateFromEulerDeg(const Vector3& euler)
{
    float halfPitchRad = euler.x * 0.5f * (XM_PI / 180.0f);
    float halfYawRad = euler.y * 0.5f * (XM_PI / 180.0f);
    float halfRollRad = euler.z * 0.5f * (XM_PI / 180.0f);

    float sp = sinf(halfPitchRad);
    float cp = cosf(halfPitchRad);
    float sy = sinf(halfYawRad);
    float cy = cosf(halfYawRad);
    float sr = sinf(halfRollRad);
    float cr = cosf(halfRollRad);

    return Quaternion(
        sp * cy * cr + cp * sy * sr,
        cp * sy * cr - sp * cy * sr,
        cp * cy * sr - sp * sy * cr,
        cp * cy * cr + sp * sy * sr
    );
}

Quaternion Quaternion::CreateFromEulerRad(const Vector3& euler)
{
    float halfPitch = euler.x * 0.5f;
    float halfYaw = euler.y * 0.5f;
    float halfRoll = euler.z * 0.5f;

    float sp = sinf(halfPitch);
    float cp = cosf(halfPitch);
    float sy = sinf(halfYaw);
    float cy = cosf(halfYaw);
    float sr = sinf(halfRoll);
    float cr = cosf(halfRoll);

    return Quaternion(
        sp * cy * cr + cp * sy * sr,
        cp * sy * cr - sp * cy * sr,
        cp * cy * sr - sp * sy * cr,
        cp * cy * cr + sp * sy * sr
    );
}

Quaternion Quaternion::Lerp(const Quaternion& a, const Quaternion& b, float t)
{
    return a.Lerp(b, t);
}

Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)
{
    return a.Slerp(b, t);
}

Quaternion Quaternion::FromToRotation(const Vector3& from, const Vector3& to)
{
    Vector3 f = from.Normalized();
    Vector3 t = to.Normalized();
    float cosTheta = f.Dot(t);
    Vector3 rotationAxis;

    if (cosTheta < -1.0f + 0.001f)
    {
        // 180 degree rotation around any orthogonal vector
        rotationAxis = Vector3(0.0f, 0.0f, 1.0f).Cross(f);
        if (rotationAxis.Length() < 0.01f) // If collinear, try another axis
            rotationAxis = Vector3(1.0f, 0.0f, 0.0f).Cross(f);
        rotationAxis = rotationAxis.Normalized();
        return CreateFromAxisAngle(rotationAxis, PI);
    }

    rotationAxis = f.Cross(t);
    float s = sqrtf((1.0f + cosTheta) * 2.0f);
    float invs = 1.0f / s;

    return Quaternion(
        rotationAxis.x * invs,
        rotationAxis.y * invs,
        rotationAxis.z * invs,
        s * 0.5f
    ).Normalized();
}

Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up)
{
    if(forward.Length() < 0.001f)
    {
        return Quaternion::Identity(); // No forward direction, return identity
	}

    Vector3 f = forward.Normalized();
    Vector3 r = up.Cross(f).Normalized();

    if(r.Length() < 0.001f) // If forward and up are parallel, choose an arbitrary right vector
    {
        r = Vector3(0.0f, 0.0f, 1.0f).Cross(f).Normalized();
        if (r.Length() < 0.001f) // If still parallel, try another axis
            r = Vector3(1.0f, 0.0f, 0.0f).Cross(f).Normalized();
	}

    Vector3 u = f.Cross(r);
    float trace = r.x + u.y + f.z;
    Quaternion result;

    if (trace > 0.0f)
    {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        result.w = 0.25f * s;
        result.x = (u.z - f.y) / s;
        result.y = (f.x - r.z) / s;
        result.z = (r.y - u.x) / s;
    }
    else if ((r.x > u.y) && (r.x > f.z))
    {
        float s = sqrtf(1.0f + r.x - u.y - f.z) * 2.0f;
        result.w = (u.z - f.y) / s;
        result.x = 0.25f * s;
        result.y = (u.x + r.y) / s;
        result.z = (f.x + r.z) / s;
    }
    else if (u.y > f.z)
    {
        float s = sqrtf(1.0f + u.y - r.x - f.z) * 2.0f;
        result.w = (f.x - r.z) / s;
        result.x = (u.x + r.y) / s;
        result.y = 0.25f * s;
        result.z = (f.y + u.z) / s;
    }
    else
    {
        float s = sqrtf(1.0f + f.z - r.x - u.y) * 2.0f;
        result.w = (r.y - u.x) / s;
        result.x = (f.x + r.z) / s;
        result.y = (f.y + u.z) / s;
        result.z = 0.25f * s;
    }

	return result.Normalized();
}

DirectX::XMVECTOR Quaternion::ToXMVECTOR() const
{
	return DirectX::XMLoadFloat4(this);
}

Quaternion Quaternion::FromXMVECTOR(DirectX::FXMVECTOR q)
{
    Quaternion result;
    DirectX::XMStoreFloat4(&result, q);

    return result;
}

Quaternion& Quaternion::operator=(const Quaternion& rhs)
{
    if (this != &rhs)
    {
        x = rhs.x;
        y = rhs.y;
        z = rhs.z;
        w = rhs.w;
    }
    return *this;
}

Quaternion Quaternion::operator*(const Quaternion& rhs) const
{
    return Quaternion(
        w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
        w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
        w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
        w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
	);
}

Quaternion& Quaternion::operator*=(const Quaternion& rhs)
{
    *this = *this * rhs;
    return *this;
}

//===============================
// Matrix4x4 Implementation
//===============================
Matrix4x4 Matrix4x4::Identity = []() {
    Matrix4x4 m;
	XMStoreFloat4x4(&m, XMMatrixIdentity());
	return m;
	}();

float Matrix4x4::Determinant() const
{
    return XMVectorGetX(XMMatrixDeterminant(*this));
}

Matrix4x4 Matrix4x4::Inverse() const
{
	return Matrix4x4(XMMatrixInverse(nullptr, *this));
}

Vector4 Matrix4x4::GetRow(int row) const
{
    const float* m = &_11;
    int i = row * 4;
    return Vector4(m[i], m[i + 1], m[i + 2], m[i + 3]);
}

Vector4 Matrix4x4::GetCol(int col) const
{
    const float* m = &_11;
    return Vector4(m[col], m[col + 4], m[col + 8], m[col + 12]);
}

Vector3 Matrix4x4::GetTranslation() const
{
    return GetRow(3).xyz();
}

Quaternion Matrix4x4::GetRotation() const
{
    XMVECTOR scale, rotQuat, translation;
    if (XMMatrixDecompose(&scale, &rotQuat, &translation, *this))
    {
        return Quaternion(rotQuat);
    }
	return Quaternion::Identity();
}

Vector3 Matrix4x4::GetScale() const
{
    return Vector3(GetRow(0).xyz().Length(), GetRow(1).xyz().Length(), GetRow(2).xyz().Length());
}

bool Matrix4x4::Decompose(Vector3& outTranslation, Quaternion& outRotation, Vector3& outScale) const
{
    XMVECTOR scale, rotQuat, translation;
    if (XMMatrixDecompose(&scale, &rotQuat, &translation, *this))
    {
        outTranslation = Vector3(translation);
        outRotation = Quaternion(rotQuat);
        outScale = Vector3(scale);
        return true;
    }
	return false;
}

Vector3 Matrix4x4::TransformPoint(const Vector3& point) const
{
    return Vector3(XMVector3TransformCoord(point, *this));
}

Vector3 Matrix4x4::TransformDirection(const Vector3& direction) const
{
	return Vector3(XMVector3TransformNormal(direction, *this));
}

Vector3 Matrix4x4::TransformVector(const Vector3& vector) const {
    return Vector3(XMVector3TransformNormal(vector, *this));
}

Vector3 Matrix4x4::TransformNormal(const Vector3& normal) const {
    XMVECTOR det;
    XMMATRIX invTrans = XMMatrixTranspose(XMMatrixInverse(&det, *this));
    return Vector3(XMVector3TransformNormal(normal, invTrans));
}

Matrix4x4 Matrix4x4::Transpose() const
{
	return Matrix4x4(XMMatrixTranspose(*this));
}

Matrix4x4 Matrix4x4::CreateTranslation(const Vector3& translation)
{
	return Matrix4x4(XMMatrixTranslation(translation.x, translation.y, translation.z));
}

Matrix4x4 Matrix4x4::CreateScale(const Vector3& scale)
{
	return Matrix4x4(XMMatrixScaling(scale.x, scale.y, scale.z));
}

Matrix4x4 Matrix4x4::CreateRotationX(float angleRad)
{
	return Matrix4x4(XMMatrixRotationX(angleRad));
}

Matrix4x4 Matrix4x4::CreateRotationY(float angleRad)
{
    return Matrix4x4(XMMatrixRotationY(angleRad));
}

Matrix4x4 Matrix4x4::CreateRotationZ(float angleRad)
{
    return Matrix4x4(XMMatrixRotationZ(angleRad));
}

Matrix4x4 Matrix4x4::CreateScale(float scale)
{
	return Matrix4x4(XMMatrixScaling(scale, scale, scale));
}

Matrix4x4 Matrix4x4::CreateFromQuaternion(const Quaternion& q)
{
	return Matrix4x4(XMMatrixRotationQuaternion(q.ToXMVECTOR()));
}

Matrix4x4 Matrix4x4::CreateTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
{
    Matrix4x4 scaleMatrix = Matrix4x4::CreateScale(scale);
    Matrix4x4 rotationMatrix = Matrix4x4::CreateFromQuaternion(rotation);
    Matrix4x4 translationMatrix = Matrix4x4::CreateTranslation(translation);

    return scaleMatrix * rotationMatrix * translationMatrix;
}

Matrix4x4 Matrix4x4::CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
{
    return Matrix4x4(XMMatrixLookAtLH(eye, target, up));
}

Matrix4x4 Matrix4x4::CreatePerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
	return Matrix4x4(XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ));
}

Matrix4x4 Matrix4x4::CreateOrthographic(float width, float height, float nearZ, float farZ)
{
    return Matrix4x4(XMMatrixOrthographicLH(width, height, nearZ, farZ));
}

Matrix4x4 Matrix4x4::CreateBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& cameraUp)
{
    constexpr float kEpsilon = 1e-6f;

    auto makeTranslationIdentity = [&](const Vector3& pos) -> Matrix4x4
        {
            Matrix4x4 m = Identity;
            m._41 = pos.x;
            m._42 = pos.y;
            m._43 = pos.z;
            return m;
        };

    Vector3 view = cameraPos - objectPos;
    if (view.LengthSq() <= kEpsilon)
    {
        return makeTranslationIdentity(objectPos);
    }

    Vector3 forward = view.Normalized();

    Vector3 upRef = cameraUp;
    if (upRef.LengthSq() <= kEpsilon)
    {
        upRef = Vector3::Up();
    }
    else
    {
        upRef = upRef.Normalized();
    }

    Vector3 right = upRef.Cross(forward);

    if (right.LengthSq() <= kEpsilon)
    {
        Vector3 fallbackUp = Vector3::Forward();

        if (std::fabs(fallbackUp.Dot(forward)) > 0.999f)
        {
            fallbackUp = Vector3::Right();
        }

        right = fallbackUp.Cross(forward);

        if (right.LengthSq() <= kEpsilon)
        {
            return makeTranslationIdentity(objectPos);
        }
    }

    right = right.Normalized();

    Vector3 up = forward.Cross(right).Normalized();

    Matrix4x4 billboardMatrix = Identity;
    billboardMatrix._11 = right.x;
    billboardMatrix._12 = right.y;
    billboardMatrix._13 = right.z;
    billboardMatrix._14 = 0.0f;
    billboardMatrix._21 = up.x;
    billboardMatrix._22 = up.y;
    billboardMatrix._23 = up.z;
    billboardMatrix._24 = 0.0f;
    billboardMatrix._31 = forward.x;
    billboardMatrix._32 = forward.y;
    billboardMatrix._33 = forward.z;
    billboardMatrix._34 = 0.0f;
    billboardMatrix._41 = objectPos.x;
    billboardMatrix._42 = objectPos.y;
    billboardMatrix._43 = objectPos.z;
    billboardMatrix._44 = 1.0f;

    return billboardMatrix;
}

Matrix4x4 Matrix4x4::CreateCylindricalBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& cameraUp,const Vector3& axis)
{
    constexpr float kEpsilon = 1e-6f;

    auto makeTranslationIdentity = [&](const Vector3& pos) -> Matrix4x4
        {
            Matrix4x4 m = Identity;
            m._41 = pos.x;
            m._42 = pos.y;
            m._43 = pos.z;
            return m;
        };

    Vector3 view = cameraPos - objectPos;
    if (view.LengthSq() <= kEpsilon)
    {
        return makeTranslationIdentity(objectPos);
    }

    Vector3 up = axis.Normalized();
    if (up.LengthSq() <= kEpsilon)
    {
        return CreateBillboard(objectPos, cameraPos, cameraUp);
    }

    Vector3 planarForward = view.Reject(up);

    if (planarForward.LengthSq() <= kEpsilon)
    {
        planarForward = cameraUp.Reject(up);

        if (planarForward.LengthSq() <= kEpsilon)
        {
            planarForward = Vector3::Forward().Reject(up);

            if (planarForward.LengthSq() <= kEpsilon)
            {
                planarForward = Vector3::Right().Reject(up);
            }
        }
    }

    Vector3 forward = planarForward.Normalized();
    Vector3 right = up.Cross(forward).Normalized();
    forward = right.Cross(up).Normalized();

    Matrix4x4 billboardMatrix = Identity;
    billboardMatrix._11 = right.x;
    billboardMatrix._12 = right.y;
    billboardMatrix._13 = right.z;
    billboardMatrix._14 = 0.0f;
    billboardMatrix._21 = up.x;
    billboardMatrix._22 = up.y;
    billboardMatrix._23 = up.z;
    billboardMatrix._24 = 0.0f;
    billboardMatrix._31 = forward.x;
    billboardMatrix._32 = forward.y;
    billboardMatrix._33 = forward.z;
    billboardMatrix._34 = 0.0f;
    billboardMatrix._41 = objectPos.x;
    billboardMatrix._42 = objectPos.y;
    billboardMatrix._43 = objectPos.z;
    billboardMatrix._44 = 1.0f;

    return billboardMatrix;
}

Matrix4x4 Matrix4x4::Transpose(const Matrix4x4& m)
{
	return Matrix4x4(XMMatrixTranspose(m));
}

Matrix4x4& Matrix4x4::operator=(const Matrix4x4& rhs)
{
    if (this != &rhs)
    {
        _11 = rhs._11; _12 = rhs._12; _13 = rhs._13; _14 = rhs._14;
        _21 = rhs._21; _22 = rhs._22; _23 = rhs._23; _24 = rhs._24;
        _31 = rhs._31; _32 = rhs._32; _33 = rhs._33; _34 = rhs._34;
        _41 = rhs._41; _42 = rhs._42; _43 = rhs._43; _44 = rhs._44;
    }
	return *this;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const
{
    return Matrix4x4(XMMatrixMultiply(*this, rhs));
}

Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& rhs)
{
    *this = *this * rhs;
    return *this;
}

Matrix4x4::operator DirectX::XMMATRIX() const
{
	return XMLoadFloat4x4(this);
}

Quaternion::operator XMVECTOR() const 
{
    return XMLoadFloat4(this);
}

//===============================
// Transform3D Implementation
//===============================
Matrix4x4 Transform3D::GetMatrix() const
{
    return Matrix4x4::CreateTRS(position, rotation, scale);
}

Matrix4x4 Transform3D::GetInverseMatrix() const
{
	return GetMatrix().Inverse();
}
