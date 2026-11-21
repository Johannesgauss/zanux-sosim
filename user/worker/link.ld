ENTRY(_start)

SECTIONS {
	. = 0x0000000000400000;

	.text : {
		*(.text.entry)
		*(.text .text.*)
	}

	/DISCARD/ : {
		*(.eh_frame)
		*(.note .note.*)
		*(.comment)
	}
}
