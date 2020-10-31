#define F_CPU 8000000L

// PORT A
#define SET   5	 // Set Red(X) or Green(O) button
#define RIGHT 6	 // Button to move cursor one unit right
#define LEFT  7	 // Button to move cursor one unit left

// PORT B
#define PLAYER_2_B 0  // Player 2 indication
#define PLAYER_2_G 1  // Red is for loss, Green is for win
#define PLAYER_2_R 2  // Blue is for a tie
#define PLAYER_1_B 3  // Player 1 indication
#define PLAYER_1_G 4  // Red is for loss, Green is for win
#define PLAYER_1_R 5  // Blue is for a tie

// PORT C
#define COL_R_0 0  // Red(X) column 0
#define COL_G_0 1  // Green(O) column 0
#define COL_R_1 2  // Red(X) column 1
#define COL_G_1 3  // Green(O) column 1
#define COL_R_2 4  // Red(X) column 2
#define COL_G_2 5  // Green(O) column 2

// PORT D
#define COL_B_0 0  // Blue column 0
#define COL_B_1 1  // Blue column 1
#define COL_B_2 2  // Blue column 2
#define ROW_0	5  // Row 0
#define ROW_1	6  // Row 1
#define	ROW_2	7  // Row 2

// Turn queue flag
#define PLAYER_1_TURN 1	 // For Player 1 to take a turn
#define PLAYER_2_TURN 2	 // For Player 1 to take a turn

// Return values for CheckEnd function
#define GO_ON_PLAYING 0	 // Not a tie, no one won, keep playing	flag
#define PLAYER_1_WON  1	 // Player 1 won the game flag
#define PLAYER_2_WON  2	 // Player 2 won the game flag
#define TIE			  3	 // No free cells, a tie flag

// States for one RGB LED
#define OFF   0	 // No color
#define RED   1	 // Red color
#define GREEN 2  // Green color

// XO matrix size
#define ROWS_AMOUNT 3  // Amount of rows in the XO matrix
#define COLS_AMOUNT 3  // Amount of columns in the XO matrix

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

const uint8_t redColumns[COLS_AMOUNT]   = {COL_R_0, COL_R_1, COL_R_2};
const uint8_t greenColumns[COLS_AMOUNT] = {COL_G_0, COL_G_1, COL_G_2};
const uint8_t blueColumns[COLS_AMOUNT]  = {COL_B_0, COL_B_1, COL_B_2};
const uint8_t rows[ROWS_AMOUNT]			= {ROW_0, ROW_1, ROW_2};

// XO matrix structure
struct XOs{
	// States of the matrix`s elements
	// 0 is for Turned off LED
	// 1 is for Red(X) LED
	// 2 is for Green(O) LED
	uint8_t states[ROWS_AMOUNT][COLS_AMOUNT];
	// Current position of the cursor
	uint8_t currentRow;	   // Row position
	uint8_t currentColumn; // Column position
	// Turn queue flag
	// 1 if for Player 1 to take a turn
	// 2 is for Player 2 to take a turn
	uint8_t turnFlag;
	// Dynamic indication flag allows to show the matrix sequentially
	// 0 is for the 0th row to show up
	// 1 is for the 1st row to show up
	// ...
	// N is for the Nth row to show up (we have 3 rows)
	uint8_t dynamicIndicationFlag;
	// Dynamic indication function allows to show the matrix
	// We will use it in the timer
	void DynamicIndication(void){
		PORTD |= ((1 << ROW_0) | (1 << ROW_1) | (1 << ROW_2));
		PORTC &= ~((1 << COL_R_0) | (1 << COL_R_1) | (1 << COL_R_2) | (1 << COL_G_0) | (1 << COL_G_1) | (1 << COL_G_2));
		PORTD &= ~((1 << COL_B_0) | (1 << COL_B_1) | (1 << COL_B_2));
		for (uint8_t i = 0; i < COLS_AMOUNT; i++){
			if (states[dynamicIndicationFlag][i] == OFF){
				// Nothing
			}
			else if (states[dynamicIndicationFlag][i] == RED){
				PORTC |= (1 << redColumns[i]);
			}
			else if (states[dynamicIndicationFlag][i] == GREEN){
				PORTC |= (1 << greenColumns[i]);
			}
			if ((dynamicIndicationFlag == currentRow) and (i == currentColumn)){
				PORTD |= (1 << blueColumns[i]);
			}
		}
		PORTD &= ~(1 << rows[dynamicIndicationFlag]);
		if (dynamicIndicationFlag == 0){
			dynamicIndicationFlag = 1;
		}
		else if (dynamicIndicationFlag == 1){
			dynamicIndicationFlag = 2;
		}
		else if (dynamicIndicationFlag == 2){
			dynamicIndicationFlag = 0;
		}
	}
	// Resets the cursor to (0;0) position
	void SetInitialPosition(void){
		currentColumn = 0;
		currentRow = 0;
	}
	// Sets the LED to be Red(X) or Green(O), depending on whos turn is at present
	void SetCurrentState(void){
		// Changing the state of the LED where the cursor is to Green(O) or Red(X)
		states[currentRow][currentColumn] = turnFlag;
		// Changing the turn flag, so that another player will have a turn
		if (turnFlag == PLAYER_1_TURN){
			turnFlag = PLAYER_2_TURN;
		}
		else if (turnFlag == PLAYER_2_TURN){
			turnFlag = PLAYER_1_TURN;
		}
		SetInitialPosition();
	}
	// Checks if the game was finished
	// Returns:
	// 0 if	the game is not finished
	// 1 if Player 1 won
	// 2 if Player 2 won
	// 3 if there is a tie
	uint8_t CheckEnd(void){
		// Sum gets the info about the combination:
		// If the winning combination is of 3 cells and Sum is 3 then Player 1 won, if Sum is 6 then Player 2 won
		// If the winning combination is of 4 cells and Sum is 4 then Player 1 won, if Sum is 8 then Player 2 won
		uint8_t sum = 0;
		// Flag, that checks if there are any empty cells in the matrix
		uint8_t anyEmptyCells = 0;
		// Checking if there is a horizontal winning combination
		// x x x x   o o o o   o o o o
		// o o o o	 x x x x   o o o o
		// o o o o	 o o o o   x x x x
		for (uint8_t i = 0; i < ROWS_AMOUNT; i++){
			for (uint8_t j = 0; j < COLS_AMOUNT; j++){
				if (states[i][j] > 0){
					sum += states[i][j];
				}
				else if (states[i][j] == 0){
					sum = 0;
					anyEmptyCells = 1;
					break;
				}
			}
			if (sum == 3){
				PORTB |= (1 << PLAYER_1_G);
				PORTB &= ~(1 << PLAYER_2_R);
				return PLAYER_1_WON;
			}
			else if (sum == 6){
				PORTB |= (1 << PLAYER_2_G);
				PORTB &= ~(1 << PLAYER_1_R);
				return PLAYER_2_WON;
			}
			sum = 0;
		}
		sum = 0;
		// Check if there is a vertical winning combination
		// x o o o   o x o o   o o x o   o o o x
		// x o o o   o x o o   o o x o   o o o x
		// x o o o   o x o o   o o x o   o o o x
		for (uint8_t i = 0; i < COLS_AMOUNT; i++){
			for (uint8_t j = 0; j < ROWS_AMOUNT; j++){
				if (states[j][i] > 0){
					sum += states[j][i];
				}
				else if (states[j][i] == 0){
					sum = 0;
					break;
				}
			}
			if (sum == 3){
				PORTB |= (1 << PLAYER_1_G);
				PORTB &= ~(1 << PLAYER_2_R);
				return PLAYER_1_WON;
			}
			else if (sum == 6){
				PORTB |= (1 << PLAYER_2_G);
				PORTB &= ~(1 << PLAYER_1_R);
				return PLAYER_2_WON;
			}
			sum = 0;
		}
		sum = 0;
		// Check if there is a diagonal winning combination
		// x o o
		// o x o
		// o o x
		if ((states[0][0]) and (states[1][1]) and (states[2][2])){
			if ((states[0][0] + states[1][1] + states[2][2]) == 3){
				PORTB |= (1 << PLAYER_1_G);
				PORTB &= ~(1 << PLAYER_2_R);
				return PLAYER_1_WON;
			}
			else if((states[0][0] + states[1][1] + states[2][2]) == 6){
				PORTB |= (1 << PLAYER_2_G);
				PORTB &= ~(1 << PLAYER_1_R);
				return PLAYER_2_WON;
			}
		}
		if ((states[0][2]) and (states[1][1]) and (states[2][0])){
			if ((states[0][2] + states[1][1] + states[2][0]) == 3){
				PORTB |= (1 << PLAYER_1_G);
				PORTB &= ~(1 << PLAYER_2_R);
				return PLAYER_1_WON;
			}
			else if((states[0][2] + states[1][1] + states[2][0]) == 6){
				PORTB |= (1 << PLAYER_2_G);
				PORTB &= ~(1 << PLAYER_1_R);
				return PLAYER_2_WON;
			}
		}
		// Check if there are any empty cells, if not then it is a tie
		if (anyEmptyCells == 0){
			PORTB |= (1 << PLAYER_1_B) | (1 << PLAYER_2_B);
			return TIE;
		}
		else {
			// nothing
		}
		// if not all cells are filled and no one has won then keep playing
		return GO_ON_PLAYING;
	}
	// Initialization function
	void Initialization(void){
		for (uint8_t i = 0; i < ROWS_AMOUNT; i++){
			for (uint8_t j = 0; j < COLS_AMOUNT; j++){
				states[i][j] = OFF;
			}
		}
		dynamicIndicationFlag = 0;
		turnFlag = PLAYER_1_TURN;
		SetInitialPosition();
	}
};

// XO field struct
XOs XO;
// Ports initialization	function
void PortsInitialization(void);
// Timers initialization function
void TimersInitialization(void);

// Timer 0 overflow interrupt
ISR(TIMER0_COMP_vect){
	// Dynamic indication
	XO.DynamicIndication();
	// Check if someone won and reset the game in that case
	if (XO.CheckEnd() != 0){
		_delay_ms(1000);
		PORTB = 0x00;
		XO.Initialization();
	}
	else {
		// nothing
	}
}

int main(void){
	PortsInitialization();
	TimersInitialization();
	// Set up of the XO field
	XO.Initialization();
	PORTB = 0x00;
	sei();
    while(1){
		// If LEFT button pressed move the cursor
		if (!(PINA & (1 << LEFT))){
			_delay_ms(50);
			if ((PINA & (1 << LEFT))){
				// If the cursor was at (0;0) then move it to (2;3)
				if (XO.currentColumn == 0){
					if (XO.currentRow == 0){
						XO.currentRow = 2;
						XO.currentColumn = 2;
					}
					// If the cursor was at (i;0), i > 0, move it to (i-1;3)
					else if (XO.currentRow != 0){
						XO.currentRow--;
						XO.currentColumn = 2;
					}
				}
				// If the cursor was at (i;j), j > 0, move it to (i;j-1)
				else if (XO.currentColumn != 0){
					XO.currentColumn--;
				}
			}
		}
		// If RIGHT button pressed move the cursor
		if (!(PINA & (1 << RIGHT))){
			_delay_ms(50);
			if ((PINA & (1 << RIGHT))){
				// If the cursor was at (2;3) move it to (0;0)
				if (XO.currentColumn == 2){
					if (XO.currentRow == 2){
						XO.currentColumn = 0;
						XO.currentRow = 0;
					}
					// If the cursor was at (i;3), i < 2, move it to (i+1;0)
					else if (XO.currentRow != 2){
						XO.currentColumn = 0;
						XO.currentRow++;
					}
				}
				// If the cursor was at (i;j), j < 3, move it to (i;j+1)
				else if (XO.currentColumn != 2){
					XO.currentColumn++;
				}
			}
		}
		// If SET button pressed
		if (!(PINA & (1 << SET))){
			_delay_ms(50);
			if (PINA & (1 << SET)){
				// If the position was not set to X or O then set it
				if (XO.states[XO.currentRow][XO.currentColumn] == OFF){
					XO.SetCurrentState();
				}
				else{
					// Nothing
				}
			}
		}
    }
}

void TimersInitialization(void){
	TIMSK |= (1 << OCIE0);
	TCCR0 |= (1 << CS01) | (1 << CS00);
}

void PortsInitialization(void){
	DDRA |= (0 << LEFT) | (0 << RIGHT) | (0 << SET);
	DDRB |= (1 << PLAYER_1_R) | (1 << PLAYER_1_G) | (1 << PLAYER_1_B) | (1 << PLAYER_2_R) | (1 << PLAYER_2_G) | (1 << PLAYER_2_B);
	DDRC |= (1 << COL_R_0) | (1 << COL_R_1) | (1 << COL_R_2) | (1 << COL_G_0) | (1 << COL_G_1) | (1 << COL_G_2);
	DDRD |= (1 << COL_B_0) | (1 << COL_B_1) | (1 << COL_B_2) | (1 << ROW_0) | (1 << ROW_1) | (1 << ROW_2);
}