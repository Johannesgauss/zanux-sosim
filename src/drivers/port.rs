#[no_mangle]
pub extern "C" fn inb(port: u16) -> u8
{
    let value:u8;
    unsafe {
        core::arch::asm!(
            "in al, dx",
            in("dx") port,
            out("al") value,
            options(nomem, nostack, preserves_flags)
        );
    };
    return value;
}


#[no_mangle]
pub extern "C" fn outb(port: u16, value: u8) -> u8
{
    unsafe {
        core::arch::asm!(
            "out dx, al",
            in("dx") port,
            in("al") value,
            options(nomem, nostack, preserves_flags)
        );
    };
    return value;
}

#[no_mangle]
pub extern "C" fn io_wait()
{
    outb(0x80, 0);
}

#[no_mangle]
pub extern "C" fn inw(port: u16) -> u16
{
    let value: u16;
    unsafe {
        core::arch::asm!(
            "in ax, dx",
            in("dx") port,
            out("ax") value,
            options(nomem, nostack, preserves_flags)
        );
    };
    value
}

#[no_mangle]
pub extern "C" fn outw(port: u16, value: u16)
{
    unsafe {
        core::arch::asm!(
            "out dx, ax",
            in("dx") port,
            in("ax") value,
            options(nomem, nostack, preserves_flags)
        );
    };
}
