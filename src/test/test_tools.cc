
#include <iostream>
#include "../base/tools.h"
void add(int& a,int b){
    std::cout<<(a+b)<<'\n';
}

int main(){
    int a=4;
    auto f = Package2FVV(add,a,5);
    f();
}