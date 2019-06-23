# Uniparser-V3
Translates ASCII strings into command with arguments. Meant to have a genral parser already debugged<br>

Something I do often is to control my boards through a serial interface to a computer<br>
In order to control the board, I use ascii messages like "MOT+17\0" to set position of a motor to 17<br>
Parsing an ascii string is not hard, but parsers are very error prones, and are hard to debug on microcontrollers.<br>
To streamline the process this library was meant to provide a general porpouse parser that took in input a dictionary and returned in output an ID when an input command matches a dictionary entry.<br>
TO simplify things i made use of malloc, which works on the AVR c compiler but fails on the AVR c++ compile for whatever reason.<br>

EXAMPLE<br>
Dictionary:<br>
//Command dictionary. Command IDs 0 and 255 are forbidden<br>
U8 uart_cmd[] =<br>
{<br>
	//Ping: No action. Effect is to reset the connection timeout<br>
	UART_CMD_PING		, 'P', '\0',<br>
	//Sign: Ask for board signature<br>
	UART_CMD_SIGN		, 'F', '\0',<br>
	//Set Speed: R right engine L left engine<br>
	UART_CMD_SETVEL		, 'V', 'R', '%', 'd', 'L', '%', 'd', '\0',<br>
	//Dictionary terminator<br>
	'\0'<br>
};<br>
<br>
Command: <br>
"P\0" will have the call "parser_exe" return a structure with ID UART_CMD_PING and no arguments.
"VR+17L-23\0" will have the call "parser_exe" return a structure with ID UART_CMD_SETVEL and two arguments with value +17 and -23 that can be used to control the motors
