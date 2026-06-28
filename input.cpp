#include <fstream>
#include <iostream>
#include <string>

int main(){
    std::ifstream file("hello.pdf");
    if (file.is_open()){
        std::string line;
        while (std::getline(file, line)){
            std::cout<<line<<"\n";
        }
        file.close();
    }
    return 0;
}

