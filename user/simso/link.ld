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
		*(.bss .bss.*)
		*(COMMON)
	}

	/DISCARD/ : {
		*(.eh_frame)
		*(.note .note.*)
		*(.comment)
	}
}
