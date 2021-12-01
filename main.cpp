// compile with: clang++ -std=c++20 -Wall -Werror -Wextra -Wpedantic -g3 -o fruits fruits.cpp
// run with: ./fruits 2> /dev/null or ./fruits 2> bug.txt to not get weird messages in the terminal
//  "2>" redirect standard error (STDERR; cerr)
//  /dev/null is a "virtual file" which discard contents

// Works best in Visual Studio Code if you set:
//   Settings -> Features -> Terminal -> Local Echo Latency Threshold = -1

// https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit 

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
const char LEFT_CHAR     { 'a' };
const char RIGHT_CHAR    { 'd' };
const char QUIT_CHAR     { 'q' };

const string ANSI_START { "\033[" };
const string START_COLOUR_PREFIX {"1;"};
const string START_COLOUR_SUFFIX {"m"};
const string STOP_COLOUR  {"\033[0m"};

const unsigned int COLOUR_IGNORE  { 0 }; // this is a little dangerous but should work out OK
const unsigned int COLOUR_BLACK   { 30 };
const unsigned int COLOUR_RED     { 31 };
const unsigned int COLOUR_GREEN   { 32 };
const unsigned int COLOUR_YELLOW  { 33 };
const unsigned int COLOUR_BLUE    { 34 };
const unsigned int COLOUR_MAGENTA { 35 };
const unsigned int COLOUR_CYAN    { 36 };
const unsigned int COLOUR_WHITE   { 37 };

const int START_ROW        { 1 };
const int END_ROW          { 16 };
const int START_COLUMN     { 1 };
const int MIDDLE_COLUMN    { 17 };
const int END_COLUMN       { 37 };

const unsigned short MOVING_NOWHERE { 0 };
const unsigned short MOVING_LEFT    { 1 };
const unsigned short MOVING_RIGHT   { 2 };
const unsigned short MOVING_UP      { 3 };
const unsigned short MOVING_DOWN    { 4 };


#pragma clang diagnostic pop

// Globals

std::random_device rd;  //Will be used to obtain a seed for the random number engine
std::mt19937 generator(rd()); //Standard mersenne_twister_engine seeded with rd()
std::uniform_int_distribution<> lane(1, 7);

struct termios initialTerm;

// Types

// Using signed and not unsigned to avoid having to check for ( 0 - 1 ) being very large
struct position { int row; int col; };

struct score
{
    int score = 0;
    unsigned int colour = COLOUR_GREEN;
};

struct fruit 
{
    position position {START_ROW + 1, (lane( generator ) * 5) - 1 };
    unsigned int colour = COLOUR_BLUE;
};

struct basket
{
  position position { END_ROW - 1, MIDDLE_COLUMN }; // each lane the fruit falls is 3 columns wide
  unsigned int colour = COLOUR_MAGENTA;
};

typedef vector< fruit > fruitvector;

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

// Fish Logic

auto UpdateFruitPosition( fruit & fruit ) -> void
{
    int rowChange = 1;

    // Update the position of the fruit
    fruit.position.row += rowChange;
}

auto UpdateBasketPosition( basket & basket, char currentChar ) -> void
{
    // Deal with movement commands 
    int commandColChange {0};

    if ( currentChar == LEFT_CHAR )  { commandColChange -= 5; }
    if ( currentChar == RIGHT_CHAR ) { commandColChange += 5; }

    // Update the position of each fish
    // Use a reference so that the actual position updates
    auto proposedCol { basket.position.col + commandColChange };
    basket.position.col = max( START_COLUMN + 1, min( END_COLUMN - 5, proposedCol ) );
}

auto CreateFruit( fruitvector & fruits ) -> void
{
    cerr << "Creating Fruit" << endl;
    fruit newFruit { 
        .position = { .row = START_ROW + 1, .col = (( lane( generator ) ) * 5) - 1 } ,
        .colour = COLOUR_MAGENTA,
    };
    fruits.push_back( newFruit );
}

auto DrawFruit(fruit fruit) -> void
{
    MoveTo( fruit.position.row, fruit.position.col ); 
    cout << MakeColour("O", COLOUR_GREEN) << flush;
}

auto DrawBasket(basket basket ) -> void
{
    MoveTo( basket.position.row, basket.position.col ); 
    cout << "\\___/" << flush; 
}

// Functions to do

auto CheckInteraction(fruit fruit, basket basket ) -> bool
{
    if (fruit.position.col -2 == basket.position.col)

    {
        return true;
    }
        return false;
}

auto ResetFruit(fruit & fruit) -> void
{
    int newCol {(lane(generator) * 5) - 1};
    fruit.position.col = newCol;
    fruit.position.row = START_ROW + 1;
}

auto DrawBoard() -> void
{
    for ( unsigned int row = START_ROW; row <= END_ROW; row++ ) {
        if ( row == START_ROW || row == END_ROW ) {
            for ( unsigned int column = START_COLUMN; column <= END_COLUMN; column++ ) {
                MoveTo( row, column ); 
                cout << "-" << flush;
            }
        }
        else {
            MoveTo( row, START_COLUMN ); 
            cout << "|" << flush;
            MoveTo( row, END_COLUMN ); 
            cout << "|" << flush;
        }
    }
}

auto DrawScore(score score) -> void
{
    MoveTo(START_ROW, END_COLUMN + 2);
    cout << "score: " << to_string(score.score) << flush;
}

auto DrawGameOver(score score) -> void
{
    MoveTo(8,14);
    cout << MakeColour("GAME OVER :(", COLOUR_RED) << flush;
    MoveTo(9,16);
    cout << "Score: " << to_string(score.score) << flush;
    MoveTo(END_ROW, START_COLUMN);
}

auto UpdateScore(score & score) -> void
{
    score.score++;
}
// drawBoard

auto main() -> int
{
    // Set Up the system to receive input
    SetupScreenAndInput();

    // Check that the terminal size is large enough for our fishies
    const position TERMINAL_SIZE { GetTerminalSize() };
    if ( ( TERMINAL_SIZE.row < 20 ) or ( TERMINAL_SIZE.col < 38 ) )
    {
        ShowCursor();
        TeardownScreenAndInput();
        cout << endl <<  "Terminal window must be at least 20 by 22 to run this game" << endl;
        return EXIT_FAILURE;
    }

    // State Variables
    fruitvector fruits;
    fruit fruit;
    basket basket;
    score score;
    unsigned int ticks {0};

    char currentChar { NULL_CHAR };

    bool allowBackgroundProcessing { true };

    auto startTimestamp { chrono::steady_clock::now() };
    auto endTimestamp { startTimestamp };
    int elapsedTimePerTick { 100 }; // Every 0.1s check on things
    
    SetNonblockingReadState( allowBackgroundProcessing );
    ClearScreen();
    HideCursor();
    DrawBoard();
    DrawScore(score);

    while( currentChar != QUIT_CHAR )
    {
        endTimestamp = chrono::steady_clock::now();
        auto elapsed { chrono::duration_cast<chrono::milliseconds>( endTimestamp - startTimestamp ).count() };
        // We want to process input and update the world when EITHER  
        // (a) there is background processing and enough time has elapsed
        // (b) when we are not allowing background processing.
        if ( elapsed >= elapsedTimePerTick )
        {
            ticks++;
            //cerr << "Ticks [" << ticks << "] allowBackgroundProcessing ["<< allowBackgroundProcessing << "] elapsed [" << elapsed << "] currentChar [" << currentChar << "]" << endl;
            if ( currentChar == LEFT_CHAR || currentChar == RIGHT_CHAR ) {
                UpdateBasketPosition( basket, currentChar);
            }
            
            if ( fruit.position.row == END_ROW - 1){
                if (CheckInteraction(fruit, basket)){
                    ResetFruit(fruit);
                    UpdateScore(score);
                    if (elapsedTimePerTick >= 50){
                        elapsedTimePerTick -= 3;  
                    }
                }
                else{
                    break;
                }
                
            }
            UpdateFruitPosition(fruit);
            ClearScreen();
            HideCursor();
            DrawBoard();
            DrawFruit( fruit );
            DrawBasket( basket );
            DrawScore(score);

            // Clear inputs in preparation for the next iteration
            startTimestamp = endTimestamp;    
            cerr << currentChar << endl;
            currentChar = NULL_CHAR;
        }
        if (currentChar == 'a' || currentChar == 'd') {
            continue;
        }
        currentChar = getchar();
        

        // read( 0, &currentChar, 1 );

        //     UpdateFruitPosition( fruit );
        //     UpdateBasketPosition( basket, )
        //     ClearScreen();
        //     DrawBoard();
        //     DrawFishies( fishies );

        //     if ( showCommandline )
        //     {
        //         cerr << "Showing Command Line" << endl;
        //         MoveTo( 21, 1 ); 
        //         ShowCursor();
        //         cout << "Command:" << flush;
        //     }
        //     else { HideCursor(); }

        //     // Clear inputs in preparation for the next iteration
        //     startTimestamp = endTimestamp;    
        //     currentChar = NULL_CHAR;
        // }
        // // Depending on the blocking mode, either read in one character or a string (character by character)
        // if ( showCommandline )
        // {
        //     while ( read( 0, &currentChar, 1 ) == 1 && ( currentChar != '\n' ) )
        //     {
        //         cout << currentChar << flush; // the flush is important since we are in non-echoing mode
        //         currentCommand += currentChar;
        //     }
        //     cerr << "Received command [" << currentCommand << "]" << endl;
        //     currentChar = NULL_CHAR;
        // else
        // {
        //     read( 0, &currentChar, 1 );
        // }
    }
    // Tidy Up and Close Down

    ClearScreen();
    DrawBoard();
    DrawFruit( fruit );
    DrawBasket( basket );
    DrawGameOver(score);
    ShowCursor();
    SetNonblockingReadState( false );
    TeardownScreenAndInput();
    cout << endl; // be nice to the next command
    return EXIT_SUCCESS;
}
