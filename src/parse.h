#ifndef PARSE_H
#define PARSE_H

#include "cxxopts/cxxopts.hpp"

class EndException : public std::exception
{
       int _bad;

    public:
        EndException(int bad);
        /**
         * Returns 1 if the termination is because of an error, 0 if otherwise
         */
        int is_bad() const noexcept;
};

cxxopts::ParseResult parse(int argc, char* argv[]);

#endif // PARSE_H
