use crate::drivers::serial::serial_putc;
use crate::ffi::gfx::print_char;

#[repr(C)]
pub struct CursorRect {
    pub x: u16,
    pub y: u16,
}

#[repr(C)]
pub struct LimineFramebuffer {
    pub address: *mut u32,
    pub width: u64,
    pub height: u64,
    pub pitch: u64,
    pub bpp: u16,
    pub memory_model: u8,
    pub red_mask_size: u8,
    pub red_mask_shift: u8,
    pub green_mask_size: u8,
    pub green_mask_shift: u8,
    pub blue_mask_size: u8,
    pub blue_mask_shift: u8,
    pub unused: [u8; 7],
    pub edid_size: u64,
    pub edid: *mut u8,
    pub mode_count: u64,
    pub modes: *mut *mut u8,
}

#[repr(C)]
pub struct LimineFramebufferResponse {
    pub revision: u64,
    pub framebuffer_count: u64,
    pub framebuffers: *mut *mut LimineFramebuffer,
}

#[repr(C)]
pub struct LimineFramebufferRequest {
    pub id: [u64; 4],
    pub revision: u64,
    pub response: *mut LimineFramebufferResponse,
}

#[inline(always)]
unsafe fn framebuffer_request_ptr() -> *mut LimineFramebufferRequest {
    let ptr: *mut LimineFramebufferRequest;
    core::arch::asm!(
        "lea {}, [rip + framebuffer_request]",
        out(reg) ptr,
        options(pure, nomem, nostack, preserves_flags)
    );
    ptr
}

#[inline(always)]
unsafe fn cursor_ptr() -> *mut CursorRect {
    let ptr: *mut CursorRect;
    core::arch::asm!(
        "lea {}, [rip + cursor]",
        out(reg) ptr,
        options(pure, nomem, nostack, preserves_flags)
    );
    ptr
}

#[inline(always)]
unsafe fn font8x8_ptr() -> *const [[u8; 8]; 128] {
    let ptr: *const [[u8; 8]; 128];
    core::arch::asm!(
        "lea {}, [rip + font8x8_basic]",
        out(reg) ptr,
        options(pure, nomem, nostack, preserves_flags)
    );
    ptr
}

#[inline]
fn fb_channel(value: u8, mask_size: u8, mask_shift: u8) -> u32 {
    let max = (1u64 << mask_size) - 1;
    (((value as u64) * max / 255) << mask_shift) as u32
}

#[inline]
unsafe fn rust_fb_pixel(fb: *const LimineFramebuffer, red: u8, green: u8, blue: u8) -> u32 {
    fb_channel(red, (*fb).red_mask_size, (*fb).red_mask_shift)
        | fb_channel(green, (*fb).green_mask_size, (*fb).green_mask_shift)
        | fb_channel(blue, (*fb).blue_mask_size, (*fb).blue_mask_shift)
}

#[inline]
unsafe fn get_fb() -> *mut LimineFramebuffer {
    let req = framebuffer_request_ptr();
    let resp = core::ptr::read_volatile(&(*req).response);
    if resp.is_null() || (*resp).framebuffer_count < 1 || (*resp).framebuffers.is_null() {
        return core::ptr::null_mut();
    }
    *(*resp).framebuffers
}

#[no_mangle]
pub extern "C" fn gfx_clear_screen() {
    unsafe {
        let fb = get_fb();
        if !fb.is_null() && !(*fb).address.is_null() {
            let ptr = (*fb).address;
            let total_pixels = ((*fb).pitch as usize / 4) * ((*fb).height as usize);
            for i in 0..total_pixels {
                core::ptr::write_volatile(ptr.add(i), 0);
            }
        }
        let cur = cursor_ptr();
        (*cur).x = 0;
        (*cur).y = 0;
    }
}

#[no_mangle]
pub extern "C" fn gfx_scroll_if_needed() {
    unsafe {
        let fb = get_fb();
        if fb.is_null() || (*fb).address.is_null() {
            return;
        }
        let max_rows = ((*fb).height / 8) as u16;
        if max_rows == 0 {
            return;
        }
        let cur = cursor_ptr();
        if (*cur).y >= max_rows - 1 {
            let ptr = (*fb).address;
            let stride = ((*fb).pitch / 4) as usize;
            let row_pixels = stride * 8;
            let copy_pixels = ((max_rows - 1) as usize) * row_pixels;

            for i in 0..copy_pixels {
                let v = core::ptr::read_volatile(ptr.add(i + row_pixels));
                core::ptr::write_volatile(ptr.add(i), v);
            }
            for i in copy_pixels..(copy_pixels + row_pixels) {
                core::ptr::write_volatile(ptr.add(i), 0);
            }
            (*cur).y = max_rows - 2;
        }
    }
}

#[no_mangle]
pub extern "C" fn gfx_putc_color(c: u8, r: u8, g: u8, b: u8) {
    serial_putc(c);

    unsafe {
        let fb = get_fb();
        if fb.is_null() || (*fb).address.is_null() {
            print_char(c);
            return;
        }

        gfx_scroll_if_needed();

        let cur = cursor_ptr();
        let max_cols = ((*fb).width / 8) as u16;
        if c == b'\n' {
            (*cur).x = 0;
            (*cur).y += 1;
            gfx_scroll_if_needed();
            return;
        }

        let fb_ptr = (*fb).address;
        let color = rust_fb_pixel(fb, r, g, b);
        let stride = ((*fb).pitch / 4) as usize;
        let font = font8x8_ptr();
        let glyph = &(*font)[(c & 0x7F) as usize];

        for row in 0..8usize {
            let row_bits = glyph[row];
            for col in 0..8usize {
                if (row_bits & (1u8 << col)) != 0 {
                    let px = ((*cur).x as usize) * 8 + col;
                    let py = ((*cur).y as usize) * 8 + row;
                    core::ptr::write_volatile(fb_ptr.add(py * stride + px), color);
                }
            }
        }

        (*cur).x += 1;
        if max_cols > 2 && (*cur).x >= max_cols - 1 {
            (*cur).x = 0;
            (*cur).y += 1;
            gfx_scroll_if_needed();
        }
    }
}

#[inline]
unsafe fn cstr_len(s: *const u8) -> usize {
    if s.is_null() {
        return 0;
    }
    let mut len = 0usize;
    while core::ptr::read_volatile(s.add(len)) != 0 {
        len += 1;
    }
    len
}

#[no_mangle]
pub extern "C" fn gfx_print_color(s: *const u8, r: u8, g: u8, b: u8) {
    if s.is_null() {
        return;
    }
    let mut i = 0usize;
    unsafe {
        loop {
            let ch = core::ptr::read_volatile(s.add(i));
            if ch == 0 {
                break;
            }
            gfx_putc_color(ch, r, g, b);
            i += 1;
        }
    }
}

#[no_mangle]
pub extern "C" fn gfx_print_u64(mut val: u64, r: u8, g: u8, b: u8) {
    if val == 0 {
        gfx_putc_color(b'0', r, g, b);
        return;
    }
    let mut buf = [0u8; 24];
    let mut pos = 0usize;
    while val > 0 && pos < 22 {
        buf[pos] = b'0' + (val % 10) as u8;
        val /= 10;
        pos += 1;
    }
    while pos > 0 {
        pos -= 1;
        gfx_putc_color(buf[pos], r, g, b);
    }
}

#[no_mangle]
pub extern "C" fn gfx_print_u64_pad(mut val: u64, w: i32, suffix: *const u8, r: u8, g: u8, b: u8) {
    let mut buf = [0u8; 32];
    let mut pos = 0usize;
    if val == 0 {
        buf[pos] = b'0';
        pos += 1;
    } else {
        while val > 0 && pos < 20 {
            buf[pos] = b'0' + (val % 10) as u8;
            val /= 10;
            pos += 1;
        }
    }

    let sfx_len = unsafe { cstr_len(suffix) };
    let total_len = (pos + sfx_len) as i32;
    while pos > 0 {
        pos -= 1;
        gfx_putc_color(buf[pos], r, g, b);
    }
    if !suffix.is_null() {
        gfx_print_color(suffix, r, g, b);
    }
    let mut pad = total_len;
    while pad < w {
        gfx_putc_color(b' ', r, g, b);
        pad += 1;
    }
}

#[no_mangle]
pub extern "C" fn gfx_print_fixed10_pad(val_x10: u64, w: i32, suffix: *const u8, r: u8, g: u8, b: u8) {
    let mut whole = val_x10 / 10;
    let frac = (val_x10 % 10) as u8;
    let mut buf = [0u8; 32];
    let mut pos = 0usize;

    if whole == 0 {
        buf[pos] = b'0';
        pos += 1;
    } else {
        while whole > 0 && pos < 16 {
            buf[pos] = b'0' + (whole % 10) as u8;
            whole /= 10;
            pos += 1;
        }
    }

    let digits = pos;
    while pos > 0 {
        pos -= 1;
        gfx_putc_color(buf[pos], r, g, b);
    }
    gfx_putc_color(b'.', r, g, b);
    gfx_putc_color(b'0' + frac, r, g, b);

    let sfx_len = unsafe { cstr_len(suffix) };
    if !suffix.is_null() {
        gfx_print_color(suffix, r, g, b);
    }

    let mut printed = (digits + 2 + sfx_len) as i32;
    while printed < w {
        gfx_putc_color(b' ', r, g, b);
        printed += 1;
    }
}
