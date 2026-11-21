use crate::port::{inb, outb};

const COM1_PORT: u16 = 0x3F8;

#[no_mangle]
pub extern "C" fn serial_putc(c: u8) {
    if c == b'\n' {
        while (inb(COM1_PORT + 5) & 0x20) == 0 {}
        outb(COM1_PORT, b'\r');
    }
    while (inb(COM1_PORT + 5) & 0x20) == 0 {}
    outb(COM1_PORT, c);
}

#[no_mangle]
pub extern "C" fn serial_puts(s: *const u8) {
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
            serial_putc(ch);
            i += 1;
        }
    }
}
