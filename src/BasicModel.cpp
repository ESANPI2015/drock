#include "BasicModel.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>

namespace Drock {

void Model::setupMetaModel()
{
    // Setup meta model
    // NOTE: We want to handle the basic model which is also stored in the DROCK DB ... therefore we will
    // NOT model the classes in the system_modelling library
    
    // The domain specifies to which view a component belongs
    // For example a 'Task' belongs to the 'SOFTWARE' domain
    // This meta component holds all domains
    createComponent("Drock::Model::Domain", "DOMAIN");

    // Predefine some domains we already know
    createComponent("Drock::Model::Domain::SOFTWARE", "SOFTWARE", Hyperedges{"Drock::Model::Domain"});
    createComponent("Drock::Model::Domain::COMPUTATION", "COMPUTATION", Hyperedges{"Drock::Model::Domain"});

    // TODO: Here we could also define the main component classes we already know
    // HARDWARE: Device, Processor, Bus etc.
    // SOFTWARE: Task, Port etc.

    // Create a meta component class for interfaces
    createInterface("Drock::Model::Interface", "Interface");
}

Model::Model()
{
    setupMetaModel();
}

Model::Model(const Hypergraph& base)
: Component::Network(base)
{
    setupMetaModel();
}

Model::~Model()
{
}

std::string Model::domainFromLabel(const std::string& label)
{
    std::size_t pos = label.find("::");
    return label.substr(0,pos);
}

std::string Model::typeFromLabel(const std::string& label)
{
    std::size_t pos1 = label.find("::");
    std::size_t pos2 = label.find("::", pos1+2);
    return pos2 > pos1+2 ? label.substr(pos1+2, pos2-pos1-2) : std::string();
}

std::string Model::nameFromLabel(const std::string& label)
{
    std::size_t pos1 = label.find("::");
    std::size_t pos2 = label.find("::", pos1+2);
    std::size_t pos3 = label.find("::", pos2+2);
    return pos3 > pos2+2 ? label.substr(pos2+2, pos3-pos2-2) : std::string();
}

std::string Model::versionFromLabel(const std::string& label)
{
    std::size_t pos1 = label.find("::");
    std::size_t pos2 = label.find("::", pos1+2);
    std::size_t pos3 = label.find("::", pos2+2);
    return pos3 < std::string::npos ? label.substr(pos3+2) : std::string();
}

std::string Model::labelFrom(const std::string& domain, const std::string& type, const std::string& name, const std::string& version)
{
    std::string label;
    if (domain.empty())
        return label;
    label = domain;
    if (type.empty())
        return label;
    label += "::"+type;
    if (name.empty())
        return label;
    label += "::"+name;
    if (version.empty())
        return label;
    return label+"::"+version;
}

std::string Model::interfaceTypeFromLabel(const std::string& label)
{
    std::size_t pos = label.find("::");
    return label.substr(0,pos);
}

std::string Model::interfaceDirectionFromLabel(const std::string& label)
{
    std::size_t pos = label.find("::");
    return pos < std::string::npos ? label.substr(pos+2) : std::string();
}

std::string Model::interfaceLabelFrom(const std::string& type, const std::string& direction)
{
    std::string label;
    if (type.empty())
        return label;
    label = type;
    if (direction.empty())
        return label;
    return label+"::"+direction;
}

bool Model::domainSpecificImport(const std::string& serialized)
{
    YAML::Node spec = YAML::Load(serialized);

    // TODO: Actually we want to compose only the UID, the label should be short!
    // Handle domain
    if (!spec["domain"].IsDefined())
        return false;
    const std::string domain(spec["domain"].as<std::string>());
    // Create a new meta component (if not already there!)
    const UniqueId domainUid("Drock::Model::Domain::"+labelFrom(domain));
    createComponent(domainUid, labelFrom(domain), Hyperedges{"Drock::Model::Domain"});

    // Handle domain specific type
    if (!spec["type"].IsDefined())
        return false;
    const std::string type(spec["type"].as<std::string>());
    // Create or find a new meta component in the selected domain
    const UniqueId typeUid("Drock::Model::Domain::"+labelFrom(domain, type));
    Hyperedges super(componentClasses(labelFrom(domain, type), Hyperedges{domainUid}));
    if (super.empty())
    {
        // Create new meta model
        super = createComponent(typeUid, labelFrom(domain, type), Hyperedges{domainUid});
    }

    // Handle name
    if (!spec["name"].IsDefined())
        return false;
    const std::string name(spec["name"].as<std::string>());

    // For each of the versions we have to create a new component
    if (!spec["versions"].IsDefined())
        return false;
    const YAML::Node& versions(spec["versions"]);
    for (auto it = versions.begin(); it != versions.end(); it++)
    {
        // Create model for the (domain, type, name, vname) tupel
        const YAML::Node& version(*it);
        const std::string& vname(version["name"].as<std::string>());
        const UniqueId modelUid("Drock::Model::Domain::"+labelFrom(domain, type, name, vname));
        Hyperedges newModelUid(createComponent(modelUid,labelFrom(domain, type, name, vname),super));

        // Handle interfaces
        // TODO: Also, for interfaces we should assemble the UID of the superIf by (type, direction) tupel
        if (version["interfaces"].IsDefined())
        {
            Hyperedges allInterfaces;
            const YAML::Node& ifs(version["interfaces"]);
            for (auto ifIt = ifs.begin(); ifIt != ifs.end(); ifIt++)
            {
                const YAML::Node& interfaceYAML(*ifIt);
                const std::string& ifName(interfaceYAML["name"].as<std::string>());
                const std::string& ifType(interfaceYAML["type"].as<std::string>());
                const std::string& ifDirection(interfaceYAML["direction"].as<std::string>());

                // Find or create superclass
                Hyperedges superIf(interfaceClasses(interfaceLabelFrom(ifType, ifDirection), Hyperedges{"Drock::Model::Interface"}));
                if (superIf.empty())
                {
                    superIf = createInterface("Drock::Model::Interface::"+interfaceLabelFrom(ifType, ifDirection), interfaceLabelFrom(ifType, ifDirection), Hyperedges{"Drock::Model::Interface"});
                }

                // Create interface
                allInterfaces = unite(allInterfaces, instantiateFrom(superIf, ifName));
            }
            // Attach interfaces to model
            hasInterface(newModelUid, allInterfaces);
        }

        // TODO: Handle subcomponents
        
        // TODO: Handle config and other unmodeled stuff!
    }
    return true;
}

std::string Model::domainSpecificExport(const UniqueId& uid)
{
    std::stringstream ss;
    YAML::Node spec;
    if (!get(uid))
        return std::string();
    // In the label we should find
    // * domain
    spec["domain"] = domainFromLabel(get(uid)->label());
    // * type
    spec["type"] = typeFromLabel(get(uid)->label());
    // * name
    spec["name"] = nameFromLabel(get(uid)->label());
    // * version
    YAML::Node versionsYAML(spec["versions"]);
    YAML::Node versionYAML;
    versionYAML["name"] = versionFromLabel(get(uid)->label());

    // Interfaces
    YAML::Node interfacesYAML(versionYAML["interfaces"]);
    Hyperedges ifs(interfacesOf(Hyperedges{uid}));
    for (const UniqueId& ifId : ifs)
    {
        YAML::Node interfaceYAML;
        interfaceYAML["name"] = get(ifId)->label();
        Hyperedges superIfs(instancesOf(Hyperedges{ifId}, "", TraversalDirection::DOWN));
        for (const UniqueId& suid : superIfs)
        {
            const std::string& ifType(interfaceTypeFromLabel(get(suid)->label()));
            const std::string& ifDirection(interfaceDirectionFromLabel(get(suid)->label()));
            if (ifType.empty() || ifDirection.empty())
                continue;
            interfaceYAML["type"] = ifType;
            interfaceYAML["direction"] = ifDirection;
            interfacesYAML.push_back(interfaceYAML);
        }
    }

    // TODO: Handle subcomponents

    // TODO: Handle config and other unmodeled stuff

    // Push version (there will be only one)
    versionsYAML.push_back(versionYAML);

    ss << spec;
    return ss.str();
}


}
