/****************************************************************************
**	R@P PROJECT
*****************************************************************************
**
*****************************************************************************
**	Author: 			Orso Eric
**	Creation Date:		24/08/11
**	Last Edit Date:
**	Revision:			1
**	Version:			0.1 ALFA
*****************************************************************************
**	DEPENDENCIES:
**	stdint.h
**	at_utils.h
**	MALLOC.h
****************************************************************************/

/****************************************************************************
**	HYSTORY VERSION
*****************************************************************************
**	0.1 ALPHA
**		>Written the cmd check function. Was a good idea, I could write
**		the error states, the status transitions and check the basic flow
**		of the FSM (the check FSM is a stripped down version of the exe one)
**		>Writing the parser_exe. I'm splitting the main parser in auxiliary functions
**		>written the parser_search_first. When IDLE find all command with the first byte as the given one
**		>I stiched together parser_exe enough that it can decode string commands
**		IDEA: look next cmd entry to jump into status arg
**		>Intrduced error codes. Error codes are handled in FSM post processing
**		>now handle ARG->ID state transition
**		>code tightening.
**		now the status word is a structure, i have cnt and 2 bits that
**		store argument sign and argument first byte processed
**		>solved a bug. I need to destroy old arguments when I come out of status IDLE
**		I had kept when i went into IDLE to allow user to fetch them
**		handled by a new section in post processign (flag 4)
**		>now all argument decoding work!
**		>Now i optimize id_index pruning
**	0.2 ALPHA
**		Memory problem with fetching arguments, i create a dedicated output structure
**		>Added the support for packet memory structure.
**			>born, bury
**			>add argument
**			>calc arg_index
**			>get U8, S8, U16, S16
**			error code generation
**		>Bug Solved. Arg, fix, ARG would bug and create 3 arguments
**		the problem was that i need to initialize parser_tmp.status.arg_first
**		when I go from ID -> ARG. otherwise it works for first argument,
**		but bug out for the second one
**		I have to clear argument sign for the same reason parser_tmp.status.arg_sign
****************************************************************************/

/****************************************************************************
**	DESCRIPTION
*****************************************************************************
**	A command is composed by fixed alphanumeric chars and argument descriptors
**	Each command is associated with a unique ID
**	A command cannot start with an argument descriptor
**		Example command
**	Dictionary: 17, 'c', 'i', 'a', 'o', '\0'
**	Input:	"ciao come va"
**			    ^ Here parser exe will return 17. Before and after 0
**	Dictionary: 91, 'p', 'o', 's', '%', 'u', '\0'
**	Input:	"pos+32pos-120"
**			      ^      ^ First return 91, with +32 inside parser structure
**						   Second return 91 with -120 inside parser structure
**	Writing a working dictionary is important. And complex to check
**	I do it once upon parser creation. If I put all the check c
**	EXAMPLES: Can/Can't:
**	>You CANNOT have commands that share IDs (Not Implemened Yet)
**	Actually there is no implementation reason, and could be useful... I'm thinking about it
**		8,	"hi"
**		8,	"hello"		<- ERROR
**		9,	"hello"		<- OK
**	>You CANNOT have commands that fork argument/fix
**	It's not allowed because it increase the complexity of the FSM (i would need a state for each id_index entry)
**		9,	"cia%u"
**		10,	"ciao"		<- ERROR (9 and 10 share cia, but fork % and o.
**		10,	"cioo"		<- OK
**	>You CANNOT have commands that differs only in type of argument
**	When the parser get '+', '3', ... It has no way of knowing which argument use
**		9,	"Miao%u"
**		10,	"Miao%D"	<-ERROR
**		10, "Bau%D"		<- OK
**	>You CAN have commands that are contained in each others
**	Command are executed upon processing an incoming '\0' it means that this will have the intended behaviour
**		9,	"hell"
**		10, "hello"		<- OK
**	>You CAN have commands that have multiple arguments
**	In this example, The parser structure will have a length of 2. you can access the result using the macros.
**	You wrote the command, you have to specify inside the macros at which offset variabiles are found.
**	If arg_len is 0xff, it means at least one argument overflowed
**		93,	"MovX%uY%u"
**	>You CAN feed arguments without signs
**	I reworked the state transition. By restricting commands not to fork between arg or fix,
**	I jump based on dictionary content (%->ARG/else ID), so I can have signeless numbers
**		cmd: 	81,	"MovX%u"
**		data:	"Mov32"		<- OK
**		data:	"Mov+32"	<- OK
**	>You CANNOT put 2 argument back to back. separate them with at least one fixed char
**		identifying a second sign in ARG would require another status var
**		cmd: 	81,	"Mov%u%u" <- ERR
**		cmd: 	81,	"Mov%uy%u" <- OK
**
**
*****************************************************************************
**	Command restriction:
**	>Cannot have ID = 0 or 255
**	>Cannot start with an argument
**	>Cannot start with a number
**	>Can only start with a letter
**	>Valid arguments are %u %s %U %S
**	>Arguments are stored inside parser structure
**	>After valid ID access parameters using Parser Macros
*****************************************************************************
**	PROBLEM:
**	>How do I handle command that starts the same way?
**	example 11 kaaaaa, 12 kaaab, 13 kaaaac
**	IDEAS:
**	1) Do not allow for that (only first entry count)
**		It's counter intuitive
**	2) when I check first byte, remember how many commands start with that.
**	Uhm... I need a vector of indexes to handle this, and complexity
**	Otherwise I need to remember the cumber of similar command and my command string
**	SOLUTION:
**	When I search for first byte i search all dictionary and remember the starting
**	point of all command (index vector)
**	Each time a byte between commands differ, i remove it from the indexes list
**	A miss happen when that number fall to zero
**	Example
**	11 Kaaaaa
**	12 Kaab
**	13 Kaaaac
**	14 kaaaaad < ERR: I would never get here. commands that fully contain other commands are forbidden
**	It's easier if numbers always use the sign
****************************************************************************/

/****************************************************************************
**	KNOWN BUG
*****************************************************************************
**	>The log contained a chinese string! it was caused by a %c that printed 0
**	>I changed malloc to builtin malloc and did stuff but it's not enough
**	I restricted the bug to the access of the argument
**	I add a packet structure where i put data and I make functions to safely access them
**	more overhead but better
****************************************************************************/

/****************************************************************************
**	TODO
*****************************************************************************
**	>Check for duplicated ID
**	>Check for command with same first byte (it's not an error, but they would be ignored
**	>Check for commands that fully contains other commands
****************************************************************************/


/****************************************************************************
**	INCLUDE
****************************************************************************/

//#define DEBUG_FILE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "debug.h"
#include "at_utils.h"
#include "at_parser.h"

/****************************************************************************
**	GLOBAL VARIABILE
****************************************************************************/

//Last error of the parser
Parser_error_codes parser_error_code = PARSER_OK;

/****************************************************************************
**	FUNCTION
****************************************************************************/

/****************************************************************************
**	born_parser
*****************************************************************************
**	PARAMETER:
**		U8 *cmd:	Link to a dictionary
**	RETURN:
**	DESCRIPTION:
****************************************************************************/

Parser *born_parser( U8 *cmd )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//Return value
	Parser *ret;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("dictionary: %p\n", cmd);

	//If: null pointer argument
	if (cmd == NULL)
	{
		DRETURN_ARG("ERR: null pointer argument\n");
		return NULL;
	}
	//if: the dictionary is good
	if (parser_cmd_chk( cmd ) != PARSER_OK)
	{
		DRETURN_ARG("ERR: Bad dictionary\n");
		return NULL;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

		///Allocate the base structure

	//Call malloc
	ret = (Parser *)MALLOC(sizeof(Parser) *1);
	DPRINT("MALLOC: %d bytes | adr: %p\n", sizeof(Parser) *1, (void *)ret);
	//If: malloc failed
	if (ret == NULL)
	{
		DRETURN_ARG("ERR: PARSER_ERROR_MALLOC_FAIL!\n");
		return NULL;
	}
		///Link and fill
	ret -> cmd 				= cmd;
	ret -> packet			= NULL;			//a decoded command is stored ina PArser_packet structure
	ret -> status.cnt 		= PARSER_IDLE;
	ret -> status.arg_sign 	= 0;
	ret -> status.arg_first	= 0;

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN_ARG("adr: %p\n", (void *)ret);

	return ret;
}	//end function: born_parser

/****************************************************************************
**	bury_parser
*****************************************************************************
**	PARAMETER:
**		Parser *	: pointer to the parser the user wants disposed off
**	RETURN:
**		U8 *link to dictionary
**	DESCRIPTION:
**	Safely dispose of a parser structure.
**	The link to the dictionary is returned since the dictionary was not allocated by this
****************************************************************************/

U8 *bury_parser( Parser *parser )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	U8 *cmd = NULL;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("adr: %p\n", (void *)parser);
	if (parser == NULL)
	{
		DRETURN_ARG("ERR: Invalid Parser\n");
		return NULL;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//save link to dictionart
	cmd = parser -> cmd;
	//destroy argument vector
	if (parser -> packet != NULL)
	{
		bury_packet( parser -> packet);
	}
	//destroy ID incdex vector
	if (parser -> id_index != NULL)
	{
		FREE( parser -> id_index );
	}
	//destroy parser structure
	FREE( parser );

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN();

	return cmd;
}	//end function: bury_parser

/****************************************************************************
**	born_packet
*****************************************************************************
**	PARAMETER:
**	RETURN:
**	DESCRIPTION:
**	Create a packet structure with space for a desctiptor. 0x00 will create an empty one
****************************************************************************/

Parser_packet *born_packet( void )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//Return var
	Parser_packet *ret = NULL;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER();
	/*
	//If: the user gave a bad arg descriptor
	if ((arg_descriptor != 0x00) && (!IS_ARG_DESCRIPTOR(arg_descriptor)))
	{
		//Signal error code
		parser_error_code = PARSER_ERROR_INVALID_ARG_DESCRIPTOR;
		//Return with error
		return NULL;
	}
	*/

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//allocate a parser packet structure
	ret = (Parser_packet *)MALLOC( sizeof(Parser_packet) *1 );
	DPRINT("MALLOC: %d bytes | adr: %p\n", sizeof(Parser_packet) *1, (void *)ret);
	//If: malloc fail
	if (ret == NULL)
	{
		//Signal error code
		parser_error_code = PARSER_ERROR_MALLOC_FAIL;
		//Return with error
		return NULL;
	}
	//Initialize structure
	ret -> id 			= 0x00;
	ret -> arg_num 		= 0;
	ret -> arg_len		= 0;
	ret -> arg			= NULL;

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN_ARG("adr: %p\n",(void *)ret);
	return ret;
}	//end function: born_packet

/****************************************************************************
**	bury_packet
*****************************************************************************
**	PARAMETER:
**		Parser_packet *packet		: Source packet
**	RETURN:
**      0: OK otherwise FAIL
**	DESCRIPTION:
**	Safely dispose of a packet structure
****************************************************************************/

U8 bury_packet( Parser_packet *packet )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("adr: %p\n",(void *)packet);
	if (packet == NULL)
	{
		DRETURN_ARG("ERR: packet does not exist");
		return (U8)0xff;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//Free argument list
	if (packet -> arg != NULL)
	{
		DPRINT("FREE: adr: %p\n", (void *)packet -> arg);
		FREE( packet -> arg );
		packet -> arg = NULL;
	}
	//Free base structure
	DPRINT("FREE: adr: %p\n", (void *)packet);
	FREE( packet );
	packet = NULL;

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN();
	return 0x00;
}	//end function: bury_packet

/****************************************************************************
**	packet_add_arg
*****************************************************************************
**	PARAMETER:
**		Parser_packet *packet		: source packet structure
**		U8 arg_descriptor			: argument to be added
**	RETURN:
**	DESCRIPTION:
**	Add an argument to a packet, initialize it
****************************************************************************/

U8 packet_add_arg( Parser_packet *packet, U8 arg_descriptor )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//Temp vars
	S8 t;		//temp counter
	U8 *u8p;	//Temp pointer
	//calculations
	U8 arg_num, new_arg_num;
	U8 arg_len, new_arg_len;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("adr: %p | arg: >%c<\n", (void *)packet, arg_descriptor);
	//If: bad parameters
	if ((packet == NULL) || (!IS_ARG_DESCRIPTOR(arg_descriptor)))
	{
		parser_error_code = PARSER_ERROR_NULL_POINTER_PARAMETER;
		DRETURN_ARG("ERR%d: PARSER_ERROR_NULL_POINTER_PARAMETER\n", parser_error_code);
		return PARSER_ERROR_NULL_POINTER_PARAMETER;
	}
	//Check structure consistency
	if (PARSER_PEDANTIC_CHECK)
	{
		if ( ((packet -> arg_len == 0) && (packet -> arg != NULL)) || ((packet -> arg_len != 0) && (packet -> arg == NULL)) )
		{
			parser_error_code = PARSER_ERROR_INCONSISTENT_VALUES;
			DRETURN_ARG("ERR%d: PARSER_ERROR_INCONSISTENT_VALUES\n", parser_error_code);
			return PARSER_ERROR_INCONSISTENT_VALUES;
		}
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

		///Fetch variabiles
	//Number of arguments
	arg_num = (packet -> arg_num);
	//length of argument vector
	arg_len = (packet -> arg_len);
	DPRINT("arg_num: %d, arg_len: %d\n", packet -> arg_num, packet -> arg_len);
	//old arg vector
	u8p = packet -> arg;
		///Calculate Parameters
	//Calculate number of arguments
	new_arg_num =  arg_num +1;
	//If: 1Byte aregument
	if (IS_1B_ARG(arg_descriptor))
	{
		//1 byte for arg_descriptor, 1 byte for argument
		new_arg_len = arg_len +1 +1;
	}
	//If: 2Byte aregument
	else //if (IS_2B_ARG(arg_descriptor))
	{
		//1 byte for arg_descriptor, 2 byte for argument
		new_arg_len = arg_len +1 +2;
	}
		///Allocate Memory
	//if: packet arg was empty before
	if (u8p == NULL)
	{
		u8p = (U8 *)MALLOC( sizeof(U8) * new_arg_len );
		DPRINT("MALLOC: %d bytes | adr: %p\n", sizeof(U8) * new_arg_len, (void *)u8p);
	}
	//if: packet arg was NOT empty before
	else
	{
		u8p = (U8 *)REALLOC( u8p, sizeof(U8) *new_arg_len );
		DPRINT("REALLOC: old adr: %p, %d bytes | adr: %p\n", (void *)u8p, sizeof(U8) * new_arg_len, (void *)u8p);
	}
	//If: malloc fail
	if (u8p == NULL)
	{
		//Signal error code
		parser_error_code = PARSER_ERROR_MALLOC_FAIL;
		DRETURN_ARG("ERR%d: PARSER_ERROR_MALLOC_FAIL\n", parser_error_code);
		//Return with error
		return PARSER_ERROR_MALLOC_FAIL;
	}
		///Reorder Old Memory
	//if: packet arg was NOT empty before
	if (arg_num > 0)
	{
		//shift content by 1Byte from the right place. Make room for the new arg_descriptor
		//from [arglen-1, arg_num] to [arg_len, new_arg_num]
		for (t = arg_len;t > arg_num;t--)
		{
			DPRINT("Transfer %d to %d\n", t -1, t);
			//Make room for arg_descriptor
			u8p[ t ] = u8p[ t -1 ];
		}
	}
		///Write New Memory
	//Write new arg_descriptor
	u8p[ arg_num ] = arg_descriptor;
	//Initialize new argument
	for (t = arg_len +1;t < new_arg_len;t++)
	{
		u8p[t] = (U8)0x00;
	}
	//Save new parameters
	packet -> arg_num	= new_arg_num;
	packet -> arg_len	= new_arg_len;
	packet -> arg		= u8p;
	DPRINT("arg_num: %d, arg_len: %d\n", packet -> arg_num, packet -> arg_len);

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN();
	return PARSER_OK;
}	//end function:	packet_add_arg

/****************************************************************************
**	packet_calc_arg_index
*****************************************************************************
**	PARAMETER:
**		Packet *packet		: address of packet structure
**		U8 index			: index of the argument the user wants to fetch
**	RETURN:
**		index of the argument inside argument vector
**		0xff -> there was an error. error code is stored in parser_error_code
**	DESCRIPTION:
**	calculate index of an argument inside a packet
**	 there is a bug using FOR. with index 0 i must not enter it. for always execute at least once
****************************************************************************/

inline U8 packet_calc_arg_index( Parser_packet *packet, U8 index )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//temp vars
	U8 t;		//temp counter
	U8 u8t;
	//index of an argument inside argument vector
	U8 arg_index;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("adr: %p, index: %d\n",(void *)packet, (U8)index);
	//if: null pointer packet
	//if: null pointer arg vector
	//if: user index is greater than number of arguments
	if ((packet == NULL) || (packet -> arg == NULL) || (index >= packet -> arg_num))
	{
		//Set error code
		parser_error_code = PARSER_ERROR_BAD_PARAMETERS;
		//return MAX
		return MAX_U8;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//offset is where data starts is the number of arguments (1B for each arg_descriptor entry)
	arg_index = packet -> arg_num;
	//While init
	t = 0;
	//For: scan arg_descriptor entries
	while (t < index)
	{
		//fetch arg_descriptor of argument[t]
		u8t = packet -> arg[t];
		DPRINT("arg%d, arg_descriptor: %c: ", t, u8t);

		//if: 1B descriptor
		if (IS_1B_ARG(u8t))
		{
			DPRINT("1B\n");
			arg_index += 1;
		}
		//if: 2B descriptor
		else if (IS_2B_ARG(u8t))
		{
			DPRINT("2B\n");
			arg_index += 2;
		}
		//If: arg_descriptor is not valid
		else
		{
			//Set error code
			parser_error_code = PARSER_ERROR_INVALID_ARG_DESCRIPTOR;
			DRETURN_ARG("ERR%d: PARSER_ERROR_INVALID_ARG_DESCRIPTOR\n", parser_error_code);
			//return MAX
			return MAX_U8;
		}
		//next
		t++;
	}	//End For: scan arg_descriptor entries
	DPRINT("arg_index: %d\n", arg_index);
	//Out of range argument index
	if (arg_index >= packet -> arg_len)
	{
		//happen if descriptors are wrong
		//Set error code
		parser_error_code = PARSER_ERROR_INCONSISTENT_VALUES;
		DRETURN_ARG("ERR%d: PARSER_ERROR_INCONSISTENT_VALUES\n", parser_error_code);
		//return MAX
		return MAX_U8;
	}

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN_ARG("arg_index: %d\n", arg_index);
	return arg_index;
}	//end function:	packet_calc_arg_index

/****************************************************************************
**	packet_get_u8
*****************************************************************************
**	PARAMETER:
**		Packet *packet		: address of packet structure
**		U8 index			: index of the argument the user wants to fetch
**	RETURN:
**		value of the argument
**		MAX_U8 if error, if MAX_U8 check parser_error_code for info. PARSER_OK mean the argument really OK
**	DESCRIPTION:
**	Safely get an U8 argument from a packet structure
****************************************************************************/

U8 packet_get_u8( Parser_packet *packet, U8 index )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//index to the argument vector
	U8 arg_index;
	//return value
	U8 ret;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DPRINT(">packet_get_u8. adr: %x, index: %d\n", (U32)packet, index );

	//if: var type is not U8
	if (packet -> arg[index] != 'u')
	{
		//Set error code
		parser_error_code = PARSER_ERROR_BAD_PARAMETERS;
		//return MAX
		return MAX_U8;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//Calculate arg_index
	arg_index = packet_calc_arg_index( packet, index );
	//If: arg_index failed
	if (arg_index == MAX_U8)
	{
		//packet_error_code hold the error code
		//Return MAX_U8, it can be legit, the user has to check error code to see if it is
		return MAX_U8;
	}
	//Fetch argument
	ret = packet -> arg[ arg_index ];

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DPRINT(">END packet_get_u8. ret: %u\n", ret);
	return ret;
}	//end function:	packet_get_u8

/****************************************************************************
**	packet_get_s8
*****************************************************************************
**	PARAMETER:
**		Packet *packet		: address of packet structure
**		U8 index			: index of the argument the user wants to fetch
**	RETURN:
**		value of the argument
**		MAX_U8 if error, if MAX_S8 check parser_error_code for info. PARSER_OK mean the argument really OK
**	DESCRIPTION:
**	Safely get an S8 argument from a packet structure
****************************************************************************/

S8 packet_get_s8( Parser_packet *packet, U8 index )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//index to the argument vector
	U8 arg_index;
	//return value
	S8 ret;

	U8 *u8p;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("adr: %p, index: %d\n", (void *)packet, index );

	//if: var type is not U8
	if (packet -> arg[index] != 'd')
	{
		//Set error code
		parser_error_code = PARSER_ERROR_BAD_PARAMETERS;
		//return MAX
		return MAX_S8;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//Calculate arg_index
	arg_index = packet_calc_arg_index( packet, index );
	//If: arg_index failed
	if (arg_index == MAX_U8)
	{
		//packet_error_code hold the error code
		//Return MAX_U8, it can be legit, the user has to check error code to see if it is
		return MAX_S8;
	}
	//Fetch argument

	u8p = &(packet -> arg[ arg_index ]);
	ret = (S8)((U8)u8p[0]);

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN_ARG("ret: %u\n", ret);
	return ret;
}	//end function:	packet_get_s8

/****************************************************************************
**	packet_get_u16
*****************************************************************************
**	PARAMETER:
**		Packet *packet		: address of packet structure
**		U8 index			: index of the argument the user wants to fetch
**	RETURN:
**		value of the argument
**		MAX_U16 if error, if MAX_U16 check parser_error_code for info. PARSER_OK mean the argument really OK
**	DESCRIPTION:
**	Safely get an U16 argument from a packet structure
****************************************************************************/

U16 packet_get_u16( Parser_packet *packet, U8 index )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//index to the argument vector
	U8 arg_index;
	//return value
	U16 ret;
	//temp pointer
	U16 *u16p;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DPRINT(">packet_get_u16. adr: %x, index: %d\n", (U32)packet, index );

	//if: var type is not U8
	if (packet -> arg[index] != 'U')
	{
		//Set error code
		parser_error_code = PARSER_ERROR_BAD_PARAMETERS;
		//return MAX
		return MAX_U16;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//Calculate arg_index
	arg_index = packet_calc_arg_index( packet, index );
	//If: arg_index failed
	if (arg_index == MAX_U16)
	{
		//packet_error_code hold the error code
		//Return MAX_U16, it can be legit, the user has to check error code to see if it is
		return MAX_U16;
	}
	//link to an U16 *
	u16p = (U16 *)(&(packet -> arg[ arg_index ]));
	//Fetch argument
	ret = u16p[ 0 ];

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DPRINT(">END packet_get_u16. ret: %u\n", ret);
	return ret;
}	//end function:	packet_get_u16

/****************************************************************************
**	packet_get_s16
*****************************************************************************
**	PARAMETER:
**		Packet *packet		: address of packet structure
**		U8 index			: index of the argument the user wants to fetch
**	RETURN:
**		value of the argument
**		MAX_U8 if error, if MAX_S16 check parser_error_code for info. PARSER_OK mean the argument really OK
**	DESCRIPTION:
**	Safely get an S16 argument from a packet structure
****************************************************************************/

S16 packet_get_s16( Parser_packet *packet, U8 index )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//index to the argument vector
	U8 arg_index;
	//return value
	S16 ret;
	//temp pointer
	S16 *s16p;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DPRINT(">packet_get_s16. adr: %x, index: %d\n", (U32)packet, index );

	//if: var type is not U8
	if (packet -> arg[index] != 'D')
	{
		//Set error code
		parser_error_code = PARSER_ERROR_BAD_PARAMETERS;
		//return MAX
		return MAX_S16;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//Calculate arg_index
	arg_index = packet_calc_arg_index( packet, index );
	//If: arg_index failed
	if (arg_index == MAX_U8)
	{
		//packet_error_code hold the error code
		//Return MAX_U8, it can be legit, the user has to check error code to see if it is
		return MAX_S16;
	}
	//link to an S16 *
	s16p = (S16 *)(&(packet -> arg[ arg_index ]));
	//Fetch argument
	ret = s16p[ 0 ];

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DPRINT(">END packet_get_s16. ret: %u\n", ret);
	return ret;
}	//end function:	packet_get_s16

/****************************************************************************
**	parser_cmd_check
*****************************************************************************
**	PARAMETER:
**		U8 *cmd:	Link to a dictionary
**	RETURN:
**		Parser_error_codes
**		Return the error code according to the given enmu in the .h
**		PARSER_OK mean that the parser is good
**	DESCRIPTION:
**	Test the correctness of a dictionary
****************************************************************************/

Parser_error_codes parser_cmd_chk( U8 *cmd )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//index to the dictionary
	U8 index;
	//Working flags
	U8 flags;
	U8 old;
	U8 data;
	//Parser FSM status variabile (enum on .h)
	Parser_status_word status;
	//Return variabile (enum on .h)
	Parser_error_codes ret = PARSER_OK;

	//store the index to the ID of current command
	U8 cmd_index;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	//init flags (remove warning)
	flags = 0x00;
	//If: bad pointer
	if (cmd == NULL)
	{
		//ERR: null pointer parameter
		return PARSER_EXE_ERR;
	}

	DENTER_ARG("dictionary: %p\n", (U8 *)cmd);

	status.cnt = PARSER_IDLE;

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//While: Init
	cmd_index = 0xff;		//initialize to bad value
	old = cmd[0];
	index = 0;
	SET_BIT( flags, 0 );
	//While: I'm not done scanning the dictionary
	while ((index != 255) && (IS_BIT_ONE(flags, 0)))
	{
		//fetch
		data = cmd[index];
		//If: I'm IDLE. I'm waiting for a command ID
		if (status.cnt == PARSER_IDLE)
		{
			//if: I catch a terminator while in IDLE: dictionary end
			if (data == '\0')
			{
				//End While
				CLEAR_BIT( flags, 0 );
			}
			//Only forbidden IDs are terminator and 0xff
			else if (data == (U8)0xff)
			{
				//Save error code
				ret = PARSER_ERROR_BAD_CMD_ID;
				//End While
				CLEAR_BIT( flags, 0 );
				DPRINT("\tPARSER_ERROR_BAD_CMD_ID! in position: %d\n", index);
			}
			//store index to ID of current command
			cmd_index = index;
			//I got an ID, decoding command
			status.cnt = PARSER_ID;
		}
		//If: I decoded a command ID. I'm decoding the fixed part of a command
		else if (status.cnt == PARSER_ID)
		{
			//If: first byte of the fixed part
			if ((index -cmd_index) == 1)
			{
				//If: is a letter
				if (IS_LETTER( data ))
				{
					//OK: first byte of the fixed part of a command can only be a letter
				}
				else
				{
					//Save error code
					ret = PARSER_ERROR_SYNTAX_FIRST_BYTE;
					//End While
					CLEAR_BIT( flags, 0 );
					DPRINT("\tPARSER_ERROR_SYNTAX_FIRST_BYTE! in position: %d\n", index);
				}
			}
			//if: I'm decoding an argument identifier
			else if (data == '%')
			{
				//jump to decode argument identifier
				status.cnt = PARSER_ARG;
			}
			//if: I'm decoding fixed strings
			else if (IS_NUMBER(data) || IS_LETTER(data))
			{

			}
			//if: command terminator
			else if (data == '\0')
			{
				//This command is done, jump to idle
				status.cnt = PARSER_IDLE;
			}
			//If: something else
			else
			{
				//all other charaters are forbidden
				//Save error code
				ret = PARSER_ERROR_INVALID_CHAR;
				//End While
				CLEAR_BIT( flags, 0 );
				DPRINT("\tPARSER_ERROR_INVALID_CHAR! in position: %d\n", index);
			}
		}	//End if: PARSER_ID
		//If: I decoded a %, I expect an argument descriptor
		else if (status.cnt == PARSER_ARG)
		{
			//If: i found an argument descriptor char
			if (IS_ARG_DESCRIPTOR(data))
			{
				//everything is fine
				//jump back
				status.cnt = PARSER_ID;
			}
			else
			{
				//I don't have a valid argument descriptor
				//Save error code
				ret = PARSER_ERROR_INVALID_ARG_DESCRIPTOR;
				//End While
				CLEAR_BIT( flags, 0 );
				DPRINT("\tPARSER_ERROR_INVALID_ARG_DESCRIPTOR! in position: %d\n", index);
			}
		}	//End if: PARSER_ARG
		//Default case
		else
		{
			//Save error code
			ret = PARSER_ERROR_UNDEFINED_STATE;
			CLEAR_BIT( flags, 0 );
			DPRINT("\tPARSER_ERROR_UNDEFINED_STATE! in position: %d\n", index);
		}

		//Ensure that I always catch a terminator. Is still an error
		//If: double terminator
		if (IS_BIT_ONE(flags, 0) && (data == '\0') && (old == '\0'))
		{
			//dictionary terminator
			//i'm done
			ret = PARSER_ERROR_MISPLACED_END;
			CLEAR_BIT( flags, 0 );
			DPRINT("\tPARSER_ERROR_MISPLACED_END! in position: %d\n", index);
		} //End If: double terminator
		//Update old
		old = data;
		//next entry
		index++;
	}	//End While: I'm not done scanning the dictionary
	//If: dictionary overflowed
	if (index == 255)
	{
		ret = PARSER_ERROR_MAX_LENGTH_EXCEEDED;
		DPRINT("\tPARSER_ERROR_MAX_LENGTH_EXCEEDED! in position: %d\n", index);
	}

	DPRINT("scanned %d char in the dictionary\n", index);

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN();
	return ret;
}	//end function: parser_cmd_check

/****************************************************************************
**	parser_search_first
*****************************************************************************
**	PARAMETER:
**		U8 *cmd		: Link to the dictionary
**		U8 data		: data to be processed
**		U8 *num_hit	: Link to the var where to store the number of hit in th dictionary
**	RETURN:
**		U8 *		: link to the vector holding the indexes to the IDs in dictionary that were hits
**	DESCRIPTION:
**	Auxiliary function of parser exe.
**	Found the list of commands that starts with the given byte
**	Example:
**	cmd: 3,"hi",4,"hello",10,"mine%u"
**	data = 'h'
**	RET: num_hit[0] <- 2
**		cmd:	3,"hi",4,"hello",10,"mine%u"
**	Id list:	^      ^
**	Id list contain the indexes of commands with 'h' in first position
****************************************************************************/

U8 *parser_search_first( U8 *cmd, U8 data, U8 *num_hit )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//working flags
	U8 flags		= 0x00;
	//number of hit
	U8 hit_cnt		= 0;
	//will contain the vector of positions of the IDs that were hits in the dictionary
	U8 *ret			= NULL;
	//index to the dictionary
	U8 index		= 0;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("cmd: %p, data: %x, num_hit: %p\n", (void *)cmd, (U8)data, (void *)num_hit);

	//If: dictionary is not linked (should not happen, preceeding function checked it)
	//if: a bad num_hit var pointer was given
	if ( ((PARSER_PEDANTIC_CHECK) && (cmd == NULL)) || (num_hit == NULL))
	{
		DRETURN_ARG("ERR: PARSER_ERROR_NULL_POINTER_PARAMETER!\n");
		return NULL;
	}

	//If: given byte is not a letter
	if (!IS_LETTER( data ))
	{
		DRETURN_ARG("INFO: not processing character because it won't hit anyway\n");
		//I'm sure there will be no matches. Commands starts with a letter.
		num_hit[0] = 0;
		return NULL;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	//I do two passes. On first pass I calculate the number of hits. on second pass I fill the vector if needed

		///Outer cycle. first pass: count hits. second pass: allocate vector and store hits (optional)
	//While init
	SET_BIT( flags, 0 );
	//While: pass cycle
	while (IS_BIT_ONE( flags, 0 ))
	{
			///Mid cycle. Check the dictionary for hits
		//While init
		SET_BIT( flags, 1 );
		//While: searching hits in the dictionary
		while (IS_BIT_ONE( flags, 1 ))
		{
			DPRINT("index: %d, data: %x\n", index, cmd[index]);
			if ((PARSER_PEDANTIC_CHECK) && (index == 255))
			{
				//infinite cycle detected
				DPRINT("ERR: PARSER_ERROR_MAX_LENGTH_EXCEEDED!\n");
				//break mid cycle
				CLEAR_BIT( flags, 1 );
			}
			//Here, index point to an ID. Skip.
			index++;
			//if: i have an hit
			if (data == cmd[index])
			{
				//If: vector is linked (second pass)
				if (ret != NULL)
				{
					ret[ hit_cnt ] = index -1;
				}
				//Update number of hits
				hit_cnt++;
			}
			//default case
			else
			{
				//It's a miss, do nothing.
			}
				///Inner cycle: skip useless parts of a command
			//do not check for overflow or bad termination. I checked dictionary before
			//If: index != '0' skip until a '\0' is found
			while ((cmd[ index ] != '\0') && (index < 255))
			{
				//Skip entry
				index++;
			}
			DPRINT("skipped until terminator: index: %d, cmd: %x\n", index, cmd[index]);
			//Here i point to a terminator, next i have the ID. Skip
			index++;
			//If what I found after is another terminator, the dictionary is over
			if (cmd[index] == '\0')
			{
				//break mid cycle
				CLEAR_BIT( flags, 1 );
				DPRINT("double terminator found. Dictionary end\n");
			}
		}	//End While: searching for hits in the dictionary

		//if: error. infinite cycle detected
		if ((PARSER_PEDANTIC_CHECK) && (index == 255))
		{
			//I'm done
			CLEAR_BIT( flags, 0 );
			//infinite cycle detected
			DPRINT("PARSER_ERROR_MAX_LENGTH_EXCEEDED!\n");
		}
		//If: i found nothing after first pass, or I already filled the vector on second pass
		else if ( (hit_cnt == 0) || ((hit_cnt > 0) && (ret != NULL)) )
		{
			//I'm done
			CLEAR_BIT( flags, 0 );
			//Write back the hit counter
			num_hit[ 0 ] = hit_cnt;
		}
		//if: i found hits but the vector is not yet allocated
		else if ((hit_cnt > 0) && (ret == NULL))
		{
			//allocate vector
			ret = (U8 *)MALLOC( sizeof( U8 )* hit_cnt);
			DPRINT("MALLOC: %d bytes | adr: %p\n", sizeof( U8 )* hit_cnt, (void *)ret);
			//if: malloc fail
			if (ret == NULL)
			{
				DRETURN_ARG("ERR: PARSER_ERROR_MALLOC_FAIL!\n");
				return NULL;
			}
			//Clear hit counter for second pass
			hit_cnt	= 0;
			//Reset index
			index 	= 0;
		}
		//Default case
		else
		{
			//algorithmic error in the FSM
			DRETURN_ARG("ERR: PARSER_ERROR_UNDEFINED_STATE!\n");
			return NULL;
		}
	}	//End While: pass cycle

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN_ARG("num_hit: %d %d, ret: %p\n", hit_cnt, num_hit[0], (void *)ret);
	return ret;
}	//end function: parser_search_first

/****************************************************************************
**	parser_calc_arg
*****************************************************************************
**	PARAMETER:
**	RETURN:
**	DESCRIPTION:
**	This function will perform arg = 10*arg+num. return 0x00: OK, 0xff: Overflows
**	There are lots of nuances in doing this operation. I do it in a function because of this
**	>Type Cast and use U8 vector as number
**	>Overflow Detection
****************************************************************************/

Parser_error_codes parser_calc_arg( U8 *arg, U8 arg_descriptor, S8 num )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	S8 *s8p;
	U8 *u8p;
	S16 *s16p;
	U16 *u16p;

	S8 s8t;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG("arg adr: %p, arg_descriptor: %x, chiper: %d\n", (void *)arg, arg_descriptor, num );
	//If: null pointer parameter
	if ((PARSER_PEDANTIC_CHECK) && (arg == NULL))
	{
		DRETURN_ARG("ERR: PARSER_ERROR_NULL_POINTER_PARAMETER!\n");
		return PARSER_ERROR_NULL_POINTER_PARAMETER;
	}
	//If: invalid agrument descriptor
	if ((PARSER_PEDANTIC_CHECK) && (!IS_ARG_DESCRIPTOR(arg_descriptor)))
	{
		DRETURN_ARG("ERR: PARSER_ERROR_INVALID_ARG_DESCRIPTOR!\n");
		return PARSER_ERROR_INVALID_ARG_DESCRIPTOR;
	}
	//If: chipher out of range
	if ((PARSER_PEDANTIC_CHECK) && ((num > (S8)9) || (num < (S8)-9)) )
	{
		DRETURN_ARG("ERR: PARSER_ERROR_INVALID_ARG_DESCRIPTOR!\n");
		//actually it's an algorithmic error. A bad one at that
		return PARSER_ERROR_INVALID_ARG_DESCRIPTOR;
	}

	///--------------------------------------------------------------------------
	///	BODY
	///--------------------------------------------------------------------------

	if ( arg_descriptor == 'u' )
	{
		//link pointer
		u8p = (U8 *)arg;
		DPRINT("arg: %d,",(U8)u8p[0]);
			///overflow detection
		//if
		if (u8p[0] > ((U8)(+255/10)))
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		u8p[0] = 10*u8p[0];
		//if
		if (u8p[0] > ((U8)(+255)-num))
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		u8p[0] = u8p[0] +num;
		DPRINT_NOTAB("new arg: %d\n",(U8)u8p[0]);
	}
	else if ( arg_descriptor == 'd' )
	{
		//link pointer
		s8p = (S8 *)arg;
		//fetch var
		s8t = s8p[0];
		DPRINT("arg: %d,",(S8)s8p[0]);
			///overflow detection
		//if s8p[0] is positive, num will be positive. dual for negative
		if ( ((s8t > 0) && (s8t > ((S8)(+127/10)))) || ((s8t < 0) && (s8t < ((S8)(-128/10)))) )
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		s8t = 10*s8t;
		//if s8p[0] is positive, num will be positive. dual for negative
		if ( ((s8t > 0) && (s8t > ((S8)(+127)-num))) || ((s8t < 0) && (s8t < ((S8)(-128)-num))) )
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		s8t = s8t +num;
		//write back
		s8p[0] = s8t;
		DPRINT_NOTAB("new arg: %d\n",(S8)s8p[0]);
	}
	else if ( arg_descriptor == 'U' )
	{
		//link pointer
		u16p = (U16 *)arg;
		DPRINT("arg: %u,",(U16)u16p[0]);
			///overflow detection
		//if arg*10 will overflow?
		if (u16p[0] > (MAX_U16/10))
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		u16p[0] = 10*u16p[0];
		//if arg+num will overflow?
		if (u16p[0] > (MAX_U16-num))
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		u16p[0] = u16p[0] +num;
		DPRINT_NOTAB("new arg: %d\n",(U16)u16p[0]);
	}
	else //if ( arg_descriptor == 'D' )
	{
		//link pointer
		s16p = (S16 *)arg;
		DPRINT("arg: %d,",(S16)s16p[0]);
			///overflow detection
		//if
		if ( ((s16p[0] > 0) && (s16p[0] > (MAX_S16/10))) || ((s16p[0] < 0) && (s16p[0] < (MIN_S16/10))) )
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		s16p[0] = 10*s16p[0];
		//if
		if ( ((s16p[0] > 0) && (s16p[0] > (MAX_S16-num))) || ((s16p[0] < 0) && (s16p[0] < (MIN_S16-num))) )
		{
			//Overflow
			DRETURN_ARG("ERR: PARSER_ERROR_ARG_OVERFLOW!\n");
			return PARSER_ERROR_ARG_OVERFLOW;
		}
		//perform operation
		s16p[0] = s16p[0] +num;
		DPRINT_NOTAB("new arg: %d\n",(S16)s16p[0]);
	}

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	DRETURN();

	return PARSER_OK;
}	//end function: parser_calc_arg

/****************************************************************************
**	PARSER EXE
*****************************************************************************
**	PARAMETER
**		Parser : parser structure, contain the dictionary, the status and the arguments
**		U8		: byte to be decoded
**	RETURN:
**		A Parser Packet structure, containing the ID of the decoded packet,
**		the number of and the list of the arguments and
**		the length of the argument vector and the argument vector
**	DESCRIPTION:
**	Process an incoming byte against a parser.
**	I return a structure for the argument, the uC was having problem and corrupting
**	the memory at access. don't know why.
****************************************************************************/

Parser_packet *parser_exe( Parser *parser, U8 din )
{
	///--------------------------------------------------------------------------
	///	STATIC VARIABILE
	///--------------------------------------------------------------------------

	///--------------------------------------------------------------------------
	///	LOCAL VARIABILE
	///--------------------------------------------------------------------------

	//Temp Vars
	U8 t;			//Counters
	U8 u8t, u8t1;	//Temp U8
	U8 *u8p;		//Temp U8 *
	S8 s8t;
	//Store the link to the dictionary
	U8 *cmd;
	//working flags
	//	7:	'1' -> RESET FSM
	//	6:	'1' -> Full Valid Command has been decoded
	//	5:	'1' -> Add arg
	//	4:	in status arg, '1' i have processed at least one sign/number. Used to detect back to back arguments
	U8 flags;
	//Return value. 0x00 idling, 0xff reset, others: ID of command decoded
	Parser_packet *ret	= NULL;
	//Store error codes of FSM. Handled in FSM post processing
	Parser_error_codes error_code = PARSER_OK;
	//I move copy the input structrue here
	Parser parser_tmp;

	///--------------------------------------------------------------------------
	///	CHECK AND INITIALIZATIONS
	///--------------------------------------------------------------------------

	DENTER_ARG( "parser: %p | ", (void *)parser );

	if (IS_LETTER(din) || IS_NUMBER(din) || IS_SIGN(din))
	{
		DPRINT_NOTAB( "din is a char: >%c<\n" , (U8)din);
	}
	else
	{
		DPRINT_NOTAB( "din: >%x<\n" , (U8)din);
	}
	//if: null pointer parameter
	if (parser == NULL)
	{
		error_code = PARSER_ERROR_NULL_POINTER_PARAMETER;
		//Error
		return NULL;
	}
	//if: check inside the parser structure
	if ( (PARSER_PEDANTIC_CHECK) && (parser -> cmd == NULL) )
	{
		error_code = PARSER_ERROR_NULL_POINTER_PARAMETER;
		//Error
		return NULL;
	}
	//Link the temp pointer to the dictionary
	cmd = parser -> cmd;
	//working flags. individual bits are used for flow control
	flags = (U8)0x00;
	//Store input indirect reference to parser structure to a local parser structure for processing
	parser_tmp = parser[0];
	DPRINT("packet adr: %p\n",(void *)parser_tmp.packet);

	///--------------------------------------------------------------------------
	///	PARSER FINITE STATE MACHINE
	///--------------------------------------------------------------------------

		///--------------------------------------------------------------------------
		///	INPUT TERMINATOR
		///--------------------------------------------------------------------------
		// Close the command. If i have a full match,

	//If: input terminator from user
	if (din == '\0')
	{
		DPRINT("Terminator. Current id_index entries: %d\n", parser_tmp.num_id);
		//If: i was decoding an argument
		if (parser_tmp.status.cnt == PARSER_ARG)
		{
			//I'm done decoding
			parser_tmp.index++;
			DPRINT("Terminator after ARG\n");
		}
		//if: I have at least one parser_tmp.id_index entry
		if (parser_tmp.num_id > 0)
		{
			//For: scan all parser_tmp.id_index entries
			for (t = 0;t<parser_tmp.num_id;t++)
			{
				DPRINT("cmd of entry: %d is %x\n", t, cmd[parser_tmp.id_index[t] +parser_tmp.index]);
				//If: the next command data would be '\0'
				if (cmd[parser_tmp.id_index[t] +parser_tmp.index] == '\0')
				{
						///I have found a valid entry!
					//if the command had no arguments, i have to create one now
					if (parser_tmp.packet == NULL)
					{
						DPRINT("no packet exists. Create a new one\n");
						ret = born_packet();

						if (ret == NULL)
						{
							error_code = PARSER_ERROR_MALLOC_FAIL;
						}
					}
					//if: i have a packet structure (i had arguments)
					else
					{
						DPRINT("Existing packet. command had arguments\n");
						//transfer link to ret
						ret = parser_tmp.packet;
						//clean pointer
						parser_tmp.packet = NULL;
					}
					//save the ID code
					ret -> id = cmd[parser_tmp.id_index[t]];
					DPRINT("Valid Command decoded! ID: %d %d, adr: %x\n", cmd[parser_tmp.id_index[t]], ret -> id, (U32)ret);
					//break cycle
					t = 254;
				}
			}
		}
		//Issue a FSM reset
		SET_BIT( flags, PARSER_FSM_RESET_BIT );
	}	//End If: input terminator from user

		///--------------------------------------------------------------------------
		///	PARSER_IDLE
		///--------------------------------------------------------------------------
		//	I have to decode the first byte
		//	I don0t do dictionary check, thay have been taken care of by calling parser_cmd_chk
		//	>if index == 0 (dictionary start)
		//		skip ID and check fixed char
		//	>skip until a '\0'
		//		skip ID and check fixed char
		//		if a '\0' '\0' I'm done. the command is not in the dictionary

	//If: PARSER_IDLE
	else if (parser_tmp.status.cnt == PARSER_IDLE)
	{
		DPRINT("PARSER_IDLE. parser_tmp.index: %d\n",parser_tmp.index);
		//Search the dictionary for all commands that starts with (data).
		//Load the number of them in num_id
		//return the address of a vector holding the position of the IDs of all hits. (allocated by the function)
		parser_tmp.id_index = parser_search_first( cmd, din, &parser_tmp.num_id );
		//if: the results are inchoerent. no match with vector or match with no vector
		if ( (PARSER_PEDANTIC_CHECK) && ( ((parser_tmp.num_id != 0) && (parser_tmp.id_index == NULL)) || ((parser_tmp.num_id == 0) && (parser_tmp.id_index != NULL)) ))
		{
			//parser_search_first return is inchoerent. wtf?
			error_code = PARSER_ERROR_UNDEFINED_ERROR;
		}
		else if (parser_tmp.num_id == 0)
		{
			//No match was found. Reset the machine
			SET_BIT( flags, PARSER_FSM_RESET_BIT );
		}
		//If: i have a match
		else //if (parser_tmp.num_id > 0)
		{
			DPRINT("At least on valid entry was found! | ");
			//relative displacement from ID, I decoded first byte. next is second
			parser_tmp.index 	= 2;
			///Next Status
			//Here, if in the dictionary i have a %. I do NOT need to check other entries.
			// The dictionary check ensured that no commands can fork between an argument and a fixed char
			if (cmd[ parser_tmp.id_index[0] +parser_tmp.index ] == '%')
			{
				DPRINT_NOTAB("Argument next\n");
				//Issue an argument initialize
				SET_BIT( flags, PARSER_FSM_INIT_ARG_BIT );
				//Next, I expect A number, starting from the sign
				parser_tmp.status.cnt = PARSER_ARG;
				//Issue an argument destroy. (upon exiting status IDLE, old arguments needs to be destroyed. User had his chance to get them)
				SET_BIT( flags, PARSER_FSM_BURY_ARG_BIT );
			}
			//The dictionary has a letter or a number. I do NOT check for error in dictionary
			else
			{
				DPRINT_NOTAB("Another ID next\n");
				//Keep looping in status ID
				parser_tmp.status.cnt = PARSER_ID;
				//Issue an argument destroy. (upon exiting status IDLE, old arguments needs to be destroyed. User had his chance to get them)
				SET_BIT( flags, PARSER_FSM_BURY_ARG_BIT );
			}
		}	//End If: i have a match
	}	//End If: PARSER_IDLE

		///--------------------------------------------------------------------------
		///	PARSER_ID
		///--------------------------------------------------------------------------
		//	Here:
		//	parser_tmp.num_id			| Hold the number of partial matches
		//	parser_tmp.id_index		| Vector of positions of IDs in the dictionary
		//	I can have: letters, signs or number
		//	I have many commands, they can need any combination of

	//if: PARSER_ID
	//if: PARSER_ARG, PARSER_ARG_NEGATIVE && is not a number
	else if ( (parser_tmp.status.cnt == PARSER_ID) || ((parser_tmp.status.cnt == PARSER_ARG) &&(!IS_NUMBER(din)) &&(!IS_SIGN(din))) )
	{
		DPRINT("PARSER_ID. parser_tmp.index: %d\n",parser_tmp.index);
		//If: I was processing an arg, and recieved a non number and got in ID
		if (parser_tmp.status.cnt == PARSER_ARG)
		{
			//advance dictionary index
			parser_tmp.index++;
			//I'm in status ID now
			parser_tmp.status.cnt = PARSER_ID;
		}
		//If: i'm recieving a number or a letter
		if ((IS_LETTER(din)) || (IS_NUMBER(din)))
		{
			DPRINT("data is number or letter: %c\n", din);
				///Prune bad entries
			DPRINT("current id_entries: %d, address: %x\n", parser_tmp.num_id,(U32)parser_tmp.id_index);
			//count the number of surviving id_index entries
			u8t = 0;
			//For: Scan all id_index entries
			for (t = 0; t < parser_tmp.num_id; t++)
			{
				//If: the command char is different from my search key (din)
				if (cmd[ parser_tmp.id_index[t] +parser_tmp.index] != din)
				{
					//mark the id_index entry for deletion (use forbidden position)
					parser_tmp.id_index[ t ] = 255;
				}
				else
				{
					//this id_index entry survived
					u8t++;
				}
			}	//End For: Scan all id_index entries
			//Allocate new id_index vector
			u8p = (U8 *)MALLOC( sizeof(U8) *u8t );
			DPRINT("MALLOC: %d bytes | adr: %p\n", sizeof(U8) *u8t, (void *)u8p);
			//If: malloc fail
			if (u8p == NULL)
			{
				error_code = PARSER_ERROR_MALLOC_FAIL;
				return NULL;
			}
			//index to old vector
			u8t1 = 0;
			//transfer surviving entries into new vector
			//For: Scan old entries
			for (t = 0;t<parser_tmp.num_id;t++)
			{
				//If: the id_index entry is valid
				if (parser_tmp.id_index[t] != 255)
				{
					//transfer entry into new vector
					u8p[ u8t1 ] = parser_tmp.id_index[t];
					//next slot in new vector
					u8t1++;
				}
			}	//End For: Scan old entries
			//Destroy old vector
			DPRINT("FREE: adr: %p\n", (void *)parser_tmp.id_index);
			FREE( parser_tmp.id_index );
			//link new vector
			parser_tmp.id_index = u8p;
			//Clear temp pointer (safety)
			u8p = NULL;
			//save number of surviving entries
			parser_tmp.num_id = u8t;
			DPRINT("surviving id_entries: %d, address: %x\n", parser_tmp.num_id,(U32)parser_tmp.id_index);
			//if: at least one parser_tmp.id_index entry survived the purge
			if (parser_tmp.num_id > 0)
			{
				DPRINT("at least one parser_tmp.id_index entry survived: ");
				//next dictionary entry
				parser_tmp.index++;
					///Next Status
				//Here, if in the dictionary i have a %. I do NOT need to check other entries.
				// The dictionary check ensured that no commands can fork between an argument and a fixed char
				if (cmd[ parser_tmp.id_index[0] +parser_tmp.index ] == '%')
				{
					DPRINT("Argument next\n");
					//Issue an argument initialize
					SET_BIT( flags, PARSER_FSM_INIT_ARG_BIT );
					//Next, I expect A number, starting from the sign
					parser_tmp.status.cnt = PARSER_ARG;
					//I have yet to decode this argument first byte.
					//Solve a bug where ARG, fix, ARG would bug out because this is not initialized
					parser_tmp.status.arg_first = 0;
					//I have to clear argument sign for the same reason
					parser_tmp.status.arg_sign = 0;
				}
				//The dictionary has a letter or a number. I do NOT check for error in dictionary
				else
				{
					DPRINT("Another ID\n");
					//Keep looping in status ID
					parser_tmp.status.cnt = PARSER_ID;
				}
			}	//End If: at least one parser_tmp.id_index entry survived the purge
			else
			{
				DPRINT("no parser_tmp.id_index entries survived the purge\n");
				//Issue an FSM reset
				SET_BIT( flags, PARSER_FSM_RESET_BIT );
			}
		}	//End If: i'm recieving a number or a letter
		//If: invalid char
		else
		{
			error_code = PARSER_ERROR_INVALID_CHAR;
		}
	}	//End if: PARSER_ID

		///--------------------------------------------------------------------------
		///	PARSER_ARG
		///--------------------------------------------------------------------------
		//  Here I'm decoding an argument
		//

	//if: PARSER_ARG
	else if (parser_tmp.status.cnt == PARSER_ARG)
	{
		DPRINT("PARSER_ARG. parser_tmp.index: %d\n",parser_tmp.index);

		//if: packet not allocated
		if ((PARSER_PEDANTIC_CHECK) && (parser_tmp.packet == NULL))
		{
			error_code = PARSER_ERROR_NULL_POINTER_IN_PARSER;
		}
		//Fetch argument descriptor (u, U, d, D)
		u8t = cmd[parser_tmp.id_index[0] +parser_tmp.index];
		//If: the dictionary is wrong
		//If: i have no arguments. (i should have at least one)
		if ((PARSER_PEDANTIC_CHECK) && (!IS_ARG_DESCRIPTOR(u8t)) && ((parser_tmp.packet -> arg_num) == 0))
		{
			//trying to write a negative number on a positive command argument
			error_code = PARSER_ERROR_INVALID_ARG_DESCRIPTOR;
		}
		//fetch argument index
		u8t = parser_tmp.packet -> arg_num -1;
		//fetch index to the argument vector
		u8t1 = packet_calc_arg_index( parser_tmp.packet, u8t );
		DPRINT("arg_index: %d, arg_descriptor: %c, arg_len: %d, arg index: %d, arg: %x\n", u8t,parser_tmp.packet -> arg[ u8t], parser_tmp.packet -> arg_len, u8t1, parser_tmp.packet -> arg[u8t1]);
		//Fetch argument descriptor (u, U, d, D)
		u8t = cmd[parser_tmp.id_index[0] +parser_tmp.index];
		//if: input sign
		if (IS_SIGN(din))
		{
				///Back To Back argument handling
			//If: I already processed signs/numbers but I get another sign
			if (parser_tmp.status.arg_first == 1)
			{
				DPRINT("detected sign during arg decoding\n");
				//This means that the current argument is done, and a new on starts
				//next dictionary entry (should be a '%' sicne i expect an argument)
				parser_tmp.index++;
				//I cannot fork between arguments and command char, i don't need to prune non %

				//Issue an argument initialize
				SET_BIT( flags, PARSER_FSM_INIT_ARG_BIT );
				if (din == '+')
				{
					//Positive number
					parser_tmp.status.arg_sign = 0;
					DPRINT("new positive arg decoding\n");
				}
				else
				{
					//Negative number
					parser_tmp.status.arg_sign = 1;
					DPRINT("new negative arg decoding\n");
				}
			}	//End If: I already processed signs/numbers but I get another sign

			//If: input '-'
			if (din == '-')
			{
				//if: the user try to feed a '-' to a %u or %U argument
				if (IS_UARG(u8t))
				{
					//trying to write a negative number on a positive command argument
					error_code = PARSER_ERROR_NEG_INPUT_POS_ARG;
				}
				//if: negative input on signed arg
				else if (IS_DARG(u8t))
				{
					//Signal that i'm dealing with a negative number
					parser_tmp.status.arg_sign = 1;
				}
			}	//End If: input '-'
			//I processed a sign
			parser_tmp.status.arg_first = 1;
		}	//End if: input sign
		//if: input number
		else if (IS_NUMBER(din))
		{
			//if ((PARSER_PEDANTIC_CHECK) && ((parser_tmp.status.cnt == PARSER_ARG_NEGATIVE) &&(IS_)) )
			//Calculate chiper
			s8t = din -'0';
			//Correct chiper sign
			if (parser_tmp.status.arg_sign == 1)
			{
				s8t = -s8t;
			}
			//Perform arg=arg*10+s8t handling all nuances and detecting overflow
			error_code = parser_calc_arg( &(parser_tmp.packet -> arg[u8t1]), u8t, s8t );
			//If: the operation failed
			if (error_code != PARSER_OK)
			{
				//Issue a FSM Reset
				SET_BIT( flags, PARSER_FSM_RESET_BIT );
			}
			//I processed a number
			parser_tmp.status.arg_first = 1;
		}	//End if: input number
		//not a sign nor a number
		else
		{
			error_code = PARSER_ERROR_UNDEFINED_ERROR;
		}
	}
	//default case
	else
	{
		printf("default case\n");
	}

	///--------------------------------------------------------------------------
	///	FSM: POST PROCESSING
	///--------------------------------------------------------------------------
	//	The FSM rise control signals that perform FSM wide action on the parser structure
	//	I handle them after the FSM code because the signal are rised in many sections,
	//	and this way I handle the signals only once at bottom instead of duplicating the code
	//	inside the FSM.

		///--------------------------------------------------------------------------
		///	Error Codes
		///--------------------------------------------------------------------------
		//	Upon detecting an error
		//	>print in debug mode
		//	>destroy arguments
		//	>reset FSM
	//If: The FSM issued an error code
	if (error_code != PARSER_OK)
	{
		switch (error_code)
		{
			case PARSER_ERROR_BAD_CMD_ID :
				DPRINT("PARSER_ERROR_BAD_CMD_ID\n");
				break;
			case PARSER_ERROR_UNDEFINED_STATE :
				DPRINT("PARSER_ERROR_UNDEFINED_STATE\n");
				break;
			case PARSER_ERROR_INVALID_CHAR :
				DPRINT("PARSER_ERROR_INVALID_CHAR\n");
				break;
			case PARSER_ERROR_INVALID_ARG_DESCRIPTOR :
				DPRINT("PARSER_ERROR_INVALID_ARG_DESCRIPTOR\n");
				break;
			case PARSER_ERROR_MISPLACED_END :
				DPRINT("PARSER_ERROR_MISPLACED_END\n");
				break;
			case PARSER_ERROR_MAX_LENGTH_EXCEEDED :
				DPRINT("PARSER_ERROR_MAX_LENGTH_EXCEEDED\n");
				break;
			case PARSER_ERROR_MALLOC_FAIL :
				DPRINT("PARSER_ERROR_MALLOC_FAIL\n");
				break;
			case PARSER_ERROR_SYNTAX_FIRST_BYTE :
				DPRINT("PARSER_ERROR_SYNTAX_FIRST_BYTE\n");
				break;
			case PARSER_ERROR_NULL_POINTER_PARAMETER :
				DPRINT("PARSER_ERROR_NULL_POINTER_PARAMETER\n");
				break;
			case PARSER_ERROR_NULL_POINTER_IN_PARSER :
				DPRINT("PARSER_ERROR_NULL_POINTER_IN_PARSER\n");
				break;
			case PARSER_ERROR_NEG_INPUT_POS_ARG :
				DPRINT("PARSER_ERROR_NEG_INPUT_POS_ARG\n");
				break;
			case PARSER_ERROR_ARG_OVERFLOW :
				DPRINT("PARSER_ERROR_ARG_OVERFLOW\n");
				break;
			default:
				DPRINT("PARSER_ERROR_UNDEFINED_ERROR error_code: %d\n", error_code);
		}
		//Destroy arguments
		SET_BIT( flags, PARSER_FSM_BURY_ARG_BIT );
		//Reset the FSM
		SET_BIT( flags, PARSER_FSM_RESET_BIT );
		//Return error code
		ret = NULL;
		//Clear error code
		error_code = PARSER_OK;
	}

		///--------------------------------------------------------------------------
		///	ARG VECTOR DESTRUCTION
		///--------------------------------------------------------------------------
		//	Argument are kept after a full command has been decoded to allow user to fetch them
		//	Once the parser_exe is called, and exit from IDLE, I assume that the user is done fetching them
		//	Also this is my last chance to clean the arguments for the current command decoding

	//If: the FSM issued an argument destrucion signal
	if (IS_BIT_ONE(flags, PARSER_FSM_BURY_ARG_BIT))
	{
		DPRINT("Destruction of argument\n");
		//clear signal
		CLEAR_BIT( flags, PARSER_FSM_BURY_ARG_BIT );
		//Free the argument vector
		if (parser_tmp.packet != NULL)
		{
			//destroy packet structure
			bury_packet(parser_tmp.packet);
		}
		//Clean the pointer as well
		parser_tmp.packet = NULL;
	}	//End If: the FSM issued an argument destrucion signal

		///--------------------------------------------------------------------------
		///	ARG VECTOR CREATION/RESIZE/DESTRUCTION
		///--------------------------------------------------------------------------

	//If: The FSM issued an argument initialize:
	if (IS_BIT_ONE(flags, PARSER_FSM_INIT_ARG_BIT))
	{
		DPRINT("Adding a new argument to parser structure\n");
		//Clear flag
		CLEAR_BIT( flags, PARSER_FSM_INIT_ARG_BIT );
		//Skip '%', next I have the argument identifier
		parser_tmp.index++;
		//fetch cmd entry (arg_descriptor)
		u8t1 = cmd[parser_tmp.id_index[0] +parser_tmp.index];
		//if: dictionary is wrong
		if ((PARSER_PEDANTIC_CHECK) && (!IS_ARG_DESCRIPTOR(u8t1)))
		{
			error_code = PARSER_ERROR_INVALID_ARG_DESCRIPTOR;
		}
		//If: no packet structure. this is the first argument
		if (parser_tmp.packet == NULL)
		{
			//create a virgin packet structure
			parser_tmp.packet = born_packet();
			//If: packet creation failed
			if (parser_tmp.packet == NULL)
			{
				//Signal error
				error_code = PARSER_ERROR_MALLOC_FAIL;
			}
		}

		//add an argument to the packet structure. argument get initialized to 0
		packet_add_arg( parser_tmp.packet, u8t1 );

		if (parser_error_code != PARSER_OK)
		{
			error_code = parser_error_code;
		}

	}	//End If: The FSM issued an argument initialize:

		///--------------------------------------------------------------------------
		///	FSM RESET
		///--------------------------------------------------------------------------
		//	An FSM reset will NOT clear the arguments
		//	They are cleaned by default upon exiting idle to allow user to look into them

	//If: a reset was issued
	if (IS_BIT_ONE(flags, PARSER_FSM_RESET_BIT))
	{
		DPRINT("FSM RESET\n");
		//Clear reset flag
		CLEAR_BIT(flags, PARSER_FSM_RESET_BIT);
		//
		parser_tmp.status.cnt 			= PARSER_IDLE;
		parser_tmp.status.arg_sign 		= 0;
		parser_tmp.status.arg_first 	= 0;
		parser_tmp.index 				= 0;
			///Destroy partial matches
		parser_tmp.num_id 				= 0;
		if (parser_tmp.id_index != NULL)
		{
			DPRINT("FREE: adr %p\n", parser_tmp.id_index );
			FREE( parser_tmp.id_index );
			parser_tmp.id_index 			= NULL;
		}
	}	//If: a reset was issued

	///--------------------------------------------------------------------------
	///	WRITE BACK
	///--------------------------------------------------------------------------

	//Write back temp parser structure
	parser[0] = parser_tmp;
	DPRINT("packet adr: %p\n", (void *)parser -> packet);
	DRETURN_ARG("ret: %p\n", (void *)ret);

	///--------------------------------------------------------------------------
	///	RETURN
	///--------------------------------------------------------------------------

	return ret;
}	//end function: parser_exe

