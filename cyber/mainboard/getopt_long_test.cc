#include <getopt.h>
#include <string>
#include <iostream>

int main(int argc, char **argv) {

  int opt;
  const std::string short_opts = "abc:d:";
  const static struct option long_opts[] = {
    { "apple", no_argument, nullptr, 'a' },
    { "bin", no_argument, nullptr, 'b' },
    { "cool", required_argument, nullptr, 'c' },
    { "dog", optional_argument, nullptr, 'd' },
    { nullptr, 0, nullptr, 0 }
  };

  while ( (opt = getopt_long(argc, argv, short_opts.c_str(), long_opts, nullptr)) != -1 ) {
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