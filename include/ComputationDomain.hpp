#ifndef _DROCK_COMPUTATIONAL_DOMAIN_HPP
#define _DROCK_COMPUTATIONAL_DOMAIN_HPP

#include <HardwareComputationalNetwork.hpp>

namespace Drock {

class Computation : public Hardware::Computational::Network
{
    public:
        Computation();
        ~Computation();

        std::string domainSpecificExport();
        bool domainSpecificImport(const std::string& serialized);
};

}

#endif
