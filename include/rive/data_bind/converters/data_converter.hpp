#ifndef _RIVE_DATA_CONVERTER_HPP_
#define _RIVE_DATA_CONVERTER_HPP_
#include "rive/generated/data_bind/converters/data_converter_base.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/data_bind_container.hpp"
#include <stdio.h>
namespace rive
{
class DataBind;
class DataConverter : public DataConverterBase, public DataBindContainer
{
public:
#ifdef WITH_RIVE_EDITOR
  void editorPrepareForDestruction() override;
#endif
  ~DataConverter();
  virtual DataValue* convert(DataValue* value, DataBind* dataBind)
  {
    return value;
  };
  virtual DataValue* reverseConvert(DataValue* value, DataBind* dataBind)
  {
    return value;
  };
  virtual DataType outputType() { return DataType::none; };
  virtual void bindFromContext(DataContext* dataContext, DataBind* dataBind);
  virtual void unbind();
  StatusCode import(ImportStack& importStack) override;
  void markConverterDirty();
  virtual void update();
  void copy(const DataConverter& object);
  // Advances the converters used by this converter's own binds.
  virtual bool advance(float elapsedTime);
  // Whether advance can do work. The base only advances its binds'
  // converters, so a bind using a converter whose binds use none never
  // needs advancing.
  virtual bool mayAdvance() const;
  void dataBindsChanged() override;
  void addDirtyDataBind(DataBind*) override;
  virtual void reset() {};

protected:
  DataBind* m_parentDataBind = nullptr;
};
} // namespace rive

#endif