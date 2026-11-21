use crate::port::{inb, outb};

const ATA_PRIMARY_DATA: u16 = 0x1F0;
const ATA_PRIMARY_SECCOUNT: u16 = 0x1F2;
const ATA_PRIMARY_LBA_LO: u16 = 0x1F3;
const ATA_PRIMARY_LBA_MID: u16 = 0x1F4;
const ATA_PRIMARY_LBA_HI: u16 = 0x1F5;
const ATA_PRIMARY_DRIVE: u16 = 0x1F6;
const ATA_PRIMARY_STATUS_CMD: u16 = 0x1F7;

const ATA_SR_BSY: u8 = 0x80;
const ATA_SR_DRQ: u8 = 0x08;
const ATA_SR_ERR: u8 = 0x01;

const ATA_CMD_READ_PIO: u8 = 0x20;
const ATA_CMD_WRITE_PIO: u8 = 0x30;
const ATA_CMD_CACHE_FLUSH: u8 = 0xE7;

#[inline]
fn ata_wait_not_busy() -> bool {
    for _ in 0..100_000 {
        let st = inb(ATA_PRIMARY_STATUS_CMD);
        if (st & ATA_SR_BSY) == 0 {
            return true;
        }
    }
    false
}

#[inline]
fn ata_wait_drq() -> bool {
    for _ in 0..100_000 {
        let st = inb(ATA_PRIMARY_STATUS_CMD);
        if (st & ATA_SR_ERR) != 0 {
            return false;
        }
        if (st & ATA_SR_BSY) == 0 && (st & ATA_SR_DRQ) != 0 {
            return true;
        }
    }
    false
}

#[no_mangle]
pub extern "C" fn ata_read_sectors(lba: u32, count: u8, buf: *mut u8) -> bool {
    if buf.is_null() || count == 0 {
        return false;
    }
    if !ata_wait_not_busy() {
        return false;
    }

    outb(ATA_PRIMARY_DRIVE, 0xE0 | (((lba >> 24) & 0x0F) as u8));
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (lba & 0xFF) as u8);
    outb(ATA_PRIMARY_LBA_MID, ((lba >> 8) & 0xFF) as u8);
    outb(ATA_PRIMARY_LBA_HI, ((lba >> 16) & 0xFF) as u8);
    outb(ATA_PRIMARY_STATUS_CMD, ATA_CMD_READ_PIO);

    let mut dst = buf as *mut u16;
    for _ in 0..(count as usize) {
        if !ata_wait_drq() {
            return false;
        }
        unsafe {
            core::arch::asm!(
                "rep insw",
                in("dx") ATA_PRIMARY_DATA,
                inout("rdi") dst,
                inout("rcx") 256usize => _,
                options(nostack, preserves_flags)
            );
        }
    }
    true
}

#[no_mangle]
pub extern "C" fn ata_write_sectors(lba: u32, count: u8, buf: *const u8) -> bool {
    if buf.is_null() || count == 0 {
        return false;
    }
    if !ata_wait_not_busy() {
        return false;
    }

    outb(ATA_PRIMARY_DRIVE, 0xE0 | (((lba >> 24) & 0x0F) as u8));
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LO, (lba & 0xFF) as u8);
    outb(ATA_PRIMARY_LBA_MID, ((lba >> 8) & 0xFF) as u8);
    outb(ATA_PRIMARY_LBA_HI, ((lba >> 16) & 0xFF) as u8);
    outb(ATA_PRIMARY_STATUS_CMD, ATA_CMD_WRITE_PIO);

    let mut src = buf as *const u16;
    for _ in 0..(count as usize) {
        if !ata_wait_drq() {
            return false;
        }
        unsafe {
            core::arch::asm!(
                "rep outsw",
                in("dx") ATA_PRIMARY_DATA,
                inout("rsi") src,
                inout("rcx") 256usize => _,
                options(nostack, preserves_flags)
            );
        }
    }

    outb(ATA_PRIMARY_STATUS_CMD, ATA_CMD_CACHE_FLUSH);
    ata_wait_not_busy()
}
