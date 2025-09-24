#!/usr/bin/python

import argparse
import atexit
import base64
import glob
import http.server
import os
import pathlib
import platform
import queue
import re
import shlex
import shutil
import signal
import socket
import socketserver
import subprocess
import sys
import threading
import time
import urllib.request
import zipfile

HANDSHAKE_TOKEN = 0xfee1600d
SHUTDOWN_TOKEN = 0xfee1dead

parser = argparse.ArgumentParser(description="Run native gms & goldens, and dump their .pngs")
parser.add_argument("tools",
                    type=str,
                    nargs="+",
                    choices=["gms", "goldens", "player"],
                    help="which tool(s) to run")
parser.add_argument("-B", "--builddir",
                    type=str,
                    default=os.getenv("RIVE_BUILDDIR"),
                    help="output directory from build")
parser.add_argument("-b", "--backend",
                    type=str,
                    default=None)
parser.add_argument("-s", "--src",
                    type=str,
                    default=os.path.join("..", "..", "..", "zzzgold", "rivs"),
                    help="INPUT directory of .riv files to render")
parser.add_argument("-o", "--outdir",
                    type=str,
                    default=os.path.join(".gold", "candidates"),
                    help="base directory to output the PNG directory structure")
parser.add_argument("-p", "--png_threads",
                    type=int,
                    default=4,
                    help="Number of pngs encoding threads on each tool process.")
parser.add_argument("-j", "--jobs-per-tool",
                    type=int,
                    default=4,
                    help="number of processes to spawn for each tool in 'args.tools' "\
                         "(non-mobile only; android/ios only get one job)")
parser.add_argument("--rows",
                    type=int,
                    default=1,
                    help="number of rows in the goldens grid")
parser.add_argument("--cols",
                    type=int,
                    default=1,
                    help="number of columns in the goldens grid")
parser.add_argument("-m", "--match",
                    type=str,
                    default=None,
                    help="`match` patter for gms")
parser.add_argument("-t", "--target",
                    default="host",
                    choices=["host", "android", "ios", "iossim", "unreal",
                             "unreal_android", "webbrowser", "webserver",
                             "webbrowserandroid", "webserverandroid"],
                    help="which platform to run on")
parser.add_argument("-a", "--android-arch",
                    default="arm64",
                    choices=["arm", "arm64"])
parser.add_argument("-u", "--ios_udid",
                    type=str,
                    default=None,
                    help="unique id of iOS device to run on (--target=ios or iossim)")
parser.add_argument("-c", "--webclient",
                    default=os.getenv("RIVE_WEBCLIENT"),
                    help="executable to launch when --target=webserver")
parser.add_argument("-k", "--options",
                    type=str,
                    default=None,
                    help="additional options to pass through (player only)")
parser.add_argument("-S", "--server_only",
                    action='store_true',
                    help="Start servers but don't launch gms or goldens tools")
parser.add_argument("-r", "--remote",
                    action='store_true',
                    help="target is remote; serve from host IP instead of localhost")
parser.add_argument("--build-only", action='store_true',
                    help="only build, don't deploy")
parser.add_argument("--no-rebuild", action='store_true',
                    help="don't rebuild the native tools in builddir")
parser.add_argument("-n", "--no-install", action='store_true',
                    help="don't package & reinstall the mobile app prior to launch")
parser.add_argument("-v", "--verbose", action='store_true', help="enable verbose output")

args = parser.parse_args()
skipped_golden_tests = set()
target_info = {} # dictionary for info about the target (ios_version, etc.)
rivsqueue = queue.Queue()

# Global pump on stdin.
input_lock = None
input_received_cond = None
input_chars = bytearray()
input_cancelled = False
def pump_input_thread():
    global input_chars
    while not input_cancelled:
        try:
            chars = sys.stdin.readline().encode("ascii")
            with input_lock:
                input_chars += chars
                input_received_cond.notify()
        except EOFError:
            return

def get_input():
    global input_lock, input_received_cond, input_chars
    if not input_lock:
        input_lock = threading.Lock()
        input_received_cond = threading.Condition(input_lock)
        threading.Thread(target=pump_input_thread, daemon=True).start()
    with input_lock:
        while not input_cancelled and len(input_chars) < 1:
            input_received_cond.wait()
        chars = input_chars
        input_chars = bytearray()
    return chars

def cancel_input():
    global input_cancelled
    with input_lock:
        input_cancelled = True
        input_received_cond.notify_all()

class text_colors:
    ERROR = '\033[91m'
    ENDCOL = '\033[0m'

# Launch a process in a separate thread and crash if it fails.
class CheckProcess(threading.Thread):
    def __init__(self, cmd, env=None):
        threading.Thread.__init__(self)
        self.cmd = cmd
        if args.server_only:
            self.cmd = ["echo", "\n    <command> "] + ['"%s"' % arg for arg in self.cmd]
        if args.verbose:
            print(' '.join(self.cmd), flush=True)
        if shutil.which(self.cmd[0]) is None:
            print(f'{text_colors.ERROR}' + self.cmd[0] + ' does not exist!' + f'{text_colors.ENDCOL}')
        else:
            self.proc = subprocess.Popen(self.cmd, env=env)
        self._did_terminate = False

    def run(self):
        self.proc.wait()
        if not self._did_terminate and self.proc.returncode != 0:
            os._exit(self.proc.returncode)

    def terminate(self):
        self._did_terminate = True
        self.proc.terminate()

def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(0)
    try:
        # doesn't even have to be reachable
        s.connect(("10.254.254.254", 1))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip

# Simple http server for web-based targets.
def start_http_server(directory, server_ip):
    import functools
    import http.server

    http_port_holder = []
    http_port_ready_event = threading.Event()

    class COOPHandler(http.server.SimpleHTTPRequestHandler):
        def end_headers(self):
            # Serve cross-origin isolated pages so SharedArrayBuffer is defined
            # for emscripten POSIX emulation.
            self.send_header("Cross-Origin-Opener-Policy", "same-origin")
            self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
            super().end_headers()

        def log_message(self, format, *args):
            # Suppress default HTTP logs like:
            # 127.0.0.1 - - [22/Aug/2025 13:37:00] "GET /index.html HTTP/1.1" 200 -
            pass

    handler = functools.partial(COOPHandler, directory=directory)

    def run_http_server():
        with socketserver.TCPServer((server_ip, 0), handler) as httpd:
            http_port_holder.append(httpd.server_address[1])
            http_port_ready_event.set()
            httpd.serve_forever()

    thread = threading.Thread(target=run_http_server, daemon=True)
    thread.start()

    http_port_ready_event.wait()  # wait until server is ready
    return (server_ip, http_port_holder[0])

# Simple websocket <-> TCP bridge for web-based targets.
def start_websocket_bridge(tcp_server_address):
    import asyncio
    import threading
    import socket
    import websockets

    websocket_port_holder = []
    port_ready_event = threading.Event()

    async def handle_websocket(websocket):
        reader, writer = await asyncio.open_connection(*tcp_server_address)

        async def tcp_to_ws():
            try:
                while True:
                    data = await reader.read(1024)
                    if not data:
                        break
                    await websocket.send(data)
            except:
                raise

        async def ws_to_tcp():
            try:
                async for message in websocket:
                    if isinstance(message, str):
                        # Text sent to our websocket is always base64.
                        message = base64.b64decode(message)
                    writer.write(message)
                    await writer.drain()
            except:
                raise

        await asyncio.gather(tcp_to_ws(), ws_to_tcp())
        writer.close()
        await writer.wait_closed()

    async def run_server(sock):
        async with websockets.serve(handle_websocket, sock=sock):
            await asyncio.Future()  # run forever

    def server_thread():
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('0.0.0.0', 0))  # OS assigns port
        sock.listen(5)
        sock.setblocking(False)

        websocket_port_holder.append(sock.getsockname()[1])
        port_ready_event.set()

        asyncio.run(run_server(sock))

    thread = threading.Thread(target=server_thread, daemon=True)
    thread.start()

    port_ready_event.wait()
    return (tcp_server_address[0], websocket_port_holder[0])

# Simple TCP server for Rive tools.
class ToolServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True

    def __init__(self, handler):
        if args.remote or args.target == "webserver":
            # The device needs to connect over the network instead of localhost.
            self.host = get_local_ip()
        else:
            self.host = "localhost" # Desktop and Android (with "adb reverse") can use local ports.
        address = (self.host, 0) # let the kernel give us a port
        self.shutdown_event = None
        self.claimed_gm_tests_lock = threading.Lock()
        # "Claim" the skipped tests upfront so they don't get drawn.
        self.claimed_gm_tests = set(skipped_golden_tests)
        super().__init__(address, handler)

    def server_activate(self):
        # Allow enough connections for each process we spawn.
        num_processes = len(args.tools) * args.jobs_per_tool
        # Each process establishes a maximum of:
        #   * 1 primary TestHarness server connection.
        #   * `png_threads` encoder connections.
        #   * 1 stdio-forwarding connection.
        #   * 1 input pump connection.
        threads_per_process = 1 + args.png_threads + 1 + 1
        self.socket.listen(num_processes * threads_per_process)

    def serve_forever_async(self):
        self.serve_thread = threading.Thread(target=self.serve_forever, daemon=True)
        self.serve_thread.start()
        print("TestHarness server running on %s:%u" % self.server_address, flush=True)

        if args.target.startswith("web"):
            self.server_address = start_websocket_bridge(self.server_address)
            print("TestHarness server bridged to WebSocket on %s:%u" % self.server_address,
                  flush=True)

            self.http_address = start_http_server(args.builddir,
                                                  self.server_address[0])
            print("HTTP server running from %s on %s:%u" % (args.builddir,
                                                            *self.http_address),
                  flush=True)

        if self.host == "localhost" and "android" in args.target:
            # Use ADB reverse port forwarding to make the TestHarness server
            # accessible from the device.
            server_port = self.server_address[1]
            subprocess.Popen(["adb", "reverse",
                              "tcp:%s" % server_port,
                              "tcp:%s" % server_port],
                             stdout=subprocess.DEVNULL)
            print("ADB reverse port forwarding established on port %u for TestHarness server" % server_port,
                  flush=True)

            if args.target.startswith("web"):
                # Use ADB reverse port forwarding to make the HTTP server
                # accessible from the device.
                http_port = self.http_address[1]
                subprocess.Popen(["adb", "reverse",
                                  "tcp:%s" % http_port,
                                  "tcp:%s" % http_port],
                                 stdout=subprocess.DEVNULL)
                print("ADB reverse port forwarding established on port %u for HTTP server" % http_port,
                      flush=True)


    # Simple utility to wait until a TCP client tells the server it has finished.
    def wait_for_shutdown_event(self, timeout=threading.TIMEOUT_MAX):
        if not self.shutdown_event:
            return True
        if timeout == threading.TIMEOUT_MAX:
            while not self.shutdown_event.wait(timeout=1):
                # Poll every second or Python doesn't receive ^C on windows.
                pass
        elif not self.shutdown_event.wait(timeout=timeout):
            return False
        self.shutdown_event = None
        return True

    def reset_shutdown_event(self):
        self.shutdown_event = threading.Event()

    def synthesize_shutdown_event(self):
        if self.shutdown_event:
            self.shutdown_event.set()

    # Only returns True the on the first request for a given name.
    # Prevents gms from running more than once in a multi-process execution.
    def claim_gm_test(self, name):
        with self.claimed_gm_tests_lock:
            if not name in self.claimed_gm_tests:
                self.claimed_gm_tests.add(name)
                return True
        return False


# RequestHandler with services for Rive tools.
class TestHarnessRequestHandler(socketserver.BaseRequestHandler):
    REQUEST_TYPE_IMAGE_UPLOAD = 0
    REQUEST_TYPE_CLAIM_GM_TEST = 1
    REQUEST_TYPE_FETCH_RIV_FILE = 2
    REQUEST_TYPE_GET_INPUT = 3
    REQUEST_TYPE_CANCEL_INPUT = 4
    REQUEST_TYPE_PRINT_MESSAGE = 5
    REQUEST_TYPE_DISCONNECT = 6
    REQUEST_TYPE_APPLICATION_CRASH = 7

    def recvall(self, byte_count):
        data = bytearray()
        while len(data) < byte_count:
            chunk = self.request.recv(min(byte_count - len(data), 4096))
            data.extend(chunk)
        return data

    def recv_string(self):
        length = int.from_bytes(self.recvall(4), byteorder="big")
        return self.recvall(length).decode("ascii")

    def handle(self):
        try:
            while True:
                # Receive the next request.
                requesttype = int.from_bytes(self.recvall(4), byteorder="big")
                if requesttype == self.REQUEST_TYPE_DISCONNECT:
                    shutdown = int.from_bytes(self.recvall(4), byteorder="big")
                    if self.server and shutdown:
                        if self.server.shutdown_event:
                            self.server.shutdown_event.set()
                    break

                elif requesttype == TestHarnessRequestHandler.REQUEST_TYPE_IMAGE_UPLOAD:
                    destination = os.path.join(args.outdir, self.recv_string())

                    with open(destination, "wb") as pngfile:
                        while True:
                            chunksize = int.from_bytes(self.recvall(4), byteorder="big")
                            if chunksize == HANDSHAKE_TOKEN:
                                break
                            pngfile.write(self.recvall(chunksize))

                    self.request.sendall(HANDSHAKE_TOKEN.to_bytes(4, byteorder="big"))

                    if args.verbose:
                        print("[server] Received %s" % destination, flush=True)

                elif requesttype == TestHarnessRequestHandler.REQUEST_TYPE_CLAIM_GM_TEST:
                    shouldrun = self.server.claim_gm_test(self.recv_string())
                    self.request.sendall(shouldrun.to_bytes(4, byteorder="big"))

                elif requesttype == TestHarnessRequestHandler.REQUEST_TYPE_FETCH_RIV_FILE:
                    remaining = rivsqueue.qsize()
                    if not args.verbose and remaining % 7 == 0:
                        print("[%3u] rivs remaining...\r" % remaining,
                              end='\r' if not args.verbose else '', flush=True)

                    try:
                        while True:
                            riv = rivsqueue.get_nowait()
                            riv_basename = os.path.basename(riv)
                            if not riv_basename in skipped_golden_tests:
                                break
                        if args.verbose:
                            print("[server] Sending %s..." % riv, end='\r', flush=True)
                        filename_ascii = riv_basename.encode("ascii")
                        self.request.sendall(len(filename_ascii).to_bytes(4, byteorder="big"))
                        self.request.sendall(filename_ascii)
                        with open(riv, "rb") as rivfile:
                            rivbytes = rivfile.read()
                            self.request.sendall(len(rivbytes).to_bytes(4, byteorder="big"))
                            self.request.sendall(rivbytes)

                    except queue.Empty:
                        # .rivs are finished. Tell the client to shutdown.
                        self.request.sendall(SHUTDOWN_TOKEN.to_bytes(4, byteorder="big"))

                elif requesttype == TestHarnessRequestHandler.REQUEST_TYPE_GET_INPUT:
                    chars = get_input()
                    if input_cancelled:
                        if args.verbose:
                            print("[server] shutting down input pump", flush=True)
                        self.request.sendall(SHUTDOWN_TOKEN.to_bytes(4, byteorder="big"))
                    else:
                        if args.verbose:
                            print("[server] sending ", chars, flush=True)
                        self.request.sendall(len(chars).to_bytes(4, byteorder="big"))
                        self.request.sendall(chars)

                elif requesttype == TestHarnessRequestHandler.REQUEST_TYPE_CANCEL_INPUT:
                    cancel_input()

                elif requesttype == TestHarnessRequestHandler.REQUEST_TYPE_PRINT_MESSAGE:
                    print(self.recv_string(), end="", flush=True)

                elif requesttype == TestHarnessRequestHandler.REQUEST_TYPE_APPLICATION_CRASH:
                    print("CRASH in tool: %s" % self.recv_string(), flush=True)
                    os._exit(-1)

        except ConnectionResetError:
            print("TestHarness connection reset by client tool", flush=True)
            os._exit(-1)


# If we aren't deploying to the host, update the given command to deploy on its intended target.
def update_cmd_to_deploy_on_target(cmd, test_harness_server, env):
    dirname = os.path.dirname(cmd[0])
    toolname = os.path.basename(cmd[0])

    if args.target == "unreal":
        unreal_exe_path = os.path.join(dirname, "Windows", "rive_unreal.exe")
        return [unreal_exe_path, "/Game/maps/" + toolname, "-ResX=1280", "-ResY=720", "-WINDOWED"] + cmd[1:]

    if args.target == "unreal_android":
        tool_args = ' '.join(["/Game/maps/" + toolname] + cmd[1:])
        return ["adb", "shell",
                    "am force-stop app.rive.rive_unreal && "
                    f"am start -n app.rive.rive_unreal/com.epicgames.unreal.GameActivity -e args '{tool_args}'"]

    if args.target == "android":
        sharedlib = os.path.join(dirname, "lib%s.so" % toolname)
        print("\nDeploying %s on android..." % sharedlib)
        tool_args = ' '.join([sharedlib] + cmd[1:])
        return ["adb", "shell",
                "am force-stop app.rive.android_tests && "
                "am start -n app.rive.android_tests/.%s -e args '%s'" % (toolname, tool_args)]

    elif args.target == "ios":
        print("\nDeploying %s on ios (udid=%s, ios_version=%i)..." %
              (toolname, args.ios_udid, target_info["ios_version"]))
        cmd = [toolname] + cmd[1:]
        if target_info["ios_version"] >= 17:
            # ios-deploy is no longer supported after iOS 17.
            return ["xcrun", "devicectl", "device", "process", "launch",
                    # "--console",  # TODO: "--console" not currently supported.
                    "--device", args.ios_udid,
                    "--environment-variables", '{"MTL_DEBUG_LAYER": "1"}',
                    "rive.app.golden-test-app"] + cmd
        else:
            return ["ios-deploy", "--noinstall", "--noninteractive", "--bundle",
                    "ios_tests/build/Debug-iphoneos/rive_ios_tests.app",
                    "--envs", "MTL_DEBUG_LAYER=1",
                    "--args", ' '.join(cmd)]

    elif args.target == "iossim":
        print("\nDeploying %s on ios simulator (udid=%s)..." % (toolname, args.ios_udid))
        cmd = [toolname] + cmd[1:]
        return ["xcrun", "simctl", "launch", args.ios_udid, "rive.app.golden-test-app"] + cmd

    elif args.target.startswith("web"):
        env["RIVE_HTTP_SERVER_DIR"] = str(pathlib.Path(args.builddir).resolve())
        if args.target == "webbrowser":
            client = ["python3", "-m", "webbrowser", "-t"]
        elif args.target == "webbrowserandroid":
            client = ["adb", "shell", "am", "start", "-a",
                      "android.intent.action.VIEW", "-d"]
        elif args.webclient:
            client = shlex.split(args.webclient)
        else:
            client = ["echo", "\nPlease navigate your web client to:\n\n"]
        return client + ["http://%s:%u/%s.html#%s" % (*test_harness_server.http_address,
                                                      toolname,
                                                      '%20'.join(cmd[1:]))]
    else:
        assert(args.target == "host")
        return cmd

def launch_gms(test_harness_server):
    tool = os.path.join(args.builddir, "gms")
    if platform.system() == "Windows" and args.target  == 'host':
        tool = tool + ".exe"
    env = os.environ.copy()
    cmd = [tool,
           "--backend", args.backend,
           "--test_harness", "%s:%u" % test_harness_server.server_address]
    if not args.target.startswith("web"):
        cmd = cmd + ["--headless",
                     "-p%i" % args.png_threads];
    if args.match:
        cmd = cmd + ["--match", args.match];
    if args.verbose:
        cmd = cmd + ["--verbose"];
    cmd = update_cmd_to_deploy_on_target(cmd, test_harness_server, env)

    procs = [CheckProcess(cmd, env) for i in range(0, args.jobs_per_tool)]
    for proc in procs:
        proc.start()

    return procs


def launch_goldens(test_harness_server):
    tool = os.path.join(args.builddir, "goldens")
    if platform.system() == "Windows" and args.target  == 'host':
        tool = tool + ".exe"
    env = os.environ.copy()
    if args.verbose:
        print("[server] Using '" + tool + "'", flush=True)

    if not os.path.exists(args.src):
        print("Can't find rivspath " + args.src, flush=True)
        return -1;

    if os.path.isdir(args.src):
        for riv in glob.iglob(os.path.join(args.src, "*.riv")):
            rivsqueue.put(riv)
    else:
        rivsqueue.put(args.src)

    cmd = [tool,
           "--test_harness", "%s:%u" % test_harness_server.server_address,
           "--backend", args.backend,
           "--rows", str(args.rows),
           "--cols", str(args.cols)]
    if not args.target.startswith("web"):
        cmd = cmd + ["--headless",
                     "-p%i" % args.png_threads];
    if args.verbose:
        cmd = cmd + ["--verbose"];
    cmd = update_cmd_to_deploy_on_target(cmd, test_harness_server, env)

    procs = [CheckProcess(cmd, env) for i in range(0, args.jobs_per_tool)]
    for proc in procs:
        proc.start()

    return procs

def launch_player(test_harness_server):
    if not os.path.exists(args.src):
        print("Can't find riv path " + args.src, flush=True)
        return -1;

    tool = os.path.join(args.builddir, "player")
    if platform.system() == "Windows" and args.target  == 'host':
        tool = tool + ".exe"
    env = os.environ.copy()
    cmd = [tool,
           "--test_harness", "%s:%u" % test_harness_server.server_address,
           "--backend", args.backend]
    if args.options:
        cmd += ["--options", args.options]
    cmd = update_cmd_to_deploy_on_target(cmd, test_harness_server, env)

    rivsqueue.put(args.src)
    player = CheckProcess(cmd, env)
    player.start()
    return player

def force_stop_android_tests_apk():
    subprocess.check_call(["adb", "shell", "am force-stop app.rive.android_tests"])

def main():
    # Parse skipped tests.
    rive_skipped_golden_tests = os.getenv("RIVE_SKIPPED_GOLDEN_TESTS")
    if rive_skipped_golden_tests:
        for test in rive_skipped_golden_tests.split(","):
            if '/' in test:
                # A "/" character separates a specific backend from a test name.
                # Only skip one of these tests if it's the same backend we're currently running.
                backend, test = test.split("/", 1)
                if backend != args.backend:
                    continue
            skipped_golden_tests.add(test)
        if args.verbose:
            print("[server] skipped_golden_tests: ", skipped_golden_tests, flush=True)

    if args.target == "android":
        args.jobs_per_tool = 1 # Android can only launch one process at a time.
        if args.builddir == None:
            args.builddir = f"out/android_{args.android_arch}_debug"
        if args.backend == None:
            args.backend = "gl"
    elif args.target == "ios":
        if args.builddir == None:
            args.builddir = "out/ios_debug"
        elif args.builddir != "out/ios_debug":
            print("The iOS wrapper app requires --builddir=out/ios_debug")
            return -1
        if args.backend == None:
            args.backend = "metal"
        args.jobs_per_tool = 1 # iOS can only launch one process at a time.
        args.remote = True # Since we can't do port forwarding in iOS, it always has to be remote.
        if not args.ios_udid:
            args.ios_udid = subprocess.check_output(["idevice_id", "-l"]).decode().strip()
        device_info = os.popen("xcrun xctrace list devices | grep %s" % args.ios_udid).read()
        target_info["ios_version"] = int(re.search(r".+\(([0-9]+)\.[0-9\.]+\).+$",
                                                   device_info).group(1))
    elif args.target == "iossim":
        if args.builddir == None:
            args.builddir = "out/iossim_universal_debug"
        elif args.builddir != "out/iossim_universal_debug":
            print("The iOS-simulator wrapper app requires --builddir=out/iossim_universal_debug")
            return -1
        if args.backend == None:
            args.backend = "metal"
        args.jobs_per_tool = 1 # iOS can only launch one process at a time.
        args.remote = True # Since we can't do port forwarding in iOS, it always has to be remote.
        if not args.ios_udid:
            args.ios_udid = "booted"
    elif args.target == 'unreal' or args.target == 'unreal_android':
         # currently, unreal needs to run only one job at a time for goldens and gms to work
        args.jobs_per_tool = 1
        if args.builddir == None:
            args.builddir = os.path.join("out", "debug")
        # unreal is currently always rhi, we may have seperate rhi types in the future like rhi_metal etc..
        args.backend = 'rhi'
    elif args.target.startswith("web"):
        args.jobs_per_tool = 1
        if args.builddir == None:
            args.builddir = "out/wasm_debug"
        if args.backend == None:
            args.backend = "gl"
    else:
        assert(args.target == "host")
        if args.builddir == None:
            args.builddir = os.path.join("out", "debug")
        if args.backend == None:
            args.backend = "metal" if platform.system() == "Darwin" else \
                           "d3d" if platform.system() == "Windows" else \
                           "gl"

    if "metal" in args.backend:
        # Turn on Metal validation layers.
        # NOTE: MoltenVK generates Metal validation errors right now, so only them on for our own
        # Metal backends.
        os.environ["MTL_DEBUG_LAYER"] = "1"

    if args.server_only:
        args.jobs_per_tool = 1 # Only print the command for each job once.

    rive_tools_dir = os.path.dirname(os.path.realpath(__file__))
    if "ios" in args.target:
        # ios links statically, so we need to build every tool every time.
        build_targets = ["gms", "goldens", "player"]
    else:
        build_targets = args.tools

    # Build the native code.
    if not args.no_rebuild and not args.no_install:
        build_rive = [os.path.join(rive_tools_dir, "../build/build_rive.sh")]
        if os.name == "nt":
            if subprocess.run(["where", "msbuild.exe"]).returncode == 0:
                # msbuild.exe is already on the $PATH; launch build_rive.sh directly.
                build_rive = ["sh"] + build_rive
            else:
                # msbuild.exe is not on the path; go through build_rive.bat.
                build_rive[0] = os.path.splitext(build_rive[0])[0] + '.bat'
        subprocess.check_call(build_rive + ["rebuild", args.builddir] + build_targets)

    # Build the wrapper app, if applicable
    if not args.no_install:
        if args.target == "unreal_android":
            unreal_android_path = os.path.join(args.builddir, "Android_ASTC")
            current = os.getcwd()
            os.chdir(unreal_android_path);
            subprocess.check_call(["Install_rive_unreal-arm64.bat"])
            print()
            os.chdir(current);
        if args.target == "android":
            # Copy the native libraries into the android_tests project.
            jnidir = os.path.join("android_tests", "app", "src", "main", "jniLibs")
            shutil.rmtree(jnidir, ignore_errors=True)
            if args.android_arch == "arm64":
                arch_full_name = "arm64-v8a"
            else:
                assert(args.android_arch == "arm")
                arch_full_name = "armeabi-v7a"
            os.makedirs(os.path.join(jnidir, arch_full_name))
            for tool in build_targets:
                sharedlib = "lib%s.so" % tool
                shutil.copy(os.path.join(args.builddir, sharedlib),
                            os.path.join(jnidir, arch_full_name))
            if "vk" in args.backend or "vulkan" in args.backend:
                layerpath = os.path.join("dependencies", "Vulkan-ValidationLayers")
                if not os.path.exists(layerpath):
                    # Download the Vulkan validation layers.
                    print("Downloading Android Vulkan validation layers...", flush=True)
                    url = "https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/"\
                          "vulkan-sdk-1.3.290.0/android-binaries-1.3.290.0.zip"
                    zipfile.ZipFile(urllib.request.urlretrieve(url)[0], 'r').extractall(path=layerpath)
                # Bundle the Vulkan validation layers.
                for lib in glob.glob(os.path.join(layerpath,
                                                  "android-binaries-1.3.290.0",
                                                  arch_full_name,
                                                  "*.so")):
                    dst = os.path.join(jnidir, arch_full_name, os.path.basename(lib))
                    print("  bundling %s -> %s" % (lib, dst), flush=True)
                    shutil.copy(lib, dst)

            # Build the android_tests wrapper app.
            cwd = os.getcwd()
            os.chdir(os.path.join(rive_tools_dir, "android_tests"))

            # Check for Java availability
            if (os.getenv("JAVA_HOME") is None and
                subprocess.run(["java", "-version"], stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL).returncode != 0):
                print("Error: Java is required to run Android Gradle tests. Please ensure the $JAVA_HOME environment variable is set. You may use Android Studio's JDK (the path can be found at Settings > Build, Execution, Deployment > Build Tools > Gradle > Gradle JDK).")
                return -1

            # Check for $ANDROID_HOME
            if not os.getenv("ANDROID_HOME"):
                print("Error: $ANDROID_HOME is not set. Please set it to the path of your Android SDK. You may use Android Studio's SDK (the path can be found at Settings > Languages & Frameworks > Android SDK > Android SDK Location).")
                return -1

            # Call gradlew to build the android_tests wrapper app.
            subprocess.check_call(["./gradlew" if os.name != "nt" else "gradlew.bat",
                                   ":app:assembleDebug"])
            os.chdir(cwd)
        elif args.target == "ios":
            # Build the ios_tests wrapper app.
            subprocess.check_call(["xcodebuild",
                                   "-destination", "generic/platform=iOS",
                                   "-config", "Debug",
                                   "build", "-project", "ios_tests/ios_tests.xcodeproj"])
        elif args.target == "iossim":
            # Build the ios_tests wrapper app for the simulator.
            subprocess.check_call(["xcodebuild",
                                   "-destination", "generic/platform=iOS Simulator",
                                   "-config", "Debug",
                                   "-sdk", "iphonesimulator",
                                   "build", "-project", "ios_tests/ios_tests.xcodeproj"])

    if args.build_only:
        return 0

    # Install the wrapper app, if applicable.
    if not args.no_install:
        if args.target == "android":
            # Install the android_tests wrapper app.
            force_stop_android_tests_apk()
            subprocess.check_call(["adb", "install", "-r", "android_tests/app/build/outputs/apk/debug/app-debug.apk"])
            print()
        elif args.target == "ios":
            # Install the ios_tests wrapper app on the device.
            if target_info["ios_version"] >= 17:
                # ios-deploy is no longer supported after iOS 17.
                subprocess.check_call(["xcrun", "devicectl", "device", "install", "app", "--device",
                                       args.ios_udid,
                                       "ios_tests/build/Debug-iphoneos/rive_ios_tests.app"])
            else:
                subprocess.check_call(["ios-deploy", "--bundle",
                                       "ios_tests/build/Debug-iphoneos/rive_ios_tests.app"])
            print()
        elif args.target == "iossim":
            # Install the ios_tests wrapper app on the simulator.
            subprocess.check_call(["xcrun", "simctl", "install", args.ios_udid,
                                   "ios_tests/build/Debug-iphonesimulator/rive_ios_tests.app"])
            print()

    if args.target == "android":
        atexit.register(force_stop_android_tests_apk)

    with (ToolServer(TestHarnessRequestHandler) as test_harness_server):
        test_harness_server.serve_forever_async()

        # On many targets we can't launch >1 instance of the app at a time.
        serial_deploy = not args.server_only and ("ios" in args.target or
                                                  args.target == "android" or
                                                  "unreal" in args.target or
                                                  args.target.startswith("web"))
        procs = []

        # Serially deployed targets are finished once they've sent their
        # shutdown event. If, 1.5 seconds after sending the shutdown event, they
        # still haven't exited, terminate them.
        SERIAL_TARGET_TERMINATE_SECONDS = 1.5

        def keyboard_interrupt_handler(signal, frame):
            if os.name == "nt":
                print("^C", end="", flush=True)
            cancel_input()
            for proc in procs:
                proc.terminate()
            if not test_harness_server.wait_for_shutdown_event(1.5):
                test_harness_server.synthesize_shutdown_event()
        signal.signal(signal.SIGINT, keyboard_interrupt_handler)

        if "gms" in args.tools:
            os.makedirs(args.outdir, exist_ok=True)
            if serial_deploy:
                test_harness_server.reset_shutdown_event()
                procs = launch_gms(test_harness_server)
                assert(len(procs) == 1)
                test_harness_server.wait_for_shutdown_event()
                procs[0].join(SERIAL_TARGET_TERMINATE_SECONDS)
                if procs[0].is_alive():
                    procs[0].terminate()
                    procs[0].join()
                procs = []
                if args.target == "android":
                    force_stop_android_tests_apk()
                    time.sleep(3)
            else:
                procs += launch_gms(test_harness_server)

        if "goldens" in args.tools:
            os.makedirs(args.outdir, exist_ok=True)
            if serial_deploy:
                test_harness_server.reset_shutdown_event()
                procs = launch_goldens(test_harness_server)
                assert(len(procs) == 1)
                test_harness_server.wait_for_shutdown_event()
                procs[0].join(SERIAL_TARGET_TERMINATE_SECONDS)
                if procs[0].is_alive():
                    procs[0].terminate()
                    procs[0].join()
                procs = []
                if args.target == "android":
                    force_stop_android_tests_apk()
                    time.sleep(3)
            else:
                procs += launch_goldens(test_harness_server)

        if "player" in args.tools:
            test_harness_server.reset_shutdown_event()
            procs = [launch_player(test_harness_server)]
            test_harness_server.wait_for_shutdown_event()
            procs[0].join(SERIAL_TARGET_TERMINATE_SECONDS)
            if procs[0].is_alive():
                procs[0].terminate()
                procs[0].join()
            procs = []

        # Wait for the processes to finish (if not in serial_deploy mode).
        for proc in procs:
            proc.join()

        if args.server_only:
            # Sleep until user input.
            input("\nPress enter to shutdown...")

        print("done                          ", flush=True)

        test_harness_server.shutdown()

    return 0

if __name__ == "__main__":
    sys.exit(main())
