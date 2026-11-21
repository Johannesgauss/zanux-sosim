ENTRY(_start)

SECTIONS {
	. = 0x0000000000400000;

	.text : {
		*(.text.entry)
		*(.text .text.*)
	}

	.rodata : {
		*(.rodata .rodata.*)
	}

	.data : {
		*(.data .data.*)
	}

	.bss : {
		__bss_start = .;
		*(.bss .bss.*)
		*(COMMON)
		__bss_end = .;
	}

	/DISCARD/ : {
		*(.eh_frame)
		*(.note .note.*)
		*(.comment)
	}
}
