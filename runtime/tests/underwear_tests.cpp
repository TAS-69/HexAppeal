#include "underwear.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 2;std::string error;
    if(!hex::runUnderwearPolicyTests(argv[1],error)){std::cerr<<"FAIL: "<<error<<'\n';return 1;}
    std::cout<<"PASS: per-witch and all-witch underwear scope, unchanged custom outfit parts, native call arguments, original-rule restoration, persistence and unavailable-route rejection.\nFixture native calls only; rendered underwear removal still requires live acceptance.\n";
}
