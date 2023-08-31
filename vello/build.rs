use std::{
    env,
    ffi::OsString,
    path::{Path, PathBuf},
    process::Command,
};

use walkdir::WalkDir;

const CHECKOUT_DIRECTORY: &str = "target";

struct Checkout {
    path: PathBuf,
}

impl Checkout {
    pub fn new(repo: &str, tag: &str) -> Self {
        let name = repo.rsplit_once('/').expect("URL format invalid").1;
        let mut path = PathBuf::from(CHECKOUT_DIRECTORY);

        path.push(name);

        if !path.is_dir() {
            Command::new("git")
                .args([
                    "clone",
                    "-b",
                    tag,
                    repo,
                    &path.as_os_str().to_string_lossy(),
                ])
                .output()
                .unwrap_or_else(|_| panic!("failed to clone {}; is git CLI available?", name));
        }

        Self { path }
    }

    pub fn join<P: AsRef<Path>>(&self, path: P) -> PathBuf {
        self.path.join(path)
    }
}

fn all_files_with_extension<P: AsRef<Path>>(
    path: P,
    extension: &str,
) -> impl Iterator<Item = PathBuf> + '_ {
    WalkDir::new(path).into_iter().filter_map(move |entry| {
        entry
            .ok()
            .map(|entry| entry.into_path())
            .filter(|path| path.extension() == Some(&OsString::from(extension)))
    })
}

fn main() {
    println!("cargo:rerun-if-changed=src/vello_renderer.hpp");
    println!("cargo:rerun-if-changed=src/vello_renderer.cpp");
    println!("cargo:rerun-if-changed=src/winit_viewer.cpp");

    let harfbuzz = Checkout::new("https://github.com/harfbuzz/harfbuzz", "8.1.1");
    let sheen_bidi = Checkout::new("https://github.com/Tehreer/SheenBidi", "v2.6");

    let target = env::var("TARGET").unwrap();
    let profile = env::var("PROFILE").unwrap();

    let mut cfg = cc::Build::new();
    cfg.cpp(true)
        .flag_if_supported("-std=c++11") // for unix
        .warnings(false)
        .file(harfbuzz.join("src/harfbuzz.cc"));

    if !target.contains("windows") {
        cfg.define("HAVE_PTHREAD", "1");
    }

    if target.contains("apple") && profile.contains("release") {
        cfg.define("HAVE_CORETEXT", "1");
    }

    if target.contains("windows") {
        cfg.define("HAVE_DIRECTWRITE", "1");
    }

    if target.contains("windows-gnu") {
        cfg.flag("-Wa,-mbig-obj");
    }

    cfg.compile("harfbuzz");

    cc::Build::new()
        .files(all_files_with_extension(sheen_bidi.join("Source"), "c"))
        .include(sheen_bidi.join("Headers"))
        .warnings(false)
        .compile("sheenbidi");

    cc::Build::new()
        .cpp(true)
        .files(all_files_with_extension("../src", "cpp"))
        .files(all_files_with_extension(
            "../viewer/src/viewer_content",
            "cpp",
        ))
        .file("src/vello_renderer.cpp")
        .file("src/winit_viewer.cpp")
        .include("src")
        .include("../include")
        .include("../viewer/include")
        .include(harfbuzz.join("src"))
        .include(sheen_bidi.join("Headers"))
        .flag("-std=c++14")
        .warnings(false)
        .define("RIVE_SKIP_IMGUI", None)
        .define("WITH_RIVE_TEXT", None)
        .compile("rive");
}
