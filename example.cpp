/****************************************************************
**	OrangeBot Project
*****************************************************************
**        /
**       /
**      /
** ______ \
**         \
**          \
*****************************************************************
**	Project
*****************************************************************
**  Brief
****************************************************************/

/****************************************************************
**	DESCRIPTION
****************************************************************
**
****************************************************************/

/****************************************************************
**	HISTORY VERSION
****************************************************************
**
****************************************************************/

/****************************************************************
**	KNOWN BUGS
****************************************************************
**
****************************************************************/

/****************************************************************
**	TODO
****************************************************************
**
****************************************************************/

/****************************************************************
**	INCLUDES
****************************************************************/

//Standard C Libraries
//#include <cstdio>
//#include <cstdlib>

//Standard C++ libraries
#include <iostream>
//#include <array>
//#include <vector>
//#include <queue>
//#include <string>
//#include <fstream>
//#include <chrono>
//#include <thread>

//OS Libraries
//#define _WIN32_WINNT 0x0500	//Enable GetConsoleWindow
//#include <windows.h>

//User Libraries
//Include user log trace
#include "debug.h"

#include "at_utils.h"
#include "at_parser.h"


/****************************************************************
**	NAMESPACES
****************************************************************/

//Never use a whole namespace. Use only what you need from it.
using std::cout;
using std::endl;

/****************************************************************
**	DEFINES
****************************************************************/

//#define TEST_GARBAGE
//#define TEST_PING
//#define TEST_FIND
#define TEST_SETVEL

	///--------------------------------------------------------------------------
	///	COMMANDS
	///--------------------------------------------------------------------------

//Communication timeout (tick@50Hz)
#define UART_TIMEOUT		50
//max length of board signature string
#define MAX_SIGN_LEN		16
//Ping command
#define UART_CMD_PING		1
//Sign command. Board is expected to answer with the signature string on UART
#define UART_CMD_SIGN		2
//Set desired forward and sideway velocity
#define UART_CMD_SETVEL		10

/****************************************************************
**	MACROS
****************************************************************/

/****************************************************************
**	PROTOTYPES
****************************************************************/

/****************************************************************
**	GLOBAL VARIABILES
****************************************************************/

//User::Dummy my_class;

//Command dictionary. Command IDs 0 and 255 are forbidden
U8 uart_cmd[] =
{
	//Ping: No action. Effect is to reset the connection timeout
	UART_CMD_PING		, 'P', '\0',
	//Sign: Ask for board signature
	UART_CMD_SIGN		, 'F', '\0',
	//Set Speed: R right engine L left engine
	UART_CMD_SETVEL		, 'V', 'R', '%', 'd', 'L', '%', 'd', '\0',
	//Dictionary terminator
	'\0'
};
//Board Signature
U8 *board_sign = (U8 *)"MazeRunner_05032";

/****************************************************************
**	FUNCTIONS
****************************************************************/

//Test functions
bool test_bench( void );
//Feed a byte to the parser and see what it decodes
bool feed_data_to_parser( Parser* parser, U8 in );

/****************************************************************************
**	Function
**	main |
****************************************************************************/
//! @return bool |
//! @brief dummy method to copy the code
//! @details verbose description
/***************************************************************************/

int main()
{
	//----------------------------------------------------------------
	//	STATIC VARIABILE
	//----------------------------------------------------------------

	//----------------------------------------------------------------
	//	LOCAL VARIABILE
	//----------------------------------------------------------------

	//----------------------------------------------------------------
	//	CHECK AND INITIALIZATIONS
	//----------------------------------------------------------------

	//Start Debugging. Show function nesting level 0 and above
	DSTART( 0 );
	//Trace Enter main
	DENTER();

	//----------------------------------------------------------------
	//	BODY
	//----------------------------------------------------------------

	cout << "OrangeBot Projects\n";
	//print in the 'debug.log' file. works just like a fully featured printf
	DPRINT("OrangeBot Projects\n");

	test_bench();

	//----------------------------------------------------------------
	//	FINALIZATIONS
	//----------------------------------------------------------------

	//Trace Return from main
	DRETURN();
	//Stop Debugging
	DSTOP();

    return 0;
}	//end function: main

/****************************************************************************
**	Function
**	test_bench |
****************************************************************************/
//! @return bool |
//! @brief test the library
//! @details feed a data to the parsed and decode decoded packets
/***************************************************************************/

bool test_bench( void )
{
	//----------------------------------------------------------------
	//	STATIC VARIABILE
	//----------------------------------------------------------------

	//----------------------------------------------------------------
	//	LOCAL VARIABILE
	//----------------------------------------------------------------

	//----------------------------------------------------------------
	//	CHECK
	//----------------------------------------------------------------

	//----------------------------------------------------------------
	//	INITIALIZATIONS
	//----------------------------------------------------------------

	//Trace Enter with arguments
	DENTER_ARG("in: %d\n", 0);

			///Parser
	//This structure hold a link to the command dictionary, the parser status variables and a link to the partial packet structure
	Parser *parser = (Parser *)NULL;

		///Parser Init
	//Create a new parser. uart_cmd is the dictionary for this parser (i can have multiple indipendent parsers)
	parser = born_parser( uart_cmd );
	//If: parser creation failed
	if (parser == NULL)
	{
		//Signal Error
		DPRINT("ERR: parser could not be allocated\n");
		DRETURN();
		return 0;
	}

	//----------------------------------------------------------------
	//	BODY
	//----------------------------------------------------------------
	//! @details algorithm:


	#ifdef TEST_GARBAGE

	feed_data_to_parser( parser, '1' );
	feed_data_to_parser( parser, 'X' );
	feed_data_to_parser( parser, '\0' );
	feed_data_to_parser( parser, '\0' );
	feed_data_to_parser( parser, 0x23 );

	#endif

		//----------------------------------------------------------------
		//	PING COMMAND
		//----------------------------------------------------------------

	#ifdef TEST_PING

	feed_data_to_parser( parser, 'P' );
	feed_data_to_parser( parser, '\0' );

	#endif

		//----------------------------------------------------------------
		//	FIND COMMAND
		//----------------------------------------------------------------

	#ifdef TEST_FIND

	feed_data_to_parser( parser, 'F' );
	feed_data_to_parser( parser, '\0' );

	#endif

		//----------------------------------------------------------------
		//	SET VELOCITU
		//----------------------------------------------------------------

	#ifdef TEST_SETVEL

	feed_data_to_parser( parser, 'V' );
	feed_data_to_parser( parser, 'R' );
	feed_data_to_parser( parser, '+' );
	feed_data_to_parser( parser, '1' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, 'L' );
	feed_data_to_parser( parser, '+' );
	feed_data_to_parser( parser, '1' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, '\0' );

	feed_data_to_parser( parser, 'V' );
	feed_data_to_parser( parser, 'R' );
	feed_data_to_parser( parser, '+' );
	feed_data_to_parser( parser, '1' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, 'L' );
	feed_data_to_parser( parser, '+' );
	feed_data_to_parser( parser, '1' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, '0' );
	feed_data_to_parser( parser, '\0' );

	#endif
	//----------------------------------------------------------------
	//	FINALIZATIONS
	//----------------------------------------------------------------

	bury_parser( parser );

	//----------------------------------------------------------------
	//	RETURN
	//----------------------------------------------------------------

	//Trace Return vith return value
	DRETURN_ARG("out: %d\n", 0);

	return true; //OK
}	//end function: Dummy | bool

/****************************************************************************
**	Function
**	feed_data_to_parser | U8
****************************************************************************/
//! @param in U8 | character to be decoded
//! @return bool | true: bad | false: all ok
//! @brief parser feeder
//! @details feed a data, decode possible packets and destroy them
/***************************************************************************/

bool feed_data_to_parser( Parser *parser, U8 in )
{
	DENTER_ARG("parser: %p | data: >%x<\n", (void *)parser, in);

	S8 s8t;
	//parser exe return a Parser_packet structure if a command is successfully decoded. User must manually destroy it when done
	Parser_packet *packet = (Parser_packet *)NULL;


		///Command parser
	//feed the input RX byte to the parser
	packet = parser_exe( parser, in );
	//if: the parser_exe is doing t's stuffs
	if (packet == NULL)
	{
		//do nothing
	}
	//if: parser exe has fully decoded a command
	else
	{
			///UART_CMD_PING
		//if: ping. Only used to reset the com timeout counter
		if (packet -> id == UART_CMD_PING)
		{
			DPRINT("Ping decoded\n");
			//do nothing
		}
			///UART_CMD_SIGN
		//If: Master is asking for signature
		else if (packet -> id == UART_CMD_SIGN)
		{
			//If: I have a valid board signature string
			if (board_sign != NULL)
			{
				DPRINT("Request for signature: %s\n", board_sign);
			}	//End If: I have a valid board signature string
		}	//End If: Master is asking for signature
			///UART_CMD_SETVEL
		else if (packet -> id == UART_CMD_SETVEL)
		{
			//fetch x. Stored in arg 0 of packet
			s8t = packet_get_s8( packet, 0 );
			DPRINT("Right engine: %d\n", s8t);
			//fetch y. Stored in arg 1 of packet
			s8t = packet_get_s8( packet, 1 );
			DPRINT(" Left engine: %d\n", s8t);

		}
		//if: parser decoded a command that i am not handling
		else
		{
			DPRINT("Unhandled packet decoded\n");
		}
		//Manually dispose of the packet structure
		bury_packet( packet );
		packet = (Parser_packet *)NULL;
	} //End parser

	DRETURN();
	return false;
}
