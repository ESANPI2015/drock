#include "BasicModel.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>

namespace Drock {

const UniqueId Model::DomainId = "Drock::Model::Domain";
const UniqueId Model::ComponentId = "Drock::Model::Component";
const UniqueId Model::ComponentTypeId = "Drock::Model::Component::Type";
const UniqueId Model::InterfaceId = "Drock::Model::Interface";
const UniqueId Model::InterfaceDirectionId = "Drock::Model::Interface::Direction";
const UniqueId Model::InterfaceTypeId = "Drock::Model::Interface::Type";

void Model::setupMetaModel()
{
    // Setup meta model
    // NOTE: We want to handle the basic model which is also stored in the DROCK DB ... therefore we will
    // NOT model the classes in the system_modelling library
    
    // The domain specifies to which view a component belongs
    // It is a class but not a component class!
    create(Model::DomainId, "Domain");

    // Create a meta component class for DROCK components
    createComponent(Model::ComponentId, "Component");
    createComponent(Model::ComponentTypeId, "Type", Hyperedges{Model::ComponentId});
    // TODO: Versions?

    // Create a meta interface class for DROCK interfaces
    createInterface(Model::InterfaceId, "Interface");
    createInterface(Model::InterfaceDirectionId, "Direction", Hyperedges{Model::InterfaceId});
    createInterface(Model::InterfaceTypeId, "Type", Hyperedges{Model::InterfaceId});

    // TODO: Do we need more? e.g. Configuration, Connector etc?
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

UniqueId Model::getDomainUid(const std::string& domain)
{
    return Model::DomainId+"::"+domain;
}

UniqueId Model::getTypeUid(const std::string& type)
{
    return Model::ComponentTypeId+"::"+type;
}

UniqueId Model::getComponentUid(const std::string& domain, const std::string& name, const std::string& version)
{
    return version.empty() ? (Model::ComponentId+"::"+domain+"::"+name) : (Model::ComponentId+"::"+domain+"::"+name+"::"+version);
}

UniqueId Model::getInterfaceUid(const std::string& type, const std::string& direction)
{
    bool valid = false;
    std::string uid(Model::InterfaceId);
    if (!type.empty())
    {
        uid = uid+"::"+type;
        valid = true;
    }
    if (!direction.empty())
    {
        uid = uid+"::"+direction;
        valid = true;
    }
    return valid ? uid : "";
}

bool Model::domainSpecificImport(const std::string& serialized)
{
    YAML::Node spec = YAML::Load(serialized);

    // Handle domain, type, name
    if (!spec["domain"].IsDefined())
        return false;
    const std::string domain(spec["domain"].as<std::string>());
    if (!spec["type"].IsDefined())
        return false;
    const std::string type(spec["type"].as<std::string>());
    if (!spec["name"].IsDefined())
        return false;
    const std::string name(spec["name"].as<std::string>());

    // Create domain
    // NOTE: For now the domain is related to subsequent components via IS-A relationship
    const UniqueId domainUid(getDomainUid(domain));
    createSubclassOf(domainUid, Hyperedges{Model::DomainId}, domain);
    // Create type
    const UniqueId typeUid(getTypeUid(type));
    createComponent(typeUid, type, Hyperedges{Model::ComponentTypeId});
    // Create a component by name which is a subclass of both a domain and a type
    const UniqueId superUid(getComponentUid(domain, name));
    createComponent(superUid, name, Hyperedges{typeUid});
    isA(Hyperedges{superUid}, Hyperedges{domainUid});
    // For each of the versions we have to create a new component
    // The question is: Do we create a subclass for each version? Or do we just use names?
    // We should do the former.
    if (!spec["versions"].IsDefined())
        return false;
    const YAML::Node& versions(spec["versions"]);
    for (auto it = versions.begin(); it != versions.end(); it++)
    {
        // Create a subclass of superUid with label (name, vname)
        const YAML::Node& version(*it);
        const std::string& vname(version["name"].as<std::string>());
        const UniqueId modelUid(getComponentUid(domain, name, vname));
        createComponent(modelUid, vname, Hyperedges{superUid});

        // TODO: Handle subcomponents & their interconnection. Create only if non-existing.
        const YAML::Node& components(version["components"]);
        if (components.IsDefined())
        {
            Hyperedges validNodeUids;
            const YAML::Node& nodes(components["nodes"]);
            if (nodes.IsDefined())
            {
                for (auto nit = nodes.begin(); nit != nodes.end(); nit++)
                {
                    const YAML::Node& node(*nit);
                    const std::string& nodeName(node["name"].as<std::string>());
                    const std::string& nodeModelName(node["model"]["name"].as<std::string>());
                    const std::string& nodeModelDomain(node["model"]["domain"].as<std::string>());
                    const std::string& nodeModelVersion(node["model"]["version"].as<std::string>());

                    // Check if a node with the same name already exists in partUids
                    // Here we could make an optimization!!!
                    Hyperedges partUids(componentsOf(Hyperedges{modelUid}, nodeName));
                    if (!partUids.size())
                    {
                        // Instantiate new subcomponent
                        // We need to find a component class named <nodeModelVersion> whose superclass is <nodeModelName> and its domain is <nodeModelDomain>
                        const UniqueId templateUid(getComponentUid(nodeModelDomain, nodeModelName, nodeModelVersion));
                        if (!get(templateUid))
                        {
                            std::cout << "Cannot find model " << templateUid << " for " << nodeName << "\n";
                            continue;
                        }
                        // TODO: If the template does not exist, shall we just create it without further knowledge?
                        // TODO: Check if we should use instantiateSuperDeepFrom ...
                        partUids = unite(partUids, instantiateDeepFrom(Hyperedges{templateUid}, nodeName));
                        // Make the new instance part of this model
                        partOf(partUids, Hyperedges{modelUid});
                    }
                    // Register (possibly new) parts for later use
                    validNodeUids = unite(validNodeUids, partUids);
                }
            }
            const YAML::Node& edges(components["edges"]);
            if (edges.IsDefined())
            {
            }
            // TODO: Handle subcomponent config
        }

        // Handle interfaces
        // TODO: Check if interface is an alias of an INNER interface
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

                // Create one subclass of Drock::Interface which encodes directionality and one for the type
                const UniqueId superIfDirUid(getInterfaceUid("",ifDirection));
                createInterface(superIfDirUid, ifDirection, Hyperedges{Model::InterfaceDirectionId});
                const UniqueId superIfTypeUid(getInterfaceUid(ifType, ""));
                createInterface(superIfTypeUid, ifType, Hyperedges{Model::InterfaceTypeId});
                // While the former classes are independent, the specific interface class from which we instantiate is dependent on BOTH
                const UniqueId superIfUid(getInterfaceUid(ifType, ifDirection));
                createInterface(superIfUid, superIfUid, Hyperedges{superIfDirUid, superIfTypeUid});

                Hyperedges interfaceUids(interfacesOf(Hyperedges{modelUid}, ifName));
                if (!interfaceUids.size())
                {
                    // Create interface
                    allInterfaces = unite(allInterfaces, instantiateFrom(superIfUid, ifName));
                }
            }
            // Attach interfaces to model
            hasInterface(Hyperedges{modelUid}, allInterfaces);
        }

        // TODO: Handle other, generic properties
    }

    return true;
}

std::string Model::domainSpecificExport(const UniqueId& uid)
{
    std::stringstream ss;
    YAML::Node spec;
    if (!get(uid))
        return std::string();

    // Find all superclasses of uid
    // This includes everything upwards (domain, type, etc.)
    Hyperedges superUids(subclassesOf(uid, "", TraversalDirection::DOWN));

    // Domains
    // NOTE: The domain could also be extracted from uid. But we want to be safe and query.
    Hyperedges allDomainUids(directSubclassesOf(Hyperedges{Model::DomainId}));
    Hyperedges domainUids(intersect(superUids, allDomainUids));
    if (domainUids.size() > 1)
    {
        std::cout << "Multiple domains found. Abort\n";
        return ss.str();
    }
    if (domainUids.size() < 1)
    {
        std::cout << "No domain found. Abort\n";
        return ss.str();
    }
    spec["domain"] = get(*domainUids.begin())->label();

    // Types
    Hyperedges allTypeUids(directSubclassesOf(Hyperedges{Model::ComponentTypeId}));
    Hyperedges typeUids(intersect(superUids, allTypeUids));
    if (typeUids.size() > 1)
    {
        std::cout << "Multiple types found. Abort\n";
        return ss.str();
    }
    if (typeUids.size() < 1)
    {
        std::cout << "No type found. Abort\n";
        return ss.str();
    }
    spec["type"] = get(*typeUids.begin())->label();

    // Components
    Hyperedges allComponentUids(directSubclassesOf(typeUids));
    Hyperedges componentUids(intersect(superUids, allComponentUids));
    if (componentUids.size() > 1)
    {
        std::cout << "Multiple components found. Abort\n";
        return ss.str();
    }
    if (componentUids.size() < 1)
    {
        std::cout << "No component found. Abort\n";
        return ss.str();
    }
    spec["name"] = get(*componentUids.begin())->label();

    // For later: Get all interface type and direction uids
    Hyperedges ifTypeUids(directSubclassesOf(Hyperedges{Model::InterfaceTypeId}));
    Hyperedges ifDirectionUids(directSubclassesOf(Hyperedges{Model::InterfaceDirectionId}));
    // Find all versions and cycle through them (if it is a model), TODO: or only export specific one 
    YAML::Node versionsYAML(spec["versions"]);
    Hyperedges allVersions(directSubclassesOf(componentUids));
    for (const UniqueId& versionUid : allVersions)
    {
        YAML::Node versionYAML;
        versionYAML["name"] = get(versionUid)->label();

        // Query interfaces
        YAML::Node interfacesYAML(versionYAML["interfaces"]);
        Hyperedges ifs(interfacesOf(Hyperedges{versionUid}));
        for (const UniqueId& ifId : ifs)
        {
            YAML::Node interfaceYAML;
            interfaceYAML["name"] = get(ifId)->label();
            Hyperedges superIfs(instancesOf(Hyperedges{ifId}, "", TraversalDirection::DOWN));
            for (const UniqueId& suid : superIfs)
            {
                Hyperedges superSuperIfs(directSubclassesOf(Hyperedges{suid}, "", TraversalDirection::DOWN));
                interfaceYAML["type"] = get(*(intersect(superSuperIfs, ifTypeUids).begin()))->label();
                interfaceYAML["direction"] = get(*(intersect(superSuperIfs, ifDirectionUids).begin()))->label();
                interfacesYAML.push_back(interfaceYAML);
            }
        }

        // Handle subcomponents
        YAML::Node componentsYAML(versionYAML["components"]);
        Hyperedges partUids(componentsOf(Hyperedges{versionUid}));
        if (partUids.size())
        {
            YAML::Node nodesYAML(componentsYAML["nodes"]);
            for (const UniqueId& partUid : partUids)
            {
                YAML::Node nodeYAML;
                nodeYAML["name"] = get(partUid)->label();
                // the direct superclass is the model version
                Hyperedges versionUids(instancesOf(Hyperedges{partUid}, "", TraversalDirection::DOWN));
                nodeYAML["model"]["version"] = get(*versionUids.begin())->label();
                // the next superclasses is the model itself (NOTE: get rid of the upper models)
                Hyperedges modelUids(directSubclassesOf(versionUids, "", TraversalDirection::DOWN));
                modelUids = subtract(modelUids, Hyperedges{Model::ComponentId, Component::Network::ComponentId});
                nodeYAML["model"]["name"] = get(*modelUids.begin())->label();
                // and the next superclasses are the type and the domain
                Hyperedges modelDomainUids(intersect(directSubclassesOf(modelUids, "", TraversalDirection::DOWN), allDomainUids));
                nodeYAML["model"]["domain"] = get(*modelDomainUids.begin())->label();
                nodesYAML.push_back(nodeYAML);
            }
            // For every pair of parts, we have to save the relations and interconnections as well.
        }

        versionsYAML.push_back(versionYAML);
    }

    // TODO: Handle config and other unmodeled stuff

    ss << spec;
    return ss.str();
}


}
