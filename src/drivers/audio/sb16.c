#include "drivers/audio/sb16.h"
#include "drivers/port.h"
#include "arch/int/irq.h"
#include "mem/pmm.h"
#include "util/memory.h"

#define SB16_BASE        0x220
#define SB16_MIXER_ADDR  (SB16_BASE + 0x04)
#define SB16_MIXER_DATA  (SB16_BASE + 0x05)
#define SB16_DSP_RESET   (SB16_BASE + 0x06)
#define SB16_DSP_READ    (SB16_BASE + 0x0A)
#define SB16_DSP_WRITE   (SB16_BASE + 0x0C)
#define SB16_DSP_STATUS  (SB16_BASE + 0x0E)
#define SB16_DSP_ACK16   (SB16_BASE + 0x0F)

/* 4 KiB DMA ring buffer (fits in a single 4 KiB page, never crosses a 64 KiB boundary). */
#define SB16_DMA_BUF_SIZE 4096U
#define SB16_DMA_HALF     (SB16_DMA_BUF_SIZE / 2U)

/* Fixed low-RAM physical page (< 16 MiB, 64 KiB aligned) safe for 8-bit ISA DMA channel 1. */
#define SB16_DMA_PHY_ADDR 0x00020000ULL

static bool s_sb16_ready = false;
static bool s_sb16_probed = false;
static uint8_t *s_dma_buf = NULL;
static uint32_t s_write_pos = 0;

static void sb16_dsp_write(uint8_t val)
{
	for (uint32_t i = 0; i < 100000; i++) {
		if ((inb(SB16_DSP_WRITE) & 0x80) == 0) {
			outb(SB16_DSP_WRITE, val);
			return;
		}
	}
}

static int sb16_dsp_read(void)
{
	for (uint32_t i = 0; i < 100000; i++) {
		if (inb(SB16_DSP_STATUS) & 0x80)
			return (int)inb(SB16_DSP_READ);
	}
	return -1;
}

static void sb16_irq_handler(void)
{
	/* Acknowledge 8-bit (and 16-bit just in case) DSP interrupt so PIC stays unblocked. */
	(void)inb(SB16_DSP_STATUS);
}

static uint32_t sb16_dma_read_pos(void)
{
	/* Read remaining count from ISA DMA channel 1 (port 0x03). */
	outb(0x0C, 0x00); /* Clear flip-flop */
	uint8_t lo = inb(0x03);
	uint8_t hi = inb(0x03);
	uint32_t rem = ((uint32_t)hi << 8) | lo;
	if (rem >= SB16_DMA_BUF_SIZE)
		return 0;
	return (SB16_DMA_BUF_SIZE - 1U - rem) % SB16_DMA_BUF_SIZE;
}

bool sb16_init(void)
{
	if (s_sb16_probed)
		return s_sb16_ready;
	s_sb16_probed = true;

	/* 1. Reset SB16 DSP. */
	outb(SB16_DSP_RESET, 1);
	for (volatile int i = 0; i < 1000; i++) {}
	outb(SB16_DSP_RESET, 0);

	if (sb16_dsp_read() != 0xAA) {
		s_sb16_ready = false;
		return false;
	}

	/* 2. Prepare 4 KiB silence-filled ISA DMA buffer below 16 MiB via HHDM. */
	s_dma_buf = (uint8_t *)phy_to_virt(SB16_DMA_PHY_ADDR);
	memset(s_dma_buf, 0x80, SB16_DMA_BUF_SIZE);
	s_write_pos = SB16_DMA_HALF;

	/* 3. Set SB16 mixer Master (0x22) and Voice/PCM (0x04) volumes to maximum. */
	outb(SB16_MIXER_ADDR, 0x22);
	outb(SB16_MIXER_DATA, 0xFF);
	outb(SB16_MIXER_ADDR, 0x04);
	outb(SB16_MIXER_DATA, 0xFF);

	/* 4. Configure SB16 IRQ = 5 (bit 1 -> 0x02) and 8-bit DMA = channel 1 (bit 1 -> 0x02). */
	outb(SB16_MIXER_ADDR, 0x80);
	outb(SB16_MIXER_DATA, 0x02);
	outb(SB16_MIXER_ADDR, 0x81);
	outb(SB16_MIXER_DATA, 0x02);

	irqr_add(5, sb16_irq_handler);

	/* 5. Program ISA DMA channel 1 for 8-bit auto-init single-mode read (RAM -> SB16). */
	uint32_t phy = (uint32_t)SB16_DMA_PHY_ADDR;
	uint16_t count = (uint16_t)(SB16_DMA_BUF_SIZE - 1U);

	outb(0x0A, 0x05);                       /* Mask DMA channel 1 */
	outb(0x0C, 0x00);                       /* Clear byte pointer flip-flop */
	outb(0x0B, 0x59);                       /* Single mode, increment, auto-init, read, ch 1 */
	outb(0x83, (uint8_t)((phy >> 16) & 0xFF)); /* Page register for ch 1 */
	outb(0x02, (uint8_t)(phy & 0xFF));      /* Base address low */
	outb(0x02, (uint8_t)((phy >> 8) & 0xFF)); /* Base address high */
	outb(0x03, (uint8_t)(count & 0xFF));    /* Count low */
	outb(0x03, (uint8_t)((count >> 8) & 0xFF)); /* Count high */
	outb(0x0A, 0x01);                       /* Unmask DMA channel 1 */

	/* 6. Turn speaker on, set output sample rate = 11025 Hz, start 8-bit auto-init playback. */
	sb16_dsp_write(0xD1);
	sb16_dsp_write(0x41);
	sb16_dsp_write((uint8_t)((SB16_SAMPLE_RATE >> 8) & 0xFF));
	sb16_dsp_write((uint8_t)(SB16_SAMPLE_RATE & 0xFF));

	uint16_t blk = (uint16_t)(SB16_DMA_HALF - 1U);
	sb16_dsp_write(0xC6);                   /* 8-bit auto-init FIFO output */
	sb16_dsp_write(0x00);                   /* Unsigned mono */
	sb16_dsp_write((uint8_t)(blk & 0xFF));
	sb16_dsp_write((uint8_t)((blk >> 8) & 0xFF));

	s_sb16_ready = true;
	return true;
}

void sb16_silence(void)
{
	if (!s_sb16_ready || !s_dma_buf)
		return;
	memset(s_dma_buf, 0x80, SB16_DMA_BUF_SIZE);
}

int64_t sb16_write_pcm(const uint8_t *pcm, size_t len)
{
	if (!pcm)
		return -1;
	if (!s_sb16_ready && !sb16_init())
		return -1;
	if (len == 0)
		return 0;

	uint32_t play_pos = sb16_dma_read_pos();
	uint32_t queued = (s_write_pos - play_pos) % SB16_DMA_BUF_SIZE;

	/* If playback overtook our write cursor (underrun), re-anchor slightly ahead. */
	if (queued > SB16_DMA_BUF_SIZE - 128U) {
		s_write_pos = (play_pos + 256U) % SB16_DMA_BUF_SIZE;
		queued = 256U;
	}

	uint32_t free_space = (SB16_DMA_BUF_SIZE - 256U) - queued;
	if (free_space == 0)
		return 0;

	size_t to_write = (len < free_space) ? len : free_space;
	for (size_t i = 0; i < to_write; i++) {
		s_dma_buf[s_write_pos] = pcm[i];
		s_write_pos = (s_write_pos + 1U) % SB16_DMA_BUF_SIZE;
	}

	return (int64_t)to_write;
}
