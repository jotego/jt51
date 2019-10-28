#include <iostream>
#include "VGMParser.hpp"

using namespace std;

int main(int argc, char *argv[]) {
    VGMParser parser(280);
    parser.open( argv[1] );
    while( parser.parse() >= 0 );   
}
