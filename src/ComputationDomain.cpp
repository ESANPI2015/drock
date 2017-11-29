#include "ComputationDomain.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>

namespace Drock {

Computation::Computation()
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

Computation::~Computation()
{
}

std::string Computation::domainSpecificExport()
{
    std::string result;
    return result;
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
        // Create a component for every version
        Hyperedges newModelUID(createComponent(type, name, super));
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
    }
    return true;
}

}
