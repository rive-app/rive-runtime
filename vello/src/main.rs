use std::{fs, time::Instant};

use rive_rs::{Artboard, File, Handle, Instantiate, Viewport};
use vello::{
    kurbo::{Affine, Rect, Vec2},
    peniko::{Color, Fill},
    util::{RenderContext, RenderSurface},
    Renderer, RendererOptions, Scene, SceneBuilder,
};
use winit::{
    dpi::LogicalSize,
    event::{ElementState, Event, MouseButton, WindowEvent},
    event_loop::{ControlFlow, EventLoop},
    window::{Window, WindowBuilder},
};

struct RenderState {
    surface: RenderSurface,
    window: Window,
}

const INITIAL_WINDOW_SIZE: LogicalSize<u32> = LogicalSize::new(700, 700);
const FRAME_STATS_CAPACITY: usize = 30;
const SCROLL_FACTOR_THRESHOLD: f64 = 100.0;

fn main() {
    let mut viewport = Viewport::default();
    let mut scene: Option<Box<dyn rive_rs::Scene>> = None;

    let event_loop = EventLoop::new();
    let mut cached_window: Option<Window> = None;
    let mut renderer: Option<Renderer> = None;
    let mut render_cx = RenderContext::new().unwrap();
    let mut render_state: Option<RenderState> = None;

    let mut mouse_pos = Vec2::default();
    let mut scroll_delta = 0.0;
    let mut frame_start_time = Instant::now();
    let mut stats = Vec::with_capacity(FRAME_STATS_CAPACITY);

    event_loop.run(move |event, _event_loop, control_flow| match event {
        Event::WindowEvent { ref event, .. } => {
            let Some(render_state) = &mut render_state else {
                return;
            };

            match event {
                WindowEvent::CloseRequested => *control_flow = ControlFlow::Exit,
                WindowEvent::Resized(size) => {
                    viewport.resize(size.width, size.height);

                    render_cx.resize_surface(&mut render_state.surface, size.width, size.height);
                    render_state.window.request_redraw();
                }
                WindowEvent::MouseInput {
                    state,
                    button: MouseButton::Left,
                    ..
                } => {
                    if let Some(scene) = &mut scene {
                        match state {
                            ElementState::Pressed => scene.pointer_down(
                                mouse_pos.x as f32,
                                mouse_pos.y as f32,
                                &viewport,
                            ),
                            ElementState::Released => {
                                scene.pointer_up(mouse_pos.x as f32, mouse_pos.y as f32, &viewport)
                            }
                        }
                    }
                }
                WindowEvent::CursorMoved { position, .. } => {
                    mouse_pos = Vec2::new(position.x, position.y);
                    if let Some(scene) = &mut scene {
                        scene.pointer_move(mouse_pos.x as f32, mouse_pos.y as f32, &viewport);
                    }
                }
                WindowEvent::MouseWheel { delta, .. } => match delta {
                    winit::event::MouseScrollDelta::LineDelta(_, lines_y) => {
                        scroll_delta = (scroll_delta
                            - (*lines_y as f64).signum() * SCROLL_FACTOR_THRESHOLD)
                            .max(0.0);
                    }
                    winit::event::MouseScrollDelta::PixelDelta(pixels) => {
                        scroll_delta = (scroll_delta - pixels.y).max(0.0);
                    }
                },
                WindowEvent::DroppedFile(path) => {
                    scene = Some({
                        let file = File::new(&fs::read(path).unwrap()).unwrap();
                        let artboard = Artboard::instantiate(&file, Handle::Default).unwrap();

                        Box::<dyn rive_rs::Scene>::instantiate(&artboard, Handle::Default).unwrap()
                    });
                }
                _ => {}
            }
        }
        Event::MainEventsCleared => {
            if let Some(render_state) = &mut render_state {
                render_state.window.request_redraw();
            }
        }
        Event::RedrawRequested(_) => {
            let mut rive_renderer = rive_rs::Renderer::default();
            let factor = (scroll_delta / SCROLL_FACTOR_THRESHOLD).max(1.0) as u32;

            let elapsed = &frame_start_time.elapsed();
            stats.push(elapsed.as_secs_f64());

            if stats.len() == FRAME_STATS_CAPACITY {
                let average = stats.drain(..).sum::<f64>() / FRAME_STATS_CAPACITY as f64;

                if let Some(state) = &mut render_state {
                    let copies = (factor > 1)
                        .then(|| format!(" ({} copies)", factor.pow(2)))
                        .unwrap_or_default();
                    state.window.set_title(&format!(
                        "Rive on Vello demo | {:.2}ms{}",
                        average * 1000.0,
                        copies
                    ));
                }
            }

            frame_start_time = Instant::now();

            let Some(render_state) = &mut render_state else {
                return;
            };
            let width = render_state.surface.config.width;
            let height = render_state.surface.config.height;
            let device_handle = &render_cx.devices[render_state.surface.dev_id];

            let render_params = vello::RenderParams {
                base_color: Color::DIM_GRAY,
                width,
                height,
            };

            let surface_texture = render_state
                .surface
                .surface
                .get_current_texture()
                .expect("failed to get surface texture");

            let mut vello_scene = Scene::default();
            let mut builder = SceneBuilder::for_scene(&mut vello_scene);

            if let Some(scene) = &mut scene {
                scene.advance_and_maybe_draw(&mut rive_renderer, *elapsed, &mut viewport);

                for i in 0..factor.pow(2) {
                    builder.append(
                        rive_renderer.scene(),
                        Some(
                            Affine::default()
                                .then_scale(1.0 / factor as f64)
                                .then_translate(Vec2::new(
                                    (i % factor) as f64 * width as f64 / factor as f64,
                                    (i / factor) as f64 * height as f64 / factor as f64,
                                )),
                        ),
                    );
                }
            } else {
                // Vello doesn't draw base color when there is no geometry.
                builder.fill(
                    Fill::NonZero,
                    Affine::IDENTITY,
                    Color::TRANSPARENT,
                    None,
                    &Rect::new(0.0, 0.0, 0.0, 0.0),
                );
            }

            if !vello_scene.data().is_empty() {
                vello::block_on_wgpu(
                    &device_handle.device,
                    renderer.as_mut().unwrap().render_to_surface_async(
                        &device_handle.device,
                        &device_handle.queue,
                        &vello_scene,
                        &surface_texture,
                        &render_params,
                    ),
                )
                .expect("failed to render to surface");
            }

            surface_texture.present();
            device_handle.device.poll(wgpu::Maintain::Poll);
        }
        Event::Suspended => {
            if let Some(render_state) = render_state.take() {
                cached_window = Some(render_state.window);
            }
            *control_flow = ControlFlow::Wait;
        }
        Event::Resumed => {
            if render_state.is_some() {
                return;
            }

            let window = cached_window.take().unwrap_or_else(|| {
                WindowBuilder::new()
                    .with_inner_size(INITIAL_WINDOW_SIZE)
                    .with_resizable(true)
                    .with_title("Rive on Vello demo")
                    .build(_event_loop)
                    .unwrap()
            });
            let size = window.inner_size();
            let surface_future = render_cx.create_surface(&window, size.width, size.height);

            let surface = pollster::block_on(surface_future).expect("Error creating surface");
            render_state = {
                let render_state = RenderState { window, surface };
                renderer = Some(
                    Renderer::new(
                        &render_cx.devices[render_state.surface.dev_id].device,
                        &RendererOptions {
                            surface_format: Some(render_state.surface.format),
                            timestamp_period: render_cx.devices[render_state.surface.dev_id]
                                .queue
                                .get_timestamp_period(),
                            use_cpu: false,
                        },
                    )
                    .expect("Could create renderer"),
                );
                Some(render_state)
            };
            *control_flow = ControlFlow::Poll;
        }
        _ => {}
    });
}
