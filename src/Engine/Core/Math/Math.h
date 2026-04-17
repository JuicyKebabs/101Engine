#pragma once
#include <DirectXMath.h>
#include <numbers>
#include <cmath>
#include <string>

// Forward declarations
struct Vector2;
struct Vector3;
struct Vector4;
struct Quaternion;
struct Matrix4x4;

// 2D vector for positions, directions, and scales
struct Vector2 : public DirectX::XMFLOAT2
{
	// Constructors
	Vector2() : DirectX::XMFLOAT2(0.0f, 0.0f) {}
	Vector2(float value) : DirectX::XMFLOAT2(value, value) {}
	Vector2(float x, float y) : DirectX::XMFLOAT2(x, y) {}
	Vector2(float arr[2]) : DirectX::XMFLOAT2(arr[0], arr[1]) {}
	explicit Vector2(const DirectX::XMFLOAT2& v) : DirectX::XMFLOAT2(v) {}

	// Constants
	static Vector2 Zero()		{ return Vector2(0.0f, 0.0f); }
	static Vector2 One()		{ return Vector2(1.0f, 1.0f); }
	static Vector2 UnitX()		{ return Vector2(1.0f, 0.0f); }
	static Vector2 UnitY()		{ return Vector2(0.0f, 1.0f); }

	// Methods
	float Length() const;
	float LengthSq() const;
	float DistanceTo(const Vector2& other) const;
	float DistanceSqTo(const Vector2& other) const;
	Vector2 Lerp(const Vector2& target, float t) const;
	Vector2 Min(const Vector2& other) const;
	Vector2 Max(const Vector2& other) const;
	Vector2 Clamp(const Vector2& min, const Vector2& max) const;
	Vector2 Abs() const;
	Vector2 Normalized() const;
	float Dot(const Vector2& other) const;
	bool NearEqual(const Vector2& other, float epsilon = 1e-6f) const;

	// Statics
	static float Distance(const Vector2& a, const Vector2& b);
	static float DistanceSq(const Vector2& a, const Vector2& b);
	static Vector2 Lerp(const Vector2& a, const Vector2& b, float t);
	static Vector2 Min(const Vector2& a, const Vector2& b);
	static Vector2 Max(const Vector2& a, const Vector2& b);
	static Vector2 Clamp(const Vector2& value, const Vector2& min, const Vector2& max);

	// Operators
	Vector2& operator= (const Vector2& rhs);
	Vector2 operator+() const;
	Vector2 operator-() const;
	Vector2 operator+(const Vector2& rhs) const;
	Vector2 operator-(const Vector2& rhs) const;
	Vector2 operator*(float scalar) const;
	Vector2 operator/(float scalar) const;
	Vector2& operator+=(const Vector2& rhs);
	Vector2& operator-=(const Vector2& rhs);
	Vector2& operator*=(float scalar);
	Vector2& operator/=(float scalar);
};

// 3D vector for positions, directions, and scales
struct Vector3 : public DirectX::XMFLOAT3
{
	// Constructors
	Vector3() : DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f) {}
	Vector3(float value) : DirectX::XMFLOAT3(value, value, value) {}
	Vector3(float x, float y, float z) : DirectX::XMFLOAT3(x, y, z) {}
	Vector3(float arr[3]) : DirectX::XMFLOAT3(arr[0], arr[1], arr[2]) {}
	explicit Vector3(const DirectX::XMFLOAT3& v) : DirectX::XMFLOAT3(v) {}
	explicit Vector3(DirectX::FXMVECTOR v) { DirectX::XMStoreFloat3(this, v); }

	// Constants
	static Vector3 Zero()		{ return Vector3( 0.0f,  0.0f,  0.0f); }
	static Vector3 One()		{ return Vector3( 1.0f,  1.0f,  1.0f); }
	static Vector3 UnitX()		{ return Vector3( 1.0f,  0.0f,  0.0f); }
	static Vector3 UnitY()		{ return Vector3( 0.0f,  1.0f,  0.0f); }
	static Vector3 UnitZ()		{ return Vector3( 0.0f,  0.0f,  1.0f); }
	static Vector3 Up()			{ return Vector3( 0.0f,  1.0f,  0.0f); }
	static Vector3 Down()		{ return Vector3( 0.0f, -1.0f,  0.0f); }
	static Vector3 Left()		{ return Vector3(-1.0f,  0.0f,  0.0f); }
	static Vector3 Right()		{ return Vector3( 1.0f,  0.0f,  0.0f); }
	static Vector3 Forward()	{ return Vector3( 0.0f,  0.0f,  1.0f); }
	static Vector3 Backward()	{ return Vector3( 0.0f,  0.0f, -1.0f); }

	// Methods
	float Length() const;
	float LengthSq() const;
	float DistanceTo(const Vector3& other) const;
	float DistanceSqTo(const Vector3& other) const;
	Vector3 Lerp(const Vector3& target, float t) const;
	Vector3 Min(const Vector3& other) const;
	Vector3 Max(const Vector3& other) const;
	Vector3 Clamp(const Vector3& min, const Vector3& max) const;
	Vector3 Abs() const;
	Vector3 Normalized() const;
	Vector3 Cross(const Vector3& other) const;
	float Dot(const Vector3& other) const;
	Vector3 Reflect(const Vector3& normal) const;
	Vector3 Refract(const Vector3& normal, float eta) const;
	bool NearEqual(const Vector3& other, float epsilon = 1e-6f) const;
	Vector3 Project(const Vector3& onto) const;
	Vector3 Reject(const Vector3& onto) const;
	float Angle(const Vector3& other) const;

	// Statics
	static float Distance(const Vector3& a, const Vector3& b);
	static float DistanceSq(const Vector3& a, const Vector3& b);
	static Vector3 Lerp(const Vector3& a, const Vector3& b, float t);
	static Vector3 Min(const Vector3& a, const Vector3& b);
	static Vector3 Max(const Vector3& a, const Vector3& b);
	static Vector3 Clamp(const Vector3& value, const Vector3& min, const Vector3& max);
	static Vector3 Transform(const Vector3& v, const Matrix4x4& m);

	// Operators
	Vector3& operator= (const Vector3& rhs);
	Vector3 operator+() const;
	Vector3 operator-() const;
	Vector3 operator+(const Vector3& rhs) const;
	Vector3 operator-(const Vector3& rhs) const;
	Vector3 operator*(float scalar) const;
	Vector3 operator/(float scalar) const;
	Vector3& operator+=(const Vector3& rhs);
	Vector3& operator-=(const Vector3& rhs);
	Vector3& operator*=(float scalar);
	Vector3& operator/=(float scalar);
	operator DirectX::XMVECTOR() const;
};

// 4D vector for homogeneous coordinates and quaternions
struct Vector4 : public DirectX::XMFLOAT4
{
	// Constructors
	Vector4() : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f) {}
	Vector4(float value) : DirectX::XMFLOAT4(value, value, value, value) {}
	Vector4(float x, float y, float z, float w) : DirectX::XMFLOAT4(x, y, z, w) {}
	Vector4(float arr[4]) : DirectX::XMFLOAT4(arr[0], arr[1], arr[2], arr[3]) {}
	explicit Vector4(const DirectX::XMFLOAT4& v) : DirectX::XMFLOAT4(v) {}

	// Constants
	static Vector4 Zero()		{ return Vector4(0.0f, 0.0f, 0.0f, 0.0f); }
	static Vector4 One()		{ return Vector4(1.0f, 1.0f, 1.0f, 1.0f); }
	static Vector4 UnitX()		{ return Vector4(1.0f, 0.0f, 0.0f, 0.0f); }
	static Vector4 UnitY()		{ return Vector4(0.0f, 1.0f, 0.0f, 0.0f); }
	static Vector4 UnitZ()		{ return Vector4(0.0f, 0.0f, 1.0f, 0.0f); }
	static Vector4 UnitW()		{ return Vector4(0.0f, 0.0f, 0.0f, 1.0f); }

	// Methods
	float Length() const;
	float LengthSq() const;
	float DistanceTo(const Vector4& other) const;
	float DistanceSqTo(const Vector4& other) const;
	Vector4 Lerp(const Vector4& target, float t) const;
	Vector4 Min(const Vector4& other) const;
	Vector4 Max(const Vector4& other) const;
	Vector4 Clamp(const Vector4& min, const Vector4& max) const;
	Vector4 Abs() const;
	Vector4 Normalized() const;
	float Dot(const Vector4& other) const;
	bool NearEqual(const Vector4& other, float epsilon = 1e-6f) const;
	Vector3 xyz() const { return Vector3(x, y, z); }

	// Statics
	static float Distance(const Vector4& a, const Vector4& b);
	static float DistanceSq(const Vector4& a, const Vector4& b);
	static Vector4 Lerp(const Vector4& a, const Vector4& b, float t);
	static Vector4 Min(const Vector4& a, const Vector4& b);
	static Vector4 Max(const Vector4& a, const Vector4& b);
	static Vector4 Clamp(const Vector4& value, const Vector4& min, const Vector4& max);

	// Operators
	Vector4& operator= (const Vector4& rhs);
	Vector4 operator+() const;
	Vector4 operator-() const;
	Vector4 operator+(const Vector4& rhs) const;
	Vector4 operator-(const Vector4& rhs) const;
	Vector4 operator*(float scalar) const;
	Vector4 operator*(const Vector4& rhs) const;
	Vector4 operator/(float scalar) const;
	Vector4& operator+=(const Vector4& rhs);
	Vector4& operator-=(const Vector4& rhs);
	Vector4& operator*=(float scalar);
	Vector4& operator/=(float scalar);
};

// Quaternion for 3D rotations
struct Quaternion : public DirectX::XMFLOAT4
{
	// Constructors
	Quaternion() : DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) {}
	Quaternion(float value) : DirectX::XMFLOAT4(value, value, value, value) {}
	Quaternion(float x, float y, float z, float w) : DirectX::XMFLOAT4(x, y, z, w) {}
	Quaternion(float arr[4]) : DirectX::XMFLOAT4(arr[0], arr[1], arr[2], arr[3]) {}
	explicit Quaternion(DirectX::XMFLOAT3 vec3) : DirectX::XMFLOAT4(vec3.x, vec3.y, vec3.z, 1.0f) {}
	explicit Quaternion(DirectX::XMFLOAT4 vec4) : DirectX::XMFLOAT4(vec4.x, vec4.y, vec4.z, vec4.w) {}
	explicit Quaternion(DirectX::FXMVECTOR m) { DirectX::XMStoreFloat4(this, m); }

	// Constants
	static Quaternion Zero() { return Quaternion(0.0f, 0.0f, 0.0f, 0.0f); }
	static Quaternion One() { return Quaternion(1.0f, 1.0f, 1.0f, 1.0f); }
	static Quaternion Identity() { return Quaternion(0.0f, 0.0f, 0.0f, 1.0f); }

	// Methods
	float Length() const;
	float LengthSq() const;
	Quaternion Normalized() const;
	Quaternion Conjugate() const;
	Quaternion Inverse() const;
	float Dot(const Quaternion& other) const;
	Vector3 RotateVector3(const Vector3& vector) const;	// Rotate a vector by this quaternion (Radians)
	Vector3 ToEulerDeg() const;	// Rotation order : Roll (Z) -> Pitch (X) -> Yaw (Y)
	Vector3 ToEulerRad() const;	// Rotation order : Roll (Z) -> Pitch (X) -> Yaw (Y)
	Matrix4x4 ToRotationMatrix() const;
	Quaternion Lerp(const Quaternion& target, float t) const;
	Quaternion Slerp(const Quaternion& target, float t) const;
	bool NearEqual(const Quaternion& other, float epsilon = 1e-6f) const;

	// Statics
	static Quaternion CreateFromAxisAngle(const Vector3& axis, float angleRad);
	static Quaternion CreateFromEulerDeg(const Vector3& euler);
	static Quaternion CreateFromEulerRad(const Vector3& euler);
	static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t);
	static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
	static Quaternion FromToRotation(const Vector3& from, const Vector3& to);
	static Quaternion LookRotation(const Vector3& forward = Vector3::Forward(), const Vector3& up = Vector3::Up());

	// XMVECTOR Interop
	DirectX::XMVECTOR ToXMVECTOR() const;
	static Quaternion FromXMVECTOR(DirectX::FXMVECTOR q);

	// Operators
	Quaternion& operator= (const Quaternion& rhs);
	Quaternion operator*(const Quaternion& rhs) const;
	Quaternion& operator*=(const Quaternion& rhs);
	operator DirectX::XMVECTOR() const;
};

// 4x4 Matrix for 3D transformations (translation, rotation, scale)
struct Matrix4x4 : public DirectX::XMFLOAT4X4
{
	// Constructors
	Matrix4x4() : DirectX::XMFLOAT4X4() {}
	Matrix4x4(float a, float b, float c, float d,
			  float e, float f, float g, float h,
			  float i, float j, float k, float l,
			  float m, float n, float o, float p) : DirectX::XMFLOAT4X4(
				  a, b, c, d,
				  e, f, g, h,
				  i, j, k, l,
				  m, n, o, p) {
	}
	Matrix4x4(Vector4 v1, Vector4 v2, Vector4 v3, Vector4 v4) : DirectX::XMFLOAT4X4(
		v1.x, v1.y, v1.z, v1.w,
		v2.x, v2.y, v2.z, v2.w,
		v3.x, v3.y, v3.z, v3.w,
		v4.x, v4.y, v4.z, v4.w) {
	}
	Matrix4x4(float arr[4][4]) : DirectX::XMFLOAT4X4(
		arr[0][0], arr[0][1], arr[0][2], arr[0][3],
		arr[1][0], arr[1][1], arr[1][2], arr[1][3],
		arr[2][0], arr[2][1], arr[2][2], arr[2][3],
		arr[3][0], arr[3][1], arr[3][2], arr[3][3]) {
	}
	Matrix4x4(float arr[16]) : DirectX::XMFLOAT4X4(
		arr[0], arr[1], arr[2], arr[3],
		arr[4], arr[5], arr[6], arr[7],
		arr[8], arr[9], arr[10], arr[11],
		arr[12], arr[13], arr[14], arr[15]) {
	}
	explicit Matrix4x4(const DirectX::XMFLOAT4X4& m) : DirectX::XMFLOAT4X4(m) {}
	explicit Matrix4x4(const DirectX::XMMATRIX& m) {DirectX::XMStoreFloat4x4(this, m);}

	// Constants
	static Matrix4x4 Identity;

	// Methods
	float Determinant() const;
	Matrix4x4 Transpose() const;
	Matrix4x4 Inverse() const;
	Vector4 GetRow(int row) const;
	Vector4 GetCol(int col) const;

	Vector3 GetTranslation() const;
	Quaternion GetRotation() const;	// SRT decomposition to extract rotation as quaternion
	Vector3 GetScale() const;	// SRT decomposition to extract scale
	bool Decompose(Vector3& outTranslation, Quaternion& outRotation, Vector3& outScale) const;
	Vector3 TransformPoint(const Vector3& point) const;
	Vector3 TransformDirection(const Vector3& direction) const;
	Vector3 TransformVector(const Vector3& vector) const;
	Vector3 TransformNormal(const Vector3& normal) const;

	// Statics
	static Matrix4x4 CreateTranslation(const Vector3& translation);
	static Matrix4x4 CreateRotationX(float angleRad);
	static Matrix4x4 CreateRotationY(float angleRad);
	static Matrix4x4 CreateRotationZ(float angleRad);
	static Matrix4x4 CreateScale(float scale);
	static Matrix4x4 CreateScale(const Vector3& scale);
	static Matrix4x4 CreateFromQuaternion(const Quaternion& q);
	static Matrix4x4 CreateTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);
	static Matrix4x4 CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
	static Matrix4x4 CreatePerspectiveFov(float fovY, float aspect, float nearZ, float farZ);
	static Matrix4x4 CreateOrthographic(float width, float height, float nearZ, float farZ);
	static Matrix4x4 CreateBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& cameraUp);
	static Matrix4x4 CreateCylindricalBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& cameraUp, const Vector3& axis);
	static Matrix4x4 Transpose(const Matrix4x4& m);

	// Operators
	Matrix4x4& operator= (const Matrix4x4& rhs);
	Matrix4x4 operator*(const Matrix4x4& rhs) const;
	Matrix4x4& operator*=(const Matrix4x4& rhs);

	operator DirectX::XMMATRIX() const;
};

//transform structure for 3D objects
struct Transform3D
{
	Vector3 position = { 0.0f, 0.0f, 0.0f };			//position
	Quaternion rotation = { 0.0f, 0.0f, 0.0f, 1.0f };	//rotation(Quaternion)
	Vector3 scale = { 1.0f, 1.0f, 1.0f };				//scale

	Matrix4x4 GetMatrix() const;
	Matrix4x4 GetInverseMatrix() const;
};

constexpr float PI = std::numbers::pi_v<float>;
constexpr float PI_DIV_2 = PI / 2.0f;
constexpr float PI_DIV_4 = PI / 4.0f;

inline float DegToRad(float degrees) {
	return degrees * (PI / 180.0f);
}

inline Vector2 DegToRad(const Vector2& degrees) {
	return Vector2(DegToRad(degrees.x), DegToRad(degrees.y));
}

inline Vector3 DegToRad(const Vector3& degrees) {
	return Vector3(DegToRad(degrees.x), DegToRad(degrees.y), DegToRad(degrees.z));
}

inline float RadToDeg(float radians) {
	return radians * (180.0f / PI);
}

inline Vector2 RadToDeg(const Vector2& radians) {
	return Vector2(RadToDeg(radians.x), RadToDeg(radians.y));
}

inline Vector3 RadToDeg(const Vector3& radians) {
	return Vector3(RadToDeg(radians.x), RadToDeg(radians.y), RadToDeg(radians.z));
}