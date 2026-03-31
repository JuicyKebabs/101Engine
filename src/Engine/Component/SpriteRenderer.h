#pragma once
#include "Component.h"
#include "Engine/Core/Math/Math.h"

class SpriteRenderer : public Component
{
public:
    SpriteRenderer();
    ~SpriteRenderer();

    void SetTexture(const std::string& texturePath);
    void SetColor(const Vector4& color);
    void SetScale(const Vector2& scale);
    void SetRotation(float rotation);

    const std::string& GetTexture() const;
    const Vector4& GetColor() const;
    const Vector2& GetScale() const;
    float GetRotation() const;

private:

    Vector4 m_color;
};
