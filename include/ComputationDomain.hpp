#ifndef _DROCK_COMPUTATIONAL_DOMAIN_HPP
#define _DROCK_COMPUTATIONAL_DOMAIN_HPP

#include <HardwareComputationalNetwork.hpp>

namespace Drock {

class Computation : public Hardware::Computational::Network
{
    public:
        Computation();
        Computation(const Hypergraph& base);
        ~Computation();

        std::string domainSpecificExport(const UniqueId& uid);
        bool domainSpecificImport(const std::string& serialized);
    protected:
        void setupMetaModel();
};

}

#endif
