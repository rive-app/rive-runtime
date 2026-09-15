import '../field_type.dart';

class ColorFieldType extends FieldType {
  ColorFieldType()
      : super(
          'Color',
          'CoreColorType',
          cppName: 'int',
        );

  @override
  String get defaultValue => '0';

  // Editor coop wire encodes colors via the varuint `deserializeRev`
  // path, same as doubles. The runtime `.riv` import path stays on
  // raw `deserialize` (4 raw bytes). Without this override, every
  // coop-applied color value (SolidColor, GradientStop, viewmodel
  // color, custom property color, keyframe color) gets read as raw
  // uint32 from a varuint stream → garbage values.
  @override
  String get deserializeFunction => 'CoreColorType::deserializeRev';
}
