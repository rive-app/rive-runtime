function makeMatrix(xx, xy, yx, yy, tx, ty) {
  const m = new DOMMatrix();
  m.a = xx;
  m.b = xy;
  m.c = yx;
  m.d = yy;
  m.e = tx;
  m.f = ty;
  return m;
}

function initializeCanvas2DRenderer() {
  const RenderPaintStyle = Module.RenderPaintStyle;
  const FillRule = Module.FillRule;
  const RenderPath = Module.RenderPath;
  const RenderImage = Module.RenderImage;
  const RenderPaint = Module.RenderPaint;
  const Renderer = Module.Renderer;
  const StrokeCap = Module.StrokeCap;
  const StrokeJoin = Module.StrokeJoin;
  const BlendMode = Module.BlendMode;

  const fill = RenderPaintStyle.fill;
  const stroke = RenderPaintStyle.stroke;

  const evenOdd = FillRule.evenOdd;
  const nonZero = FillRule.nonZero;

  function _canvasBlend(value) {
    switch (value) {
      case BlendMode.srcOver:
        return "source-over";
      case BlendMode.screen:
        return "screen";
      case BlendMode.overlay:
        return "overlay";
      case BlendMode.darken:
        return "darken";
      case BlendMode.lighten:
        return "lighten";
      case BlendMode.colorDodge:
        return "color-dodge";
      case BlendMode.colorBurn:
        return "color-burn";
      case BlendMode.hardLight:
        return "hard-light";
      case BlendMode.softLight:
        return "soft-light";
      case BlendMode.difference:
        return "difference";
      case BlendMode.exclusion:
        return "exclusion";
      case BlendMode.multiply:
        return "multiply";
      case BlendMode.hue:
        return "hue";
      case BlendMode.saturation:
        return "saturation";
      case BlendMode.color:
        return "color";
      case BlendMode.luminosity:
        return "luminosity";
    }
  }
  var CanvasRenderPath = RenderPath.extend("CanvasRenderPath", {
    "__construct": function () {
      this["__parent"]["__construct"].call(this);
      this._path2D = new Path2D();
    },
    "rewind": function () {
      this._path2D = new Path2D();
    },
    "addPath": function (path, xx, xy, yx, yy, tx, ty) {
      this._path2D["addPath"](path._path2D, makeMatrix(xx, xy, yx, yy, tx, ty));
    },
    "fillRule": function (fillRule) {
      this._fillRule = fillRule;
    },
    "moveTo": function (x, y) {
      this._path2D["moveTo"](x, y);
    },
    "lineTo": function (x, y) {
      this._path2D["lineTo"](x, y);
    },
    "cubicTo": function (ox, oy, ix, iy, x, y) {
      this._path2D["bezierCurveTo"](ox, oy, ix, iy, x, y);
    },
    "close": function () {
      this._path2D["closePath"]();
    },
  });

  function _colorStyle(value) {
    return (
      "rgba(" +
      ((0x00ff0000 & value) >>> 16) +
      "," +
      ((0x0000ff00 & value) >>> 8) +
      "," +
      ((0x000000ff & value) >>> 0) +
      "," +
      ((0xff000000 & value) >>> 24) / 0xff +
      ")"
    );
  }
  var CanvasRenderPaint = RenderPaint.extend("CanvasRenderPaint", {
    "__construct": function () {
      this["__parent"]["__construct"].call(this);
      this._value = "rgba(0, 0, 0, 1)";
      this._thickness = 1;
      this._join = "bevel";
      this._cap = "round";
      this._style = RenderPaintStyle.fill;
      this._blend = BlendMode.srcOver;
      this._gradient = null;
    },
    "color": function (value) {
      this._value = _colorStyle(value);
    },
    "thickness": function (value) {
      this._thickness = value;
    },
    "join": function (value) {
      switch (value) {
        case StrokeJoin.miter:
          this._join = "miter";
          break;
        case StrokeJoin.round:
          this._join = "round";
          break;
        case StrokeJoin.bevel:
          this._join = "bevel";
          break;
      }
    },
    "cap": function (value) {
      switch (value) {
        case StrokeCap.butt:
          this._cap = "butt";
          break;
        case StrokeCap.round:
          this._cap = "round";
          break;
        case StrokeCap.square:
          this._cap = "square";
          break;
      }
    },
    "style": function (value) {
      this._style = value;
    },
    "blendMode": function (value) {
      this._blend = _canvasBlend(value);
    },
    "clearGradient": function () {
      this._gradient = null;
    },
    "linearGradient": function (sx, sy, ex, ey) {
      this._gradient = {
        sx,
        sy,
        ex,
        ey,
        stops: [],
      };
    },
    "radialGradient": function (sx, sy, ex, ey) {
      this._gradient = {
        sx,
        sy,
        ex,
        ey,
        stops: [],
        isRadial: true,
      };
    },
    "addStop": function (color, stop) {
      this._gradient.stops.push({
        color,
        stop,
      });
    },

    "completeGradient": function () {},
    // https://github.com/rive-app/rive/issues/3816: The fill rule (and only the fill rule) on a
    // path object can mutate before flush(). To work around this, we capture the fill rule at
    // draw time. It's a little awkward having a fill rule here even though we might be a
    // stroke, so we probably want to rework this.
    "draw": function (ctx, path2D, fillRule) {
      let _style = this._style;
      let _value = this._value;
      let _gradient = this._gradient;
      let _blend = this._blend;

      ctx["globalCompositeOperation"] = _blend;

      if (_gradient != null) {
        const sx = _gradient.sx;
        const sy = _gradient.sy;
        const ex = _gradient.ex;
        const ey = _gradient.ey;
        const stops = _gradient.stops;

        if (_gradient.isRadial) {
          var dx = ex - sx;
          var dy = ey - sy;
          var radius = Math.sqrt(dx * dx + dy * dy);
          _value = ctx["createRadialGradient"](sx, sy, 0, sx, sy, radius);
        } else {
          _value = ctx["createLinearGradient"](sx, sy, ex, ey);
        }

        for (let i = 0, l = stops["length"]; i < l; i++) {
          const value = stops[i];
          const stop = value.stop;
          const color = value.color;
          _value["addColorStop"](stop, _colorStyle(color));
        }
        this._value = _value;
        this._gradient = null;
      }
      switch (_style) {
        case stroke:
          ctx["strokeStyle"] = _value;
          ctx["lineWidth"] = this._thickness;
          ctx["lineCap"] = this._cap;
          ctx["lineJoin"] = this._join;
          ctx["stroke"](path2D);
          break;
        case fill:
          ctx["fillStyle"] = _value;
          ctx["fill"](path2D, fillRule);
          break;
      }
    },
  });

  const INITIAL_ATLAS_SIZE = 512;
  let _rectanizer = null;
  let _atlasMeshList = [];
  let _atlasNumTotalVertexFloats = 0;
  let _atlasNumTotalIndices = 0;

  var CanvasRenderer = (Module.CanvasRenderer = Renderer.extend("Renderer", {
    "__construct": function (canvas) {
      this["__parent"]["__construct"].call(this);
      this._ctx = canvas["getContext"]("2d");
      this._canvas = canvas;
    },
    "save": function () {
      this._ctx["save"]();
    },
    "restore": function () {
      this._ctx["restore"]();
    },
    "transform": function (xx, xy, yx, yy, tx, ty) {
      this._ctx["transform"](xx, xy, yx, yy, tx, ty);
    },
    "rotate": function (angle) {
      const sin = Math.sin(angle);
      const cos = Math.cos(angle);
      this.transform(cos, sin, -sin, cos, 0, 0);
    },
    "_drawPath": function (path, paint) {
      const fillRule = path._fillRule === evenOdd ? "evenodd" : "nonzero";
      paint["draw"](this._ctx, path._path2D, fillRule);
    },
    "_drawImage": function (image, blend, opacity) {},
    "_getMatrix": function (out) {},
    "_drawImageMesh": function (
      image,
      blend,
      opacity,
      vtx,
      uv,
      indices,
      meshMinX,
      meshMinY,
      meshMaxX,
      meshMaxY
    ) {},
    "_clipPath": function (path) {
      const fillRule = path._fillRule === evenOdd ? "evenodd" : "nonzero";
      this._ctx["clip"](path._path2D, fillRule);
    },
    "clear": function () {
      this._ctx["clearRect"](
        0,
        0,
        this._canvas["width"],
        this._canvas["height"]
      );
    },
    "flush": function () {},
    "translate": function (x, y) {
      this.transform(1, 0, 0, 1, x, y);
    },
  }));

  Module.renderFactory = {
    makeRenderPaint: function () {
      return new CanvasRenderPaint();
    },
    makeRenderPath: function () {
      return new CanvasRenderPath();
    },
    makeRenderImage: function () {
      return null; //new CanvasRenderImage();
    },
    makeRenderer: function (canvasIDPtr, canvasIDLength) {
      const utf8 = new Uint8Array(canvasIDLength);
      utf8.set(
        Module.HEAPU8.subarray(canvasIDPtr, canvasIDPtr + canvasIDLength)
      );
      idString = new TextDecoder().decode(utf8);
      return new CanvasRenderer(document.getElementById(idString));
    },
  };

  let load = Module["load"];
  let loadContext = null;
  Module["load"] = function (bytes) {
    return new Promise(function (resolve, reject) {
      let result = null;
      loadContext = {
        total: 0,
        loaded: 0,
        ready: function () {
          resolve(result);
        },
      };
      result = load(bytes);
      if (loadContext.total == 0) {
        resolve(result);
      }
    });
  };
}
