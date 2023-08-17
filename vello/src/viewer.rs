use std::{
    ffi::CString, fs::File, io::Read, os::unix::prelude::OsStrExt, path::Path, ptr::NonNull,
};

use vello::kurbo::Vec2;

use crate::VelloRenderer;

#[derive(Debug)]
enum RawViewerContent {}

extern "C" {
    fn viewer_content_new(raw_path: *const i8) -> Option<NonNull<RawViewerContent>>;
    fn viewer_content_release(raw_viewer_content: Option<NonNull<RawViewerContent>>);
    fn viewer_content_handle_resize(
        raw_viewer_content: Option<NonNull<RawViewerContent>>,
        width: i32,
        height: i32,
    );
    // We're simply propagating the `VelloRender` pointer opaquely through the FFI.
    #[allow(improper_ctypes)]
    fn viewer_content_handle_draw(
        raw_viewer_content: Option<NonNull<RawViewerContent>>,
        raw_vello_renderer: Option<NonNull<VelloRenderer>>,
        elapsed: f64,
    );
    fn viewer_content_handle_pointer_move(
        raw_viewer_content: Option<NonNull<RawViewerContent>>,
        x: f32,
        y: f32,
    );
    fn viewer_content_handle_pointer_down(
        raw_viewer_content: Option<NonNull<RawViewerContent>>,
        x: f32,
        y: f32,
    );
    fn viewer_content_handle_pointer_up(
        raw_viewer_content: Option<NonNull<RawViewerContent>>,
        x: f32,
        y: f32,
    );
}

#[derive(Debug)]
pub struct ViewerContent {
    raw_viewer_content: Option<NonNull<RawViewerContent>>,
}

impl ViewerContent {
    pub fn new<P: AsRef<Path>>(path: P) -> Option<Self> {
        let path = path.as_ref();

        let header = {
            let mut file = File::open(path).ok()?;
            let mut buffer = [0; 4];

            file.read_exact(&mut buffer).ok()?;

            buffer
        };

        if &header != b"RIVE" {
            return None;
        }

        let c_str = CString::new(path.as_os_str().as_bytes()).unwrap();
        let raw_viewer_content = Some(unsafe { viewer_content_new(c_str.as_ptr())? });

        Some(Self { raw_viewer_content })
    }

    pub fn handle_resize(&self, width: u32, height: u32) {
        unsafe {
            viewer_content_handle_resize(self.raw_viewer_content, width as i32, height as i32);
        }
    }

    pub fn handle_draw(&mut self, renderer: &mut VelloRenderer, elapsed: f64) {
        unsafe {
            viewer_content_handle_draw(
                self.raw_viewer_content,
                NonNull::new(renderer as *mut VelloRenderer),
                elapsed,
            )
        }
    }

    pub fn handle_pointer_move(&self, pos: Vec2) {
        unsafe {
            viewer_content_handle_pointer_move(self.raw_viewer_content, pos.x as f32, pos.y as f32);
        }
    }

    pub fn handle_pointer_down(&self, pos: Vec2) {
        unsafe {
            viewer_content_handle_pointer_down(self.raw_viewer_content, pos.x as f32, pos.y as f32);
        }
    }

    pub fn handle_pointer_up(&self, pos: Vec2) {
        unsafe {
            viewer_content_handle_pointer_up(self.raw_viewer_content, pos.x as f32, pos.y as f32);
        }
    }
}

impl Drop for ViewerContent {
    fn drop(&mut self) {
        unsafe {
            viewer_content_release(self.raw_viewer_content);
        }
    }
}
