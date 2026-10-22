#ifndef _RIVE_LINEAR_GRADIENT_BASE_HPP_
#define _RIVE_LINEAR_GRADIENT_BASE_HPP_
#include "rive/container_component.hpp"
#include "rive/core/field_types/core_double_type.hpp"
namespace rive
{
class LinearGradientBase : public ContainerComponent
{
protected:
  typedef ContainerComponent Super;

public:
  static const uint16_t typeKey = 22;

  uint16_t coreType() const override { return typeKey; }

  static const uint16_t startXPropertyKey = 42;
  static const uint16_t startYPropertyKey = 33;
  static const uint16_t endXPropertyKey = 34;
  static const uint16_t endYPropertyKey = 35;
  static const uint16_t opacityPropertyKey = 46;
  static const uint16_t transformXPropertyKey = 1103;
  static const uint16_t transformYPropertyKey = 1104;
  static const uint16_t transformRotationPropertyKey = 1105;
  static const uint16_t transformScaleXPropertyKey = 1106;
  static const uint16_t transformScaleYPropertyKey = 1107;

protected:
  float m_StartX = 0.0f;
  float m_StartY = 0.0f;
  float m_EndX = 0.0f;
  float m_EndY = 0.0f;
  float m_Opacity = 1.0f;
  float m_TransformX = 0.0f;
  float m_TransformY = 0.0f;
  float m_TransformRotation = 0.0f;
  float m_TransformScaleX = 1.0f;
  float m_TransformScaleY = 1.0f;

public:
  inline float startX() const { return m_StartX; }
  void startX(float value)
  {
    if (m_StartX == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(startXPropertyKey, &m_StartX, &value);
    m_StartX = value;
    RIVE_EDITOR_CHANGED(startXChanged());
    notifyPropertyChanged(startXPropertyKey);
  }

  inline float startY() const { return m_StartY; }
  void startY(float value)
  {
    if (m_StartY == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(startYPropertyKey, &m_StartY, &value);
    m_StartY = value;
    RIVE_EDITOR_CHANGED(startYChanged());
    notifyPropertyChanged(startYPropertyKey);
  }

  inline float endX() const { return m_EndX; }
  void endX(float value)
  {
    if (m_EndX == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(endXPropertyKey, &m_EndX, &value);
    m_EndX = value;
    RIVE_EDITOR_CHANGED(endXChanged());
    notifyPropertyChanged(endXPropertyKey);
  }

  inline float endY() const { return m_EndY; }
  void endY(float value)
  {
    if (m_EndY == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(endYPropertyKey, &m_EndY, &value);
    m_EndY = value;
    RIVE_EDITOR_CHANGED(endYChanged());
    notifyPropertyChanged(endYPropertyKey);
  }

  inline float opacity() const { return m_Opacity; }
  void opacity(float value)
  {
    if (m_Opacity == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(opacityPropertyKey, &m_Opacity, &value);
    m_Opacity = value;
    RIVE_EDITOR_CHANGED(opacityChanged());
    notifyPropertyChanged(opacityPropertyKey);
  }

  inline float transformX() const { return m_TransformX; }
  void transformX(float value)
  {
    if (m_TransformX == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(transformXPropertyKey, &m_TransformX, &value);
    m_TransformX = value;
    RIVE_EDITOR_CHANGED(transformXChanged());
    notifyPropertyChanged(transformXPropertyKey);
  }

  inline float transformY() const { return m_TransformY; }
  void transformY(float value)
  {
    if (m_TransformY == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(transformYPropertyKey, &m_TransformY, &value);
    m_TransformY = value;
    RIVE_EDITOR_CHANGED(transformYChanged());
    notifyPropertyChanged(transformYPropertyKey);
  }

  inline float transformRotation() const { return m_TransformRotation; }
  void transformRotation(float value)
  {
    if (m_TransformRotation == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(
        transformRotationPropertyKey,
        &m_TransformRotation,
        &value);
    m_TransformRotation = value;
    RIVE_EDITOR_CHANGED(transformRotationChanged());
    notifyPropertyChanged(transformRotationPropertyKey);
  }

  inline float transformScaleX() const { return m_TransformScaleX; }
  void transformScaleX(float value)
  {
    if (m_TransformScaleX == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(
        transformScaleXPropertyKey,
        &m_TransformScaleX,
        &value);
    m_TransformScaleX = value;
    RIVE_EDITOR_CHANGED(transformScaleXChanged());
    notifyPropertyChanged(transformScaleXPropertyKey);
  }

  inline float transformScaleY() const { return m_TransformScaleY; }
  void transformScaleY(float value)
  {
    if (m_TransformScaleY == value)
    {
      return;
    }
    RIVE_EDITOR_CHANGING(
        transformScaleYPropertyKey,
        &m_TransformScaleY,
        &value);
    m_TransformScaleY = value;
    RIVE_EDITOR_CHANGED(transformScaleYChanged());
    notifyPropertyChanged(transformScaleYPropertyKey);
  }

  Core* clone() const override;
  void copy(const LinearGradientBase& object)
  {
    m_StartX = object.m_StartX;
    m_StartY = object.m_StartY;
    m_EndX = object.m_EndX;
    m_EndY = object.m_EndY;
    m_Opacity = object.m_Opacity;
    m_TransformX = object.m_TransformX;
    m_TransformY = object.m_TransformY;
    m_TransformRotation = object.m_TransformRotation;
    m_TransformScaleX = object.m_TransformScaleX;
    m_TransformScaleY = object.m_TransformScaleY;
    ContainerComponent::copy(object);
  }

  bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
  {
    switch (propertyKey)
    {
      case startXPropertyKey:
        m_StartX = CoreDoubleType::deserialize(reader);
        return true;
      case startYPropertyKey:
        m_StartY = CoreDoubleType::deserialize(reader);
        return true;
      case endXPropertyKey:
        m_EndX = CoreDoubleType::deserialize(reader);
        return true;
      case endYPropertyKey:
        m_EndY = CoreDoubleType::deserialize(reader);
        return true;
      case opacityPropertyKey:
        m_Opacity = CoreDoubleType::deserialize(reader);
        return true;
      case transformXPropertyKey:
        m_TransformX = CoreDoubleType::deserialize(reader);
        return true;
      case transformYPropertyKey:
        m_TransformY = CoreDoubleType::deserialize(reader);
        return true;
      case transformRotationPropertyKey:
        m_TransformRotation = CoreDoubleType::deserialize(reader);
        return true;
      case transformScaleXPropertyKey:
        m_TransformScaleX = CoreDoubleType::deserialize(reader);
        return true;
      case transformScaleYPropertyKey:
        m_TransformScaleY = CoreDoubleType::deserialize(reader);
        return true;
    }
    return ContainerComponent::deserialize(propertyKey, reader);
  }

protected:
  virtual void startXChanged() {}
  virtual void startYChanged() {}
  virtual void endXChanged() {}
  virtual void endYChanged() {}
  virtual void opacityChanged() {}
  virtual void transformXChanged() {}
  virtual void transformYChanged() {}
  virtual void transformRotationChanged() {}
  virtual void transformScaleXChanged() {}
  virtual void transformScaleYChanged() {}
#ifdef WITH_RIVE_EDITOR
#include "editor_native/generated/shapes/paint/linear_gradient_ext.inl"
#endif
};
} // namespace rive

#endif