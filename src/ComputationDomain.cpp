#include "ComputationDomain.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>

namespace Drock {

void Computation::setupMetaModel()
{
    // Setup meta model
    // NOTE: We want to handle the basic model which is also stored in the DROCK DB ... therefore we will
    // NOT model the classes in the system_modelling library
    createDevice("Drock::Computation::Device","Device");
    isA(createProcessor("Drock::Computation::Processor","Processor"), Hyperedges{"Drock::Computation::Device"});
    isA(createProcessor("Drock::Computation::Conventional","Conventional"), Hyperedges{"Drock::Computation::Processor"});
    isA(createProcessor("Drock::Computation::FPGA","FPGA"), Hyperedges{"Drock::Computation::Processor"});
    isA(createDevice("Drock::Computation::Peripheral","Peripheral"), Hyperedges{"Drock::Computation::Peripheral"});
    createInterface("Drock::Computation::Interface","Interface");
    createBus("Drock::Computation::Bus","Bus");
    isA(createNetwork("Drock::Computation::Network","system_modelling::hardware_graph::Network"), Hyperedges{"Drock::Computation::Device"});
}

Computation::Computation()
{
    setupMetaModel();
}

Computation::Computation(const Hardware::Computational::Network& base)
: Hardware::Computational::Network(base)
{
    setupMetaModel();
}

Computation::~Computation()
{
}

std::string Computation::domainSpecificExport(const UniqueId& uid)
{
    std::stringstream ss;
    YAML::Node spec;
    spec["domain"] = "COMPUTATION"; // TODO: Use protobuf spec
    spec["name"] = get(uid)->label();
    Hyperedges directSuperRaw(to(intersect(factsOf(subrelationsOf(CommonConceptGraph::IsAId)), relationsFrom(Hyperedges{uid}))));
    if (directSuperRaw.size() > 1)
    {
        // FIXME: This happens, because we use createComponent in import!!! We have to filter by domain!!!
        std::cout << "Warning: Multiple direct superclasses\n";
    }
    // WORKAROUND: For now we filter the direct superclasses by prefix
    Hyperedges directSuper;
    for (const UniqueId& suid : directSuperRaw)
    {
        if (suid.find("Drock::Computation") != std::string::npos)
            directSuper.insert(suid);
    }
    if (!directSuper.size())
    {
        std::cout << "Error: No valid superclass found\n";
        std::cout << directSuperRaw << std::endl;
        return std::string();
    }
    std::string type(get(*directSuper.begin())->label());
    spec["type"] = type; // This means: direct-subclassOf! The format does not support multiple classes

    // So, what do different versions mean? It is some form of sublassing.
    // So on toplvl is the component NAME ... The version name should be used as UID!
    // Then we can easily differentiate between different components
    YAML::Node versionsYAML(spec["versions"]);
    YAML::Node versionYAML;
    versionYAML["name"] = uid;
    versionYAML["date"] = "UNKNOWN";
    // Interfaces
    YAML::Node interfacesYAML(versionYAML["interfaces"]);
    Hyperedges ifs(interfacesOf(Hyperedges{uid}));
    for (const UniqueId& ifId : ifs)
    {
        YAML::Node interfaceYAML;
        interfaceYAML["name"] = get(ifId)->label();
        Hyperedges superIfs(instancesOf(Hyperedges{ifId}, "", TraversalDirection::DOWN));
        interfaceYAML["type"] = get(*superIfs.begin())->label();
        interfaceYAML["direction"] = "BIDIRECTIONAL"; // TODO: Use protobuf spec
        interfacesYAML.push_back(interfaceYAML);
    }
    // TODO: Handle subcomponents
    // TODO: Handle config
    versionsYAML.push_back(versionYAML);
    ss << spec;
    return ss.str();
}

bool Computation::domainSpecificImport(const std::string& serialized)
{
    YAML::Node spec = YAML::Load(serialized);
    if (!spec["domain"].IsDefined())
        return false;
    const std::string domain(spec["domain"].as<std::string>());
    // TODO: Check domain
    if (!spec["name"].IsDefined())
        return false;
    const std::string name(spec["name"].as<std::string>());
    if (!spec["type"].IsDefined())
        return false;
    const std::string type(spec["type"].as<std::string>());
    Hyperedges super(deviceClasses(type));
    if (super.empty())
        return false;
    if (!spec["versions"].IsDefined())
        return false;
    const YAML::Node& versions(spec["versions"]);
    for (auto it = versions.begin(); it != versions.end(); it++)
    {
        const YAML::Node& version(*it);
        const std::string& vname(version["name"].as<std::string>()); // Use version name as UID!!!
        // TODO: What about:
        // const std::string& vdate(version["date"]);
        // Create a component for every version
        Hyperedges newModelUID(createComponent(vname, name, super));
        // Handle interfaces
        if (version["interfaces"].IsDefined())
        {
            Hyperedges allInterfaces;
            const YAML::Node& ifs(version["interfaces"]);
            for (auto ifIt = ifs.begin(); ifIt != ifs.end(); ifIt++)
            {
                const YAML::Node& interfaceYAML(*ifIt);
                const std::string& ifName(interfaceYAML["name"].as<std::string>());
                const std::string& ifType(interfaceYAML["type"].as<std::string>());
                //const std::string& ifDirection(interface["direction"].as<std::string>());
                //std::cout << ifName << " of type " << ifType << "\n";

                // Find or create superclass
                Hyperedges superIf(interfaceClasses(type));
                if (superIf.empty())
                {
                    superIf = Component::Network::createInterface(ifType, ifType, Hyperedges{"Drock::Computation::Interface"});
                }

                // Create interface
                allInterfaces = unite(allInterfaces, instantiateFrom(superIf, ifName));
            }
            // Attach interfaces to model
            hasInterface(newModelUID, allInterfaces);
        }
        // TODO: Handle subgraph of component instances
        // TODO: Handle configuration
    }
    return true;
}

}
