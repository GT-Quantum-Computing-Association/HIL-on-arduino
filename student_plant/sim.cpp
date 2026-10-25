#include <iostream>
#include <random>

int main(){
    const int seed = seq * 918354 + 435 ;
    const int rate = 100000;
    const int num_windows = 1000;   //placeholder windows

    std::srand(seed);
    std::default_random_engine generator;
    std::poisson_distribution<int> distribution(.05); 

    

    for (int i = 0; i < num_windows)


}