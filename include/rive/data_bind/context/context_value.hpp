#ifndef _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#define _RIVE_DATA_BIND_CONTEXT_VALUE_HPP_
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/data_bind/data_values/data_value_list.hpp"
#include "rive/data_bind/context/context_target_value.hpp"
#include <algorithm>
#include <cmath>
#include <new>
#include <stdio.h>
#include <utility>
#include <type_traits>
namespace rive
{
class DataBindContextValue
{
protected:
  // Points into m_dataValueStorage once the source's value is made.
  DataValue* m_dataValue = nullptr;
  DataBindContextTargetValue m_targetValue;
  bool m_isValid = false;
  // Set when a listener action writes this bind to its source
  // (DataBind::updateSourceBinding(true)), as opposed to the target having
  // changed. Only a trigger reads it: there the write is a fire.
  bool m_sourceWriteRequested = false;

public:
  DataBindContextValue(DataBind* dataBind);
  virtual ~DataBindContextValue()
  {
    if (m_dataValue != nullptr)
    {
      m_dataValue->~DataValue();
    }
  };
  // m_dataValue points into this object's own storage.
  DataBindContextValue(const DataBindContextValue&) = delete;
  DataBindContextValue& operator=(const DataBindContextValue&) = delete;
  virtual void applyToSource(
      Core* component,
      uint32_t propertyKey,
      bool isMainDirection,
      DataBind* dataBind);
  virtual void apply(
      Core* component,
      uint32_t propertyKey,
      bool isMainDirection,
      DataBind* dataBind) {};
  void invalidate() { m_isValid = false; };
  void requestSourceWrite() { m_sourceWriteRequested = true; }
  // Re-read the target after we wrote it ourselves (source->target). The
  // cached target value is what detects target-side changes; if we leave it
  // holding the value from before our own write, a later genuine target
  // change that lands back on that stale value reads as "unchanged" and
  // never propagates to the source. Only toSource binds allocate the cache.
  void refreshTargetValue(DataBind* dataBind)
  {
    if (dataBind->toSource())
    {
      m_targetValue.syncTargetValue(dataBind);
    }
  }
  virtual bool syncTargetValue(Core* target, uint32_t propertyKey)
  {
    return false;
  };
  void syncSourceValue(DataBind* dataBind);
  DataValue* calculateUntypedDataValue(
      DataValue* input,
      bool isMainDirection,
      DataBind* dataBind)
  {
    auto converter = dataBind->converter();
    auto dataValue = converter != nullptr
                         ? isMainDirection
                               ? converter->convert(input, dataBind)
                               : converter->reverseConvert(input, dataBind)
                         : input;
    return dataValue;
  }
  template <typename T = DataValue>
  DataValue* calculateDataValue(
      DataValue* input,
      bool isMainDirection,
      DataBind* dataBind)
  {
    auto dataValue = calculateUntypedDataValue(
        input,
        isMainDirection,
        dataBind);
    if (dataValue->is<T>())
    {
      return dataValue;
    }
    return nullptr;
  };

  template <typename T = DataValue, typename U>
  U calculateValue(DataValue* input, bool isMainDirection, DataBind* dataBind)
  {
    auto dataValue = calculateDataValue<T>(input, isMainDirection, dataBind);
    if (dataValue && dataValue->template is<T>())
    {
      return dataValue->template as<T>()->value();
    }
    return T::defaultValue;
  };
  template <
      typename T = DataValue,
      typename U,
      typename V = ViewModelInstanceValue>
  void calculateValueAndApply(bool isMainDirection, DataBind* dataBind)
  {
    // Check if target value changed or binding has been invalidated
    if (m_targetValue.syncTargetValue(dataBind) || !m_isValid)
    {
      // Calculate new value after converters are applied
      auto value = calculateDataValue<T>(
          m_targetValue.dataValue(),
          isMainDirection,
          dataBind);
      if (value)
      {
        // A validator may rewrite the source synchronously while
        // applyValue notifies its delegates. Keep the requested value before
        // those callbacks can change it.
        const U appliedValue = value->template as<T>()->value();
        dataBind->suppressDirt(true);
        auto source = dataBind->source();
        source->as<V>()->applyValue(value->template as<T>());
        dataBind->suppressDirt(false);
        // Scalar properties expose the value just applied. View-model
        // references have a separate replacement path, not a scalar setter.
        if constexpr (
            std::is_arithmetic<U>::value || std::is_same<U, std::string>::value)
        {
          const auto& sourceValue = source->as<V>()->propertyValue();
          bool corrected = sourceValue != appliedValue;
          if constexpr (std::is_floating_point<U>::value)
          {
            // NaN != NaN does not mean a callback changed the value.
            corrected = corrected &&
                        !(std::isnan(sourceValue) && std::isnan(appliedValue));
          }
          if (corrected)
          {
            // Suppression avoids echoing our own write, but the validator's
            // correction must reach the target through the two-way bind.
            dataBind->addDirt(ComponentDirt::Bindings, false);
          }
        }
        m_isValid = true;
      }
    }
  };

private:
  // The source's value, held here rather than in an allocation of its own:
  // a context value holds exactly one, for as long as it lives. A string
  // and a list are the largest a source makes (see the constructor), and
  // their sizes depend on the standard library, so they size the storage.
  static constexpr size_t kDataValueStorageSize = std::max(
      sizeof(DataValueString),
      sizeof(DataValueList));
  static constexpr size_t kDataValueStorageAlign = std::max(
      alignof(DataValueString),
      alignof(DataValueList));
  alignas(kDataValueStorageAlign) unsigned char m_dataValueStorage
      [kDataValueStorageSize];

protected:
  template <typename T, typename... Args> T* makeDataValue(Args&&... args)
  {
    static_assert(
        sizeof(T) <= kDataValueStorageSize &&
            alignof(T) <= kDataValueStorageAlign,
        "a data value outgrew DataBindContextValue's storage");
    return new (m_dataValueStorage) T(std::forward<Args>(args)...);
  }
};
} // namespace rive

#endif
