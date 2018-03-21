#ifndef _DROCK_BASIC_MODEL_HPP
#define _DROCK_BASIC_MODEL_HPP

#include <ComponentNetwork.hpp>

namespace Drock {

// TODO: Actually this class encodes already Hardware::Computational::Network & Software::Graph
// We should either inherit from both (diamond shape :]) OR at least make links to both meta models
//
// TODO: Instead of using namespacing to set/get information about the basic model
// we should create metaclasses to specify types.
// Then we can also make use of the nice things of conceptual graphs, namely multiple inheritance/types

class Model : public Component::Network
{
    public:
        Model();
        Model(const Hypergraph& base);
        ~Model();

        std::string domainSpecificExport(const UniqueId& uid);
        bool domainSpecificImport(const std::string& serialized);

        // Format: domain::type::name::version
        std::string domainFromLabel(const std::string& label);
        std::string typeFromLabel(const std::string& label);
        std::string nameFromLabel(const std::string& label);
        std::string versionFromLabel(const std::string& label);
        std::string labelFrom(const std::string& domain="", const std::string& type="", const std::string& name="", const std::string& version="");

        // Format: type::direction
        std::string interfaceTypeFromLabel(const std::string& label);
        std::string interfaceDirectionFromLabel(const std::string& label);
        std::string interfaceLabelFrom(const std::string& type="", const std::string& direction="");

    protected:
        void setupMetaModel();
};

}

#endif
