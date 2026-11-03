// Hand written. Builds with threads link it with --pre-js. Where a page
// cannot share memory such a build waits forever for its thread pool, so it
// fails at once with the reason, and the host loads its build without
// threads instead.
if (!ENVIRONMENT_IS_PTHREAD) {
  const riveThreadsRefusal =
    globalThis["crossOriginIsolated"] === false
      ? "the page is not cross origin isolated"
      : typeof SharedArrayBuffer === "undefined"
        ? "the page has no SharedArrayBuffer"
        : null;
  if (riveThreadsRefusal !== null) {
    throw new Error(
      "this build runs threads, which need shared memory, and " +
        riveThreadsRefusal +
        "; load the build without threads"
    );
  }

  // emcc's abort leaves the pool's workers running, so a build that fails to
  // start, and gives way to the build without threads, ends them here.
  const riveThreadsOnAbort = Module["onAbort"];
  Module["onAbort"] = (what) => {
    if (!runtimeInitialized) {
      PThread.terminateAllThreads();
    }
    riveThreadsOnAbort?.(what);
  };

  // Safari 26.2 to 26.3.1 keep a stale heap size on threads that did not grow
  // it, so their copies into new pages trap. Only WebKit needs the check.
  const riveUserAgent = globalThis.navigator?.userAgent ?? "";
  if (/AppleWebKit\//.test(riveUserAgent) && !/Chrome\//.test(riveUserAgent)) {
    // Asked now, so the answer overlaps the download.
    const riveMissesHeapGrowth = riveThreadsMissHeapGrowth();
    Module["preRun"] = [].concat(Module["preRun"] || [], () => {
      addRunDependency("riveHeapGrowth");
      // A worker that never answers must not hold the load back.
      const timeout = new Promise((resolve) =>
        setTimeout(resolve, 1000, false)
      );
      Promise.race([riveMissesHeapGrowth, timeout]).then((misses) => {
        if (misses) {
          // abort throws once it has failed the load.
          try {
            abort(
              "this build runs threads, and this browser loses heap growth " +
                "between them (WebKit bug 303387); load the build without " +
                "threads"
            );
          } catch {}
          return;
        }
        removeRunDependency("riveHeapGrowth");
      });
    });
  }
}

function riveThreadsMissHeapGrowth() {
  // A module that reads the size of the shared memory it imports.
  const probe = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60,
    0x00, 0x01, 0x7f, 0x02, 0x09, 0x01, 0x01, 0x6d, 0x01, 0x6d, 0x02, 0x03,
    0x01, 0x02, 0x03, 0x02, 0x01, 0x00, 0x07, 0x05, 0x01, 0x01, 0x73, 0x00,
    0x00, 0x0a, 0x06, 0x01, 0x04, 0x00, 0x3f, 0x00, 0x0b,
  ]);
  return new Promise((resolve) => {
    let worker = null;
    function done(misses) {
      worker?.terminate();
      resolve(misses);
    }
    try {
      const memory = new WebAssembly.Memory({
        "initial": 1,
        "maximum": 2,
        "shared": true,
      });
      worker = new Worker(
        URL.createObjectURL(
          new Blob([
            "onmessage=(e)=>{const i=new WebAssembly.Instance(" +
              "new WebAssembly.Module(e.data[0]),{m:{m:e.data[1]}});" +
              "postMessage(0);onmessage=()=>postMessage(i.exports.s())}",
          ])
        )
      );
      worker.onerror = () => done(false);
      worker.onmessage = () => {
        memory.grow(1);
        worker.onmessage = (event) => done(event.data < 2);
        worker.postMessage(0);
      };
      worker.postMessage([probe, memory]);
    } catch {
      done(false);
    }
  });
}
