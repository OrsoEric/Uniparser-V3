#ifndef AT_PARSER_H
	#define AT_PARSER_H

	/**********************************************************************************
	**	ENVIROMENT VARIABILE
	**********************************************************************************/

	//1 -> additional (often redundant) controls are activated
	#define PARSER_PEDANTIC_CHECK	1

	/**********************************************************************************
	**	GLOBAL INCLUDE
	**********************************************************************************/

	#include <stdint.h>

	/**********************************************************************************
	**	DEFINE
	**********************************************************************************/

	//How many bytes are allocated to the parser upon command ID detected
	#define PARSER_BASE_ARG_LEN		16

	//return code of parser exe when an error occour
	#define PARSER_EXE_ERR			PARSER_ERROR_UNDEFINED_ERROR

		///Working flag bit meaning
	//	'1' -> prepare parser arguments to house a new argument
	#define PARSER_FSM_INIT_ARG_BIT		5
	//	'1' -> destroy old arguments. preparation for a new execution
	#define PARSER_FSM_BURY_ARG_BIT		6
	//	'1' -> reset the whole FSM machine
	#define PARSER_FSM_RESET_BIT		7

	/**********************************************************************************
	**	MACRO
	**********************************************************************************/

	//Get a U8 argument from a parser structure
	#define PARSER_GET_U8( parser, index )	\
		((U8)((parser) -> arg[(index)]))
	//Get a S8 argument from a parser structure
	#define PARSER_GET_S8( parser, index )	\
		((S8)((parser) -> arg[(index)]))
	//Get a U16 argument from a parser structure
	#define PARSER_GET_U16( parser, index )	\
		((U16)(((U16*)(&(parser) -> arg[(index)]))[0]))
	//Get a S16 argument from a parser structure
	#define PARSER_GET_S16( parser, index )	\
		((S16)(((S16*)(&(parser) -> arg[(index)]))[0]))

	#define IS_1B_ARG( data )	\
		((data == 'u') || (data == 'd'))

	#define IS_2B_ARG( data )	\
		((data == 'U') || (data == 'D'))

	#define IS_UARG( data )	\
		((data == 'u') || (data == 'U'))

	#define IS_DARG( data )	\
		((data == 'd') || (data == 'D'))

	#define IS_ARG_DESCRIPTOR( data )	\
		((data == 'u') || (data == 'U') || (data == 'd') || (data == 'D'))

	#define MALLOC( size )	\
		malloc( size )

	#define FREE( adr )	\
		free( adr )

	#define REALLOC( adr, new_size )	\
		realloc( adr, new_size )

	/**********************************************************************************
	**	TYPEDEF
	**********************************************************************************/


	typedef struct _Parser Parser;

	typedef struct _Parser_packet Parser_packet;

	/**********************************************************************************
	**	STRUCTURES AND ENUM
	**********************************************************************************/

	//Statuses of the FSM
	enum _Parser_status
	{
		PARSER_IDLE		= 0,						//IDLE, awaiting for ID
		PARSER_ID		= 1,							//ID successfully decoded, decoding command
		PARSER_ARG		= 2							//Process input sign or positive
	};
	typedef enum _Parser_status Parser_status;

	//Error codes for the FSM
	enum _Parser_error_codes
	{
		PARSER_OK,
		PARSER_ERROR_BAD_CMD_ID,				//Happen if use 0 of ff as command ID
		PARSER_ERROR_UNDEFINED_STATE,			//Happen if the parser_cmd_check FSM end up in a default state
		PARSER_ERROR_INVALID_CHAR,				//Happen if i don't use an aplhanumumeric char in the fixed part of a command
		PARSER_ERROR_INVALID_ARG_DESCRIPTOR,	//Happen if after a % you put the wrong argument descriptor
		PARSER_ERROR_MISPLACED_END,				//Happen if a dictionary terminator happen outside of the FSM
		PARSER_ERROR_MAX_LENGTH_EXCEEDED,		//Happen if index is 255 after the check. Could be lack of terminators
		PARSER_ERROR_MALLOC_FAIL,				//Happen whenever at_malloc returned null
		PARSER_ERROR_SYNTAX_FIRST_BYTE,			//Happen when the first byte of the fixed part of a command is NOT a letter
		PARSER_ERROR_NULL_POINTER_PARAMETER,	//Happen if the user give a null pointer as parameter
		PARSER_ERROR_NULL_POINTER_IN_PARSER,	//Happen when a pointer in the parser structure is NULL when it shouldn't
		PARSER_ERROR_NEG_INPUT_POS_ARG,			//Happen whenever the user try to feed a '-' to a %u or %U argument
		PARSER_ERROR_ARG_OVERFLOW,				//Happens when din would overflow arg according the data type arg_descriptor
		PARSER_ERROR_INCONSISTENT_VALUES,		//Happen when two variabiles have inconsistent values (es vector length 0 with nopn NULL address)
		PARSER_ERROR_BAD_PARAMETERS,			//Happen when the user feed bad parameter to a parser function
		PARSER_ERROR_UNDEFINED_ERROR			//Happens if the error code is not in this enum
	};

	typedef enum _Parser_error_codes Parser_error_codes;


	typedef struct _Parser_status_word Parser_status_word;

	struct _Parser_status_word
	{
		Parser_status cnt 		: 2;	//3 bit for the status
		U8 arg_sign				: 1;	//argument sign '0' -> positive / '1' -> negative
		U8 arg_first			: 1;	//'0' -> i have yet to process one byte of the argument
		U8						: 4;	//unused
	};

	//Parser structure
	struct _Parser
	{
		//Pointer to the dictionary
		U8 *cmd;
		//Index relative to ID. Index inside command
		U8 index;
		//Status of the parser
		Parser_status_word status;
		//Partial hits are stored here
		U8 num_id;		//Number of partial hits
		U8 *id_index;	//vector with the position of the IDs inside the dictionary
		//Partially decoded packet. returned when fully decoded
		Parser_packet *packet;
	};

		///--------------------------------------------------------------------------
		///	MEMORY
		///--------------------------------------------------------------------------

	//Structure that fully describe a decoded command
	struct _Parser_packet
	{
		//ID of the command
		U8 id;
		//Number of argument (stored in the first part of arg vector)
		U8 arg_num;
		//Length of the arg vector (total)
		U8 arg_len;
		//firsts [arg_num] bytes hold a list of the arguments, second part hold the value of the arguments
		//	example: arg_num = 3, arg_len=7, arg={d,S,u,(d0),(S1L, S1H),(u2)}
		U8 *arg;
	};

	/**********************************************************************************
	**	PROTOTYPE: GLOBAL VARIABILE
	**********************************************************************************/

	//Last error of the parser
	extern Parser_error_codes parser_error_code;

	/**********************************************************************************
	**	PROTOTYPE: FUNCTION
	**********************************************************************************/

		///--------------------------------------------------------------------------
		///	MEMORY
		///--------------------------------------------------------------------------

	//Create a parser structure and link the dictionary
	extern Parser *born_parser( U8 *cmd );
	//Safely dispose of a parser structure. The link to the dictionary is returned since the dictionary was not allocated by this
	extern U8 *bury_parser( Parser *parser );
	//Create a packet structure with space for a desctiptor. 0x00 will create an empty one
	extern Parser_packet *born_packet( void );
	//Safely dispose of a packet structure
	extern U8 bury_packet( Parser_packet *packet );
	//Add an argument to a packet
	extern U8 packet_add_arg( Parser_packet *packet, U8 arg_descriptor );

		///--------------------------------------------------------------------------
		///	PARSER EXECUTION FUNCTIONS
		///--------------------------------------------------------------------------

	//Test the correctness of a dictionary
	extern Parser_error_codes parser_cmd_chk( U8 *cmd );
	//Auxiliary function of parser exe. Found the list of commands that starts with the given byte
	extern U8 *parser_search_first( U8 *cmd, U8 data, U8 * );
	//Process an incoming byte against a parser. return:
	//	0		: still processing
	//	0xff	: reset after parsing error (soft error)
	//	ID		: anything else is a command ID fully decoded.
	//	Arguments are stored inside parser structure, use macro to get them
	extern Parser_packet *parser_exe( Parser *parser, U8 data );

		///--------------------------------------------------------------------------
		///	PACKET ACCESS
		///--------------------------------------------------------------------------

	//Auxiliary function. calculate index of an argument inside a packet
	extern U8 packet_calc_arg_index( Parser_packet *packet, U8 index );

	//Safely get an U8 argument from a packet structure
	extern U8 packet_get_u8( Parser_packet *packet, U8 index );
	//Safely get an S8 argument from a packet structure
	extern S8 packet_get_s8( Parser_packet *packet, U8 index );
		//Safely get an U16 argument from a packet structure
	extern U16 packet_get_u16( Parser_packet *packet, U8 index );
		//Safely get an S16 argument from a packet structure
	extern S16 packet_get_s16( Parser_packet *packet, U8 index );

#else
	#warning "multiple inclusion of the header file AT_PARSER_H"
#endif
