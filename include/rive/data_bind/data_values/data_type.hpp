#ifndef _RIVE_DATA_TYPE_HPP_
#define _RIVE_DATA_TYPE_HPP_
namespace rive
{
/// Data types used for converters.
enum class DataType : unsigned int
{
    /// None.
    none = 0,

    /// String.
    string = 1,

    /// Number.
    number = 2,

    /// Bool.
    boolean = 3,

    /// Color.
    color = 4,

    /// List.
    list = 5,

    /// Enum.
    enumType = 6,

    /// Trigger.
    trigger = 7,

    /// View Model.
    viewModel = 8,

    /// Integer.
    integer = 9,

    /// Symbol list index.
    symbolListIndex = 10,

    /// Asset Image.
    assetImage = 11,

    /// Artboard.
    artboard = 12
};
} // namespace rive
#endif