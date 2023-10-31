#include <unistd.h>
#include <string>
#include <iostream>
#include <stdio.h>

int main(int argc, char **argv) {

  int opt;
  while ( (opt = getopt(argc, argv, "abc:d:")) != -1 ) {
 
    // std::cout << "opt = " << opt << std::endl;
    // std::cout << "optarg = " << optarg << std::endl;
    // std::cout << "optind = " << optind << std::endl;
    // std::cout << "argv[optind - 1] = " << argv[optind - 1]<< std::endl;

    printf("opt = %c\n", opt);
    printf("optarg = %s\n", optarg);
    printf("optind = %d\n", optind);
    printf("argv[optind - 1] = %s\n\n",  argv[optind - 1]);
  }

  return 0;
}