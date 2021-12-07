// compile with: clang++ -std=c++20 -Wall -Werror -Wextra -Wpedantic -g3 -o main main.cpp
// run with: ./team22-fruits 2> /dev/null or ./team22-fruits 2> debugoutput.txt to avoid disruptive error messages
//  "2>" redirect standard error (STDERR; cerr)
//  /dev/null is a "virtual file" which discard contents

// Works best in Visual Studio Code if you set:
//   Settings -> Features -> Terminal -> Local Echo Latency Threshold = -1

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono> // for dealing with time intervals
#include <cmath> // for max() and min()
#include <termios.h> // to control terminal modes
#include <unistd.h> // for read()
#include <fcntl.h> // to enable / disable non-blocking read()

// Because we are only using #includes from the standard, names shouldn't conflict
using namespace std;

// Constants

// Disable JUST this warning (in case students choose not to use some of these constants)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"

const char NULL_CHAR     { 'z' };
const char LEFT_CHAR     { 'a' }; // won't work if caps lock is on
const char RIGHT_CHAR    { 'd' }; // ^^^
const char QUIT_CHAR     { 'q' };

const string ANSI_START { "\033[" };
const string START_COLOUR_PREFIX {"1;"};
const string START_COLOUR_SUFFIX {"m"};
const string STOP_COLOUR  {"\033[0m"};

const unsigned int COLOUR_IGNORE  { 0 }; // this is a little dangerous but should work out OK
const unsigned int COLOUR_RED     { 31 };
const unsigned int COLOUR_GREEN   { 32 };
const unsigned int COLOUR_WHITE   { 37 };

const int START_ROW        { 1 };
const int END_ROW          { 16 };
const int START_COLUMN     { 1 };
const int MIDDLE_COLUMN    { 17 };
const int END_COLUMN       { 37 };

#pragma clang diagnostic pop

// Globals

std::random_device rd;  // will be used to obtain a seed for the random number engine
std::mt19937 generator(rd()); // standard mersenne_twister_engine seeded with rd()
std::uniform_int_distribution<> lane(1, 7);

struct termios initialTerm;

// Types

// Using signed and not unsigned to avoid having to check for ( 0 - 1 ) being very large

struct position { int row; int col; };

struct score
{
    int score = 0;
    unsigned int colour = COLOUR_WHITE;
};

struct fruit // one was added to the START_ROW and the value one was subtracted  from END-ROW to prevent conflict with the border 
{
    position position {START_ROW + 1, (lane( generator ) * 5) - 1 }; // the value is multiplied by 5 because the lane is 5 characters wide and subtract 1 to center the fruit and the lane 
    unsigned int colour = COLOUR_GREEN;
};

struct basket
{
  position position { END_ROW - 1, MIDDLE_COLUMN };
  unsigned int colour = COLOUR_WHITE;
};

// Utilty Functions

// These two functions are taken from StackExchange and are 
// all of the "magic" in this code.
auto SetupScreenAndInput() -> void
{
    struct termios newTerm;
    // Load the current terminal attributes for STDIN and store them in a global
    tcgetattr(fileno(stdin), &initialTerm);
    newTerm = initialTerm;
    // Mask out terminal echo and enable "noncanonical mode"
    // " ... input is available immediately (without the user having to type 
    // a line-delimiter character), no input processing is performed ..."
    newTerm.c_lflag &= ~ICANON;
    newTerm.c_lflag &= ~ECHO;
    newTerm.c_cc[VMIN] = 1;
 
    // Set the terminal attributes for STDIN immediately
    auto result { tcsetattr(fileno(stdin), TCSANOW, &newTerm) };
    if ( result < 0 ) { cerr << "Error setting terminal attributes [" << result << "]" << endl; }
}
auto TeardownScreenAndInput() -> void
{
    // Reset STDIO to its original settings
    tcsetattr( fileno( stdin ), TCSANOW, &initialTerm );
}
auto SetNonblockingReadState( bool desiredState = true ) -> void
{
    auto currentFlags { fcntl( 0, F_GETFL ) };
    if ( desiredState ) { fcntl( 0, F_SETFL, ( currentFlags | O_NONBLOCK ) ); }
    else { fcntl( 0, F_SETFL, ( currentFlags & ( ~O_NONBLOCK ) ) ); }
    cerr << "SetNonblockingReadState [" << desiredState << "]" << endl;
}
// Everything from here on is based on ANSI codes
// Note the use of "flush" after every write to ensure the screen updates
auto ClearScreen() -> void { cout << ANSI_START << "2J" << flush; }
auto MoveTo( unsigned int x, unsigned int y ) -> void { cout << ANSI_START << x << ";" << y << "H" << flush; }
auto HideCursor() -> void { cout << ANSI_START << "?25l" << flush; }
auto ShowCursor() -> void { cout << ANSI_START << "?25h" << flush; }
auto GetTerminalSize() -> position
{
    // This feels sketchy but is actually about the only way to make this work
    MoveTo(999,999);
    cout << ANSI_START << "6n" << flush ;
    string responseString;
    char currentChar { static_cast<char>( getchar() ) };
    while ( currentChar != 'R')
    {
        responseString += currentChar;
        currentChar = getchar();
    }
    // format is ESC[nnn;mmm ... so remove the first 2 characters + split on ; + convert to unsigned int
    // cerr << responseString << endl;
    responseString.erase(0,2);
    // cerr << responseString << endl;
    auto semicolonLocation = responseString.find(";");
    // cerr << "[" << semicolonLocation << "]" << endl;
    auto rowsString { responseString.substr( 0, semicolonLocation ) };
    auto colsString { responseString.substr( ( semicolonLocation + 1 ), responseString.size() ) };
    // cerr << "[" << rowsString << "][" << colsString << "]" << endl;
    auto rows = stoul( rowsString );
    auto cols = stoul( colsString );
    position returnSize { static_cast<int>(rows), static_cast<int>(cols) };
    // cerr << "[" << returnSize.row << "," << returnSize.col << "]" << endl;
    return returnSize;
}

auto MakeColour( string inputString, 
                 const unsigned int foregroundColour = COLOUR_WHITE,
                 const unsigned int backgroundColour = COLOUR_IGNORE ) -> string
{
    string outputString;
    outputString += ANSI_START;
    outputString += START_COLOUR_PREFIX;
    outputString += to_string( foregroundColour );
    if ( backgroundColour ) 
    { 
        outputString += ";";
        outputString += to_string( ( backgroundColour + 10 ) ); // Tacky but works
    }
    outputString += START_COLOUR_SUFFIX;
    outputString += inputString;
    outputString += STOP_COLOUR;
    return outputString;
}

// Game Logic

auto UpdateFruitPosition( fruit & fruit ) -> void
{
    int rowChange = 1;
    // Ampersand used as a reference so that the actual position updates
    fruit.position.row += rowChange;
}

auto UpdateBasketPosition( basket & basket, char currentChar ) -> void
{
    // Deal with movement commands 
    int commandColChange {0};

    if ( currentChar == LEFT_CHAR )  { commandColChange -= 5; }
    if ( currentChar == RIGHT_CHAR ) { commandColChange += 5; }


    auto proposedCol { basket.position.col + commandColChange };
    basket.position.col = max( START_COLUMN + 1, min( END_COLUMN - 5, proposedCol ) ); // 5 was subtracted from END_COLUMN to ensure the right side of the basket does not go past the border
}

/* Can be used if you want to spawn multiple fruits at a time

typedef vector< fruit > fruitvector;
auto CreateFruit( fruitvector & fruits ) -> void
{
    cerr << "Creating Fruit" << endl;
    fruit newFruit { 
        .position = { .row = START_ROW + 1, .col = (( lane( generator ) ) * 5) - 1 } ,
        .colour = COLOUR_GREEN,
    };
    fruits.push_back( newFruit );
} */

auto DrawFruit( fruit fruit ) -> void
{
    MoveTo( fruit.position.row, fruit.position.col ); 
    cout << MakeColour("O", COLOUR_GREEN) << flush;
}

auto DrawBasket( basket basket ) -> void
{
    MoveTo( basket.position.row, basket.position.col ); 
    cout << "\\___/" << flush; 
}

// The IsCaught function is called when the fruit reaches the bottom row
auto IsCaught( fruit fruit, basket basket ) -> bool
{
    if (fruit.position.col - 2 == basket.position.col) // fruit is centered in the columns while basket is left aligned. 2 is substracted so the positions line up.
    {
    	return true;
    }
    return false;
}

auto ResetFruit( fruit & fruit ) -> void
{
    int newCol {(lane(generator) * 5) - 1};
    fruit.position.col = newCol;
    fruit.position.row = START_ROW + 1;
}

auto DrawBoard() -> void
{
    for ( unsigned int row = START_ROW; row <= END_ROW; row++ ) 
    {
        if ( row == START_ROW || row == END_ROW ) 
        {
            for ( unsigned int column = START_COLUMN; column <= END_COLUMN; column++ ) 
            {
                MoveTo( row, column ); 
                cout << "-" << flush;
            }
        }
        else 
        {
            MoveTo( row, START_COLUMN ); 
            cout << "|" << flush;
            MoveTo( row, END_COLUMN ); 
            cout << "|" << flush;
        }
    }
}

auto DrawScore( score score ) -> void
{
    MoveTo(START_ROW, END_COLUMN + 2); // END_COLUMN + 2 to ensure the score is displayed outside the border
    cout << "score: " << to_string(score.score) << flush;
}

auto DrawGameOver( score score ) -> void
{
    // The values 8 and 14 were chosen for the "GAME OVER :(" message to ensure it is aligned in the middle of the box. The value 16 was chosen for score because it is a shorter word
    MoveTo(8,14);
    cout << MakeColour("GAME OVER :(", COLOUR_RED) << flush;
    MoveTo(9,16);
    cout << "Score: " << to_string(score.score) << flush;
    MoveTo(END_ROW, START_COLUMN); // after the game ends, we don't want to command line spawning in the middle of the game but rather the line after the bottom row of our game
}

auto UpdateScore( score & score ) -> void
{
    score.score++;
}

auto main() -> int
{
    // Set Up the system to receive input
    SetupScreenAndInput();

    // Check that the terminal size is large enough for our fishies
    const position TERMINAL_SIZE { GetTerminalSize() };
    if ( ( TERMINAL_SIZE.row < END_ROW ) or ( TERMINAL_SIZE.col < END_COLUMN + 11 ) ) // END_COLUMN + 11 to ensure enough space for the score
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl <<  "Terminal window must be at least " << to_string(END_ROW) << " by " << to_string(END_COLUMN + 11) <<  " to run this game" << endl;
        return EXIT_FAILURE;
    }

    // State Variables
    fruit fruit;
    basket basket;
    score score;
    unsigned int ticks { 0 };

    char currentChar { NULL_CHAR }; // do nothing until user presses left or right

    bool allowBackgroundProcessing { true };

    auto startTimestamp { chrono::steady_clock::now() };
    auto endTimestamp { startTimestamp };
    int elapsedTimePerTick { 100 }; // every 0.1s check on things; used as the speed of the falling fruit
    
    SetNonblockingReadState( allowBackgroundProcessing );
    ClearScreen();
    HideCursor();
    DrawBoard();
    DrawScore(score);

    while( currentChar != QUIT_CHAR )
    {
        endTimestamp = chrono::steady_clock::now();
        auto elapsed { chrono::duration_cast<chrono::milliseconds>( endTimestamp - startTimestamp ).count() };
        // We want to process input and update the world when enough time has elapsed
        if ( elapsed >= elapsedTimePerTick )
        {
            ticks++;
            cerr << "Ticks [" << ticks << "] allowBackgroundProcessing ["<< allowBackgroundProcessing << "] elapsed [" << elapsed << "] currentChar [" << currentChar << "]" << endl;

            if ( currentChar == LEFT_CHAR || currentChar == RIGHT_CHAR ) 
            {
                UpdateBasketPosition( basket, currentChar );
            }
        
            if ( fruit.position.row == END_ROW - 1 ) 
            {
                if ( IsCaught( fruit, basket ) ) // IsCaught called only when row of fruit and basket are equal
                {
                    ResetFruit( fruit );
                    UpdateScore( score );
                    if ( elapsedTimePerTick >= 50 ) // don't go faster than 50 ms or else too laggy and impossible to win
                    {
                        // A known issue is lag, especially as elapsedTimePerTick decreases, because rerendering is occuring too quickly 
                        elapsedTimePerTick -= 3;
                    }
                }
                else
                {
                    break; // user unable to catch the fruit
                }  
            }
            // Update screen every time interval
            UpdateFruitPosition( fruit );
            ClearScreen();
            HideCursor();
            DrawBoard();
            DrawFruit( fruit );
            DrawBasket( basket );
            DrawScore( score );

            // Clear inputs in preparation for the next iteration
            startTimestamp = endTimestamp;    
            cerr << currentChar << endl;
            currentChar = NULL_CHAR;
        }
        // Only get keyboard input if "a" or "d" hasn't been registered yet, otherwise getchar will override keyboard input to produce a null character
        if ( !( currentChar == LEFT_CHAR || currentChar == RIGHT_CHAR ) ) {
            currentChar = getchar();
        }
    }
    
    // Screen redrawn to remove score counter and print game over screen
    ClearScreen();
    DrawBoard();
    DrawFruit( fruit );
    DrawBasket( basket );
    DrawGameOver( score );

    // Tidy Up and Close Down
    ShowCursor();
    SetNonblockingReadState( false );
    TeardownScreenAndInput();
    cout << endl; // be nice to the next command
    return EXIT_SUCCESS;
}
