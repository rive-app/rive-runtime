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
}
