#ifndef PARSE_H
#define PARSE_H

#include "cxxopts/cxxopts.hpp"

/**
 * If this exception is thrown, the program should be terminated.
 */
class EndException : public std::exception
{
       const int _bad;

    public:
        explicit EndException(int bad);
        /**
         * Returns 1 if the termination is because of an error, 0 if otherwise
         */
        int is_bad() const noexcept;
};

/**
 * Parse command line arguments.
 */
cxxopts::ParseResult parse(int argc, char* argv[]);

#endif // PARSE_H
