Lightweight Rive runtime for the Web using WASM and Canvas2D.

# Getting Started

## Browser
To use the library, run `npm install rive-canvas` and then simply include it:

    <script src="/node_modules/rive-canvas/rive.js"></script>
    Rive({
        locateFile: (file) => '/node_modules/rive-canvas/'+file,
    }).then((module) => {
        // Code goes here using Rive.
    });

As with all npm packages, there's a freely available CDN via unpkg.com:

    <script src="https://unpkg.com/rive-canvas@0.0.2/rive.js"></script>
    Rive({
         locateFile: (file) => 'https://unpkg.com/rive-canvas@0.0.2/'+file,
    }).then(...)