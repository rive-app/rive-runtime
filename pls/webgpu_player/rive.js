const RIVE_PLS_TYPE_NONE = 0;
const RIVE_PLS_TYPE_PIXEL_LOCAL_STORAGE = 1;
const RIVE_PLS_TYPE_SUBPASS_LOAD = 2;

const RIVE_LOAD_ACTION_CLEAR = 0;
const RIVE_LOAD_ACTION_PRESERVE = 1;

const RIVE_FIT_FILL = 0;
const RIVE_FIT_CONTAIN = 1;
const RIVE_FIT_COVER = 2;
const RIVE_FIT_FIT_WIDTH = 3;
const RIVE_FIT_FIT_HEIGHT = 4;
const RIVE_FIT_NONE = 5;
const RIVE_FIT_SCALE_DOWN = 6;

const RIVE_ALIGNMENT_TOP_LEFT = [-1.0, -1.0];
const RIVE_ALIGNMENT_TOP_CENTER = [0.0, -1.0];
const RIVE_ALIGNMENT_TOP_RIGHT = [1.0, -1.0];
const RIVE_ALIGNMENT_CENTER_LEFT = [-1.0, 0.0];
const RIVE_ALIGNMENT_CENTER = [0.0, 0.0];
const RIVE_ALIGNMENT_CENTER_RIGHT = [1.0, 0.0];
const RIVE_ALIGNMENT_BOTTOM_LEFT = [-1.0, 1.0];
const RIVE_ALIGNMENT_BOTTOM_CENTER = [0.0, 1.0];
const RIVE_ALIGNMENT_BOTTOM_RIGHT = [1.0, 1.0];

function RiveInitialize(device, queue, canvasWidth, canvasHeight, invertedY, pixelLocalStorageType)
{
    Module._RiveInitialize(JsValStore.add(device),
                           JsValStore.add(queue),
                           canvasWidth,
                           canvasHeight,
                           invertedY,
                           pixelLocalStorageType);
}

function RiveLoadFile(bytes)
{
    const wasmPtr = Module._malloc(bytes.length);
    Module.HEAP8.subarray(wasmPtr, wasmPtr + bytes.length).set(bytes);
    const nativePtr = Module._RiveLoadFile(wasmPtr, bytes.length);
    Module._free(wasmPtr);
    return nativePtr ? new File(nativePtr) : null;
}

function RiveBeginRendering(targetTextureView, loadAction, clearColor)
{
    return new Renderer(Module._RiveBeginRendering(JsValStore.add(targetTextureView),
                                                   loadAction,
                                                   clearColor));
}

function RiveFlushRendering()
{
    Module._RiveFlushRendering();
}

function File(nativePtr)
{
    this.nativePtr = nativePtr;
    this.artboardNamed = (name) => {
        const artboardPtr = Module.ccall('File_artboardNamed',
                                         'number',
                                         ['number', 'string'],
                                         [this.nativePtr, name]);
        return artboardPtr ? new ArtboardInstance(artboardPtr) : null;
    };
    this.artboardDefault = () => {
        const artboardPtr = Module._File_artboardDefault(this.nativePtr);
        return artboardPtr ? new ArtboardInstance(artboardPtr) : null;
    };
    this.destroy = () => Module._File_destroy(this.nativePtr);
}

function ArtboardInstance(nativePtr)
{
    this.nativePtr = nativePtr;
    this.width = () => Module._ArtboardInstance_width(this.nativePtr)
    this.height = () => Module._ArtboardInstance_height(this.nativePtr)
    this.stateMachineNamed = function(name) {
        const stateMachinePtr = Module.ccall('ArtboardInstance_stateMachineNamed',
                                             'number',
                                             ['number', 'string'],
                                             [this.nativePtr, name]);
        return stateMachinePtr ? new StateMachineInstance(stateMachinePtr) : null;
    }
    this.animationNamed = function(name) {
        const animationPtr = Module.ccall('ArtboardInstance_animationNamed',
                                          'number',
                                          ['number', 'string'],
                                          [this.nativePtr, name]);
        return animationPtr ? new LinearAnimationInstance(animationPtr) : null;
    }
    this.defaultStateMachine = function() {
        const stateMachinePtr = Module._ArtboardInstance_defaultStateMachine(this.nativePtr);
        return stateMachinePtr ? new StateMachineInstance(stateMachinePtr) : null;
    }
    this.align = function(renderer, frameBounds, fit, alignment) {
        Module._ArtboardInstance_align(this.nativePtr,
                                       renderer.nativePtr,
                                       frameBounds[0],
                                       frameBounds[1],
                                       frameBounds[2],
                                       frameBounds[3],
                                       fit,
                                       alignment[0],
                                       alignment[1]);
    };
    this.destroy = () => Module._ArtboardInstance_destroy(this.nativePtr);
}

function StateMachineInstance(nativePtr)
{
    this.nativePtr = nativePtr;
    this.setBool = (inputName, value) =>
        Module.ccall('StateMachineInstance_setBool', null,
                     ['number', 'string', 'number'],
                     [this.nativePtr, inputName, value]);
    this.setNumber = (inputName, value) =>
        Module.ccall('StateMachineInstance_setNumber', null,
                     ['number', 'string', 'number'],
                     [this.nativePtr, inputName, value]);
    this.fireTrigger = (inputName) =>
        Module.ccall('StateMachineInstance_fireTrigger', null,
                     ['number', 'string'],
                     [this.nativePtr, inputName]);
    this.pointerDown = (x, y) =>
        Module._StateMachineInstance_pointerDown(this.nativePtr, x, y);
    this.pointerMove = (x, y) =>
        Module._StateMachineInstance_pointerMove(this.nativePtr, x, y);
    this.pointerUp = (x, y) =>
        Module._StateMachineInstance_pointerUp(this.nativePtr, x, y);
    this.advanceAndApply = (elapsed) =>
        Module._StateMachineInstance_advanceAndApply(this.nativePtr, elapsed);
    this.draw = (renderer) => Module._StateMachineInstance_draw(this.nativePtr, renderer.nativePtr);
    this.destroy = () => Module._StateMachineInstance_destroy(this.nativePtr);
}

function LinearAnimationInstance(nativePtr)
{
    this.nativePtr = nativePtr;
    this.advanceAndApply = (elapsed) =>
        Module._LinearAnimationInstance_advanceAndApply(this.nativePtr, elapsed);
    this.draw = (renderer) =>
        Module._LinearAnimationInstance_draw(this.nativePtr, renderer.nativePtr);
    this.destroy = () => Module._LinearAnimationInstance_destroy(this.nativePtr);
}

function Renderer(nativePtr)
{
    this.nativePtr = nativePtr;
    this.save = () => Module._Renderer_save(this.nativePtr);
    this.restore = () => Module._Renderer_restore(this.nativePtr);
    this.translate = (x, y) => Module._Renderer_translate(this.nativePtr, x, y);
    this.transform = (xx, xy, yx, yy, tx, ty) =>
        Module._Renderer_transform(this.nativePtr, xx, xy, yx, yy, tx, ty);
}
