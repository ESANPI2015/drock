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
        /* Additional Upper Concepts */
        static const UniqueId DomainId;
        static const UniqueId ComponentId;
        static const UniqueId ComponentTypeId;
        static const UniqueId InterfaceId;
        static const UniqueId InterfaceDirectionId;
        static const UniqueId InterfaceTypeId;
        static const UniqueId EdgeTypeId;
        static const UniqueId ConfigurationId;
        static const UniqueId HasConfigId;
        static const UniqueId AliasOfId;

        Model();
        Model(const Hypergraph& base);
        ~Model();

        std::string domainSpecificExport(const UniqueId& uid);
        bool domainSpecificImport(const std::string& serialized);

        // Generate UIDs for fast lookup
        UniqueId getDomainUid(const std::string& domain);
        UniqueId getTypeUid(const std::string& type);
        UniqueId getComponentUid(const std::string& domain, const std::string& name, const std::string& version="");
        UniqueId getInterfaceUid(const std::string& type, const std::string& direction);
        UniqueId getEdgeUid(const std::string& type);

        // Apply config
        Hyperedges hasConfig(const Hyperedges& parentUids, const Hyperedges& childrenUids);
        Hyperedges instantiateConfigOnce(const Hyperedges& parentUids, const std::string& label="");

        // Query config
        Hyperedges configsOf(const Hyperedges& uids, const std::string& label="");

        // Create aliasPort
        Hyperedges aliasOf(const Hyperedges& aliasInterfaceUids, const Hyperedges& originalInterfaceUids);
        Hyperedges instantiateAliasInterfaceOnce(const Hyperedges& parentUids, const Hyperedges& interfaceUids, const std::string& label="");

        // Query original interfaces of alias interfaces
        Hyperedges originalInterfaces(const Hyperedges& uids, const std::string& label="");

        // Check if we are in the SOFTWARE domain
        bool inSoftwareDomain(const UniqueId& domainUid);
        bool isInput(const UniqueId& interfaceDirUid);
        bool isOutput(const UniqueId& interfaceDirUid);

    protected:
        void setupMetaModel();
};

}

#endif
