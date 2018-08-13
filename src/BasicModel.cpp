#include "BasicModel.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>

// Import other domains
#include "SoftwareGraph.hpp"
#include "HardwareComputationalNetwork.hpp"

namespace Drock {

const UniqueId Model::DomainId = "Drock::Model::Domain";
const UniqueId Model::ComponentId = "Drock::Model::Component";
const UniqueId Model::ComponentTypeId = "Drock::Model::Component::Type";
const UniqueId Model::InterfaceId = "Drock::Model::Interface";
const UniqueId Model::InterfaceDirectionId = "Drock::Model::Interface::Direction";
const UniqueId Model::InterfaceTypeId = "Drock::Model::Interface::Type";
const UniqueId Model::EdgeTypeId = "Drock::Model::Relation";
const UniqueId Model::ConfigurationId = "Drock::Model::Configuration";
const UniqueId Model::HasConfigId = "Drock::Model::Relation::HasConfig";

void Model::setupMetaModel()
{
    // Import meta models from other domains
    Software::Graph sg;
    Hardware::Computational::Network hcn;
    importFrom(sg);
    importFrom(hcn);
    
    // Setup meta model
    // NOTE: We want to handle the basic model which is also stored in the DROCK DB ... therefore we will
    // NOT model the classes in the system_modelling library
    
    // The domain specifies to which view a component belongs
    // It is a class but not a component class!
    create(Model::DomainId, "Domain");

    // The configuration specifies all entity specific values to a certain component, interface or connection
    // It is not a component!
    create(Model::ConfigurationId, "Config");

    // Create a meta component class for DROCK components
    createComponent(Model::ComponentId, "Component");
    createComponent(Model::ComponentTypeId, "Type", Hyperedges{Model::ComponentId});
    // TODO: Versions?

    // Create a meta interface class for DROCK interfaces
    createInterface(Model::InterfaceId, "Interface");
    createInterface(Model::InterfaceDirectionId, "Direction", Hyperedges{Model::InterfaceId});
    createInterface(Model::InterfaceTypeId, "Type", Hyperedges{Model::InterfaceId});

    // Create domain specific subrelations
    subrelationFrom(Model::HasConfigId, Hyperedges{Model::ComponentId}, Hyperedges{Model::ConfigurationId}, CommonConceptGraph::HasAId);
    // Predefine some known/expected domains
    createSubclassOf(getDomainUid("SOFTWARE"), Hyperedges{Model::DomainId}, "SOFTWARE"); // all component models of the SOFTWARE domain can be Software::Graph::Algorithms
    createSubclassOf(getDomainUid("COMPUTATION"), Hyperedges{Model::DomainId}, "COMPUTATION"); // all component models of the COMPUATION domain can be either a DEVICE, PROCESSOR or BUS
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

bool Model::isInput(const UniqueId& interfaceDirUid)
{
    return ((interfaceDirUid == getInterfaceUid("","INCOMING")) || (interfaceDirUid == getInterfaceUid("","BIDIRECTIONAL")) ? true : false);
}

bool Model::isOutput(const UniqueId& interfaceDirUid)
{
    return ((interfaceDirUid == getInterfaceUid("","OUTGOING")) || (interfaceDirUid == getInterfaceUid("","BIDIRECTIONAL")) ? true : false);
}

bool Model::inSoftwareDomain(const UniqueId& domainUid)
{
    return (domainUid == getDomainUid("SOFTWARE") ? true : false);
}

UniqueId Model::getDomainUid(const std::string& domain)
{
    return Model::DomainId+"::"+domain;
}

UniqueId Model::getTypeUid(const std::string& type)
{
    return Model::ComponentTypeId+"::"+type;
}

UniqueId Model::getEdgeUid(const std::string& type)
{
    return Model::EdgeTypeId+"::"+type;
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

Hyperedges Model::instantiateConfigOnce(const Hyperedges& parentUids, const std::string& label)
{
    Hyperedges result;
    // Restriction: Allow only one config per parent
    for (const UniqueId& parentUid : parentUids)
    {
        Hyperedges existingConfigUids(configsOf(Hyperedges{parentUid}));
        if (!existingConfigUids.size())
        {
            Hyperedges newConfigUids(instantiateFrom(Hyperedges{Model::ConfigurationId}, label));
            hasConfig(Hyperedges{parentUid}, newConfigUids);
            result = unite(result, newConfigUids);
            continue;
        }
        // If config exists, we update its label
        // NOTE: Maybe we should extend the label by concatenation?
        for (const UniqueId& configUid : existingConfigUids)
        {
            get(configUid)->updateLabel(label);
        }
    }
    return result;
}

Hyperedges Model::hasConfig(const Hyperedges& parentUids, const Hyperedges& childrenUids)
{
    Hyperedges result;
    for (const UniqueId& parentId : parentUids)
    {
        for (const UniqueId& childId : childrenUids)
        {
            result = unite(result, factFrom(Hyperedges{parentId}, Hyperedges{childId}, Model::HasConfigId));
        }
    }
    return result;
}

Hyperedges Model::configsOf(const Hyperedges& uids, const std::string& label)
{
    // TODO: Handle query direction!
    Hyperedges myChildren(childrenOf(uids, label));
    Hyperedges allConfigs(to(factsOf(Hyperedges{Model::HasConfigId}), label));
    return intersect(myChildren, allConfigs);
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
    // Link to lower meta models
    if (inSoftwareDomain(domainUid))
        isA(Hyperedges{superUid}, Hyperedges{Software::Graph::AlgorithmId});
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

        // Handle subcomponents & their interconnection. Create only if non-existing.
        Hyperedges validNodeUids;
        const YAML::Node& components(version["components"]);
        if (components.IsDefined())
        {
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
                        partUids = unite(partUids, instantiateComponent(Hyperedges{templateUid}, nodeName));
                        // Make the new instance part of this model
                        partOf(partUids, Hyperedges{modelUid});
                    }
                    // Register (possibly new) parts for later use
                    validNodeUids = unite(validNodeUids, partUids);
                }
            }
            Hyperedges validEdgeUids;
            const YAML::Node& edges(components["edges"]);
            if (edges.IsDefined())
            {
                for (auto eit = edges.begin(); eit != edges.end(); eit++)
                {
                    const YAML::Node& edge(*eit);
                    const std::string& edgeName(edge["name"].as<std::string>());
                    std::string edgeType("NOT_SET");
                    if (edge["type"].IsDefined())
                    {
                        edgeType = edge["type"].as<std::string>();
                    }
                    const YAML::Node &from( edge["from"] );
                    const YAML::Node &to( edge["to"] );
                    if( !from.IsDefined() || !to.IsDefined() )
                    {
                        std::cout << "Edge " << edgeName << " has no to or from entry\n";
                        continue;
                    }
                    const std::string& sourceNodeName(from["name"].as<std::string>());
                    //const std::string& sourceNodeDomain(from["domain"].as<std::string>());
                    const std::string& targetNodeName(to["name"].as<std::string>());
                    //const std::string& targetNodeDomain(to["domain"].as<std::string>());
                    // Check if edge is a true (interdomain) edge or a edge-connector-edge construct
                    bool isInterDomainEdge = (edgeType == "NOT_SET" ? false : true);
                    if (isInterDomainEdge)
                    {
                        // This edge is a relation which we can directly model.
                        const UniqueId& relUid(getEdgeUid(edgeType));
                        if (!get(relUid))
                        {
                            std::cout << "Don't know relation of type " << edgeType << "\n";
                            continue;
                        }
                        // Get all facts from relUid by name
                        Hyperedges factUids(factsOf(relUid, edgeName));
                        // Lookup entities to relate from and to
                        for (const UniqueId& fromUid : validNodeUids)
                        {
                            if (get(fromUid)->label() != sourceNodeName)
                                continue;
                            Hyperedges relsFromUids(relationsFrom(Hyperedges{fromUid}, edgeName));
                            for (const UniqueId& toUid : validNodeUids)
                            {
                                if (get(toUid)->label() != targetNodeName)
                                    continue;
                                Hyperedges relsToUids(relationsTo(Hyperedges{toUid}, edgeName));
                                Hyperedges possibleCandidateUids(intersect(factUids, intersect(relsFromUids, relsToUids)));
                                if (!possibleCandidateUids.size())
                                {
                                    Hyperedges factUid(factFrom(Hyperedges{fromUid}, Hyperedges{toUid}, Hyperedges{relUid}));
                                    get(*factUid.begin())->updateLabel(edgeName);
                                    possibleCandidateUids = unite(possibleCandidateUids, factUid);
                                }
                                // Register (possibly new) edges for later use
                                validEdgeUids = unite(validEdgeUids, possibleCandidateUids);
                            }
                        }
                    } else {
                        // These edges are based on interfaces. We can model them via ConnectedToInterfaceId.
                        const std::string& sourceInterfaceName(from["interface"].as<std::string>());
                        const std::string& targetInterfaceName(to["interface"].as<std::string>());
                        // Get all facts from relUid by name
                        Hyperedges factUids(factsOf(Component::Network::ConnectedToInterfaceId, edgeName));
                        // Lookup entities to relate from and to
                        for (const UniqueId& fromUid : validNodeUids)
                        {
                            if (get(fromUid)->label() != sourceNodeName)
                                continue;
                            Hyperedges fromInterfaceUids(interfacesOf(Hyperedges{fromUid}, sourceInterfaceName));
                            Hyperedges relsFromUids(relationsFrom(fromInterfaceUids, edgeName));
                            for (const UniqueId& toUid : validNodeUids)
                            {
                                if (get(toUid)->label() != targetNodeName)
                                    continue;
                                Hyperedges toInterfaceUids(interfacesOf(Hyperedges{toUid}, targetInterfaceName));
                                Hyperedges relsToUids(relationsTo(toInterfaceUids, edgeName));
                                Hyperedges possibleCandidateUids(intersect(factUids, intersect(relsFromUids, relsToUids)));
                                if (!possibleCandidateUids.size())
                                {
                                    Hyperedges connUids(connectInterface(fromInterfaceUids, toInterfaceUids));
                                    for (const UniqueId& connUid : connUids)
                                        get(connUid)->updateLabel(edgeName);
                                    possibleCandidateUids = unite(possibleCandidateUids, connUids);
                                }
                                // Register (possibly new) edges for later use
                                validEdgeUids = unite(validEdgeUids, possibleCandidateUids);
                            }
                        }
                    }
                }
            }
            // Handle subcomponent config
            const YAML::Node& config(components["configuration"]);
            if (config.IsDefined())
            {
                const YAML::Node& nodesConfig(config["nodes"]);
                const YAML::Node& edgesConfig(config["edges"]);
                if (nodesConfig.IsDefined())
                {
                    for (auto nit = nodesConfig.begin(); nit != nodesConfig.end(); nit++)
                    {
                        const YAML::Node& nodeConfig(*nit);
                        const std::string& nodeName(nodeConfig["name"].as<std::string>());
                        const std::string& nodeData(nodeConfig["data"].as<std::string>());
                        // Find the node! It is somewhere in the parts ...
                        for (const UniqueId& partUid : validNodeUids)
                        {
                            if (get(partUid)->label() != nodeName)
                                continue;
                            // Found one, apply config
                            instantiateConfigOnce(Hyperedges{partUid}, nodeData);
                        }

                        // TODO: Shall we follow the submodel chain? That means that we might have to use instantiateSuperDeepFrom!
                    }
                }
                if (edgesConfig.IsDefined())
                {
                    for (auto eit = edgesConfig.begin(); eit != edgesConfig.end(); eit++)
                    {
                        const YAML::Node& edgeConfig(*eit);
                        const std::string& edgeName(edgeConfig["name"].as<std::string>());
                        const std::string& edgeData(edgeConfig["data"].as<std::string>());
                        // Find the realtion. It is somewhere in the edges
                        for (const UniqueId& relUid : validEdgeUids)
                        {
                            if (get(relUid)->label() != edgeName)
                                continue;
                            // Found one, apply config
                            instantiateConfigOnce(Hyperedges{relUid}, edgeData);
                        }

                        // TODO: Shall we follow the submodel chain? That means that we might have to use instantiateSuperDeepFrom!
                    }
                }
            }
        }

        // Handle (alias) interfaces
        const YAML::Node& ifs(version["interfaces"]);
        if (ifs.IsDefined())
        {
            Hyperedges allInterfaces;
            for (auto ifIt = ifs.begin(); ifIt != ifs.end(); ifIt++)
            {
                const YAML::Node& interfaceYAML(*ifIt);
                const std::string& ifName(interfaceYAML["name"].as<std::string>());
                const std::string& ifType(interfaceYAML["type"].as<std::string>());
                const std::string& ifDirection(interfaceYAML["direction"].as<std::string>());

                // Check if interface already exists.
                Hyperedges interfaceUids(interfacesOf(Hyperedges{modelUid}, ifName));
                if (interfaceUids.size())
                {
                    // Interface already exists, so ignore it.
                    continue;
                }

                // Create one subclass of Drock::Interface which encodes directionality and one for the type
                const UniqueId superIfDirUid(getInterfaceUid("",ifDirection));
                createInterface(superIfDirUid, ifDirection, Hyperedges{Model::InterfaceDirectionId});
                const UniqueId superIfTypeUid(getInterfaceUid(ifType, ""));
                createInterface(superIfTypeUid, ifType, Hyperedges{Model::InterfaceTypeId});
                // While the former classes are independent, the specific interface class from which we instantiate is dependent on BOTH
                const UniqueId superIfUid(getInterfaceUid(ifType, ifDirection));
                createInterface(superIfUid, ifName, Hyperedges{superIfDirUid, superIfTypeUid});
                // Link to lower meta models 
                if (inSoftwareDomain(domainUid))
                {
                    isA(Hyperedges{superIfUid}, Hyperedges{Software::Graph::InterfaceId});
                    isA(Hyperedges{superIfTypeUid}, Hyperedges{superIfUid});
                    if (isInput(superIfDirUid))
                        isA(Hyperedges{superIfUid}, Hyperedges{Software::Graph::InputId});
                    if (isOutput(superIfDirUid))
                        isA(Hyperedges{superIfUid}, Hyperedges{Software::Graph::OutputId});
                }

                // Get alias information
                std::string interfaceLinkNodeName;
                if (interfaceYAML["linkToNode"].IsDefined())
                    interfaceLinkNodeName = interfaceYAML["linkToNode"].as<std::string>();
                std::string interfaceLinkInterfaceName;
                if (interfaceYAML["linkToInterface"].IsDefined())
                    interfaceLinkInterfaceName = interfaceYAML["linkToInterface"].as<std::string>();

                // Create an alias interface if needed
                if (!interfaceLinkNodeName.empty() && !interfaceLinkInterfaceName.empty())
                {
                    // Find node somwhere in parts
                    for (const UniqueId& partUid : validNodeUids)
                    {
                        if (get(partUid)->label() != interfaceLinkNodeName)
                            continue;
                        // Found. Find all interfaces with given name.
                        Hyperedges interfaceUids(interfacesOf(Hyperedges{partUid}, interfaceLinkInterfaceName));
                        allInterfaces = unite(allInterfaces, instantiateAliasInterfaceFor(Hyperedges{modelUid}, interfaceUids, ifName));
                    }
                } else {
                    // Create normal interface
                    allInterfaces = unite(allInterfaces, instantiateInterfaceFor(Hyperedges{modelUid}, Hyperedges{superIfUid}, ifName));
                }
            }
        }

        // Handle default configuration
        const YAML::Node& defaultConfig(version["defaultConfiguration"]);
        if (defaultConfig.IsDefined() && defaultConfig["name"].IsDefined() && defaultConfig["data"].IsDefined())
        {
            const std::string& name(defaultConfig["name"].as<std::string>());
            const std::string& data(defaultConfig["data"].as<std::string>());
            instantiateConfigOnce(Hyperedges{modelUid}, data);
        }

        // TODO: Handle other, generic properties (e.g. repository and so forth)
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
    Hyperedges superUids(subclassesOf(uid, "", TraversalDirection::FORWARD));

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

        // Handle subcomponents
        YAML::Node componentsYAML(versionYAML["components"]);
        YAML::Node configurationYAML(versionYAML["configuration"]);
        Hyperedges partUids(componentsOf(Hyperedges{versionUid}));
        if (partUids.size())
        {
            YAML::Node nodesYAML(componentsYAML["nodes"]);
            YAML::Node nodeConfigsYAML(configurationYAML["nodes"]);
            for (const UniqueId& partUid : partUids)
            {
                YAML::Node nodeYAML;
                nodeYAML["name"] = get(partUid)->label();
                // the direct superclass is the model version
                Hyperedges versionUids(instancesOf(Hyperedges{partUid}, "", TraversalDirection::FORWARD));
                nodeYAML["model"]["version"] = get(*versionUids.begin())->label();
                // the next superclasses is the model itself (NOTE: get rid of the upper models)
                Hyperedges modelUids(directSubclassesOf(versionUids, "", TraversalDirection::FORWARD));
                modelUids = subtract(modelUids, Hyperedges{Model::ComponentId, Component::Network::ComponentId});
                nodeYAML["model"]["name"] = get(*modelUids.begin())->label();
                // and the next superclasses are the type and the domain
                Hyperedges modelDomainUids(intersect(directSubclassesOf(modelUids, "", TraversalDirection::FORWARD), allDomainUids));
                nodeYAML["model"]["domain"] = get(*modelDomainUids.begin())->label();
                nodesYAML.push_back(nodeYAML);

                // Get configurations
                Hyperedges configUids(configsOf(Hyperedges{partUid}));
                for (const UniqueId& configUid : configUids)
                {
                    YAML::Node nodeConfigYAML;
                    nodeConfigYAML["name"] = get(partUid)->label();
                    nodeConfigYAML["data"] = get(configUid)->label();
                    nodeConfigsYAML.push_back(nodeConfigYAML);
                }
            }
            // For every pair of parts, we have to save the relations and interconnections as well.
            YAML::Node edgesYAML(componentsYAML["edges"]);
            YAML::Node edgeConfigsYAML(configurationYAML["edges"]);
            for (const UniqueId& fromUid : partUids)
            {
                Hyperedges relsFromUids(relationsFrom(Hyperedges{fromUid}));
                Hyperedges fromInterfaceUids(interfacesOf(Hyperedges{fromUid}));
                for (const UniqueId& toUid : partUids)
                {
                    Hyperedges relsToUids(relationsTo(Hyperedges{toUid}));
                    Hyperedges toInterfaceUids(interfacesOf(Hyperedges{toUid}));
                    // First: store all normal relations
                    Hyperedges commonRelUids(intersect(relsFromUids, relsToUids));
                    for (const UniqueId& commonUid : commonRelUids)
                    {
                        YAML::Node edgeYAML;
                        // Find type
                        Hyperedges edgeTypeUids(factsOf(Hyperedges{commonUid}, "", TraversalDirection::FORWARD));
                        edgeYAML["type"] = get(*edgeTypeUids.begin())->label();
                        edgeYAML["name"] = get(commonUid)->label();
                        edgeYAML["from"]["name"] = get(fromUid)->label();
                        edgeYAML["to"]["name"] = get(toUid)->label();
                        edgesYAML.push_back(edgeYAML);
                        // Get configurations
                        Hyperedges configUids(configsOf(Hyperedges{commonUid}));
                        for (const UniqueId& configUid : configUids)
                        {
                            YAML::Node edgeConfigYAML;
                            edgeConfigYAML["name"] = get(commonUid)->label();
                            edgeConfigYAML["data"] = get(configUid)->label();
                            edgeConfigsYAML.push_back(edgeConfigYAML);
                        }
                    }
                    // Second: store all connect relations between interfaces
                    for (const UniqueId& fromInterfaceUid : fromInterfaceUids)
                    {
                        Hyperedges relsFromInterfaceUids(relationsFrom(Hyperedges{fromInterfaceUid}));
                        for (const UniqueId& toInterfaceUid : toInterfaceUids)
                        {
                            Hyperedges relsToInterfaceUids(relationsTo(Hyperedges{toInterfaceUid}));
                            Hyperedges commonInterfaceRelUids(intersect(relsFromInterfaceUids, relsToInterfaceUids));
                            for (const UniqueId& commonUid : commonInterfaceRelUids)
                            {
                                YAML::Node edgeYAML;
                                edgeYAML["name"] = get(commonUid)->label();
                                edgeYAML["type"] = "NOT_SET";
                                edgeYAML["from"]["name"] = get(fromUid)->label();
                                edgeYAML["from"]["interface"] = get(fromInterfaceUid)->label();
                                edgeYAML["to"]["name"] = get(toUid)->label();
                                edgeYAML["to"]["interface"] = get(toInterfaceUid)->label();
                                edgesYAML.push_back(edgeYAML);
                                // Get configurations
                                Hyperedges configUids(configsOf(Hyperedges{commonUid}));
                                for (const UniqueId& configUid : configUids)
                                {
                                    YAML::Node edgeConfigYAML;
                                    edgeConfigYAML["name"] = get(commonUid)->label();
                                    edgeConfigYAML["data"] = get(configUid)->label();
                                    edgeConfigsYAML.push_back(edgeConfigYAML);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Query interfaces
        YAML::Node interfacesYAML(versionYAML["interfaces"]);
        Hyperedges ifs(interfacesOf(Hyperedges{versionUid}));
        for (const UniqueId& ifId : ifs)
        {
            YAML::Node interfaceYAML;
            interfaceYAML["name"] = get(ifId)->label();
            Hyperedges superIfs(instancesOf(Hyperedges{ifId}, "", TraversalDirection::FORWARD));
            // Check if it is an alias interface
            Hyperedges originalInterfaceUids(originalInterfacesOf(Hyperedges{ifId}));
            // handle interface type and direction
            for (const UniqueId& suid : superIfs)
            {
                Hyperedges superSuperIfs(directSubclassesOf(Hyperedges{suid}, "", TraversalDirection::FORWARD));
                interfaceYAML["type"] = get(*(intersect(superSuperIfs, ifTypeUids).begin()))->label();
                interfaceYAML["direction"] = get(*(intersect(superSuperIfs, ifDirectionUids).begin()))->label();
                if (!originalInterfaceUids.size())
                {
                    interfacesYAML.push_back(interfaceYAML);
                    continue;
                }
                // Store alias interface info
                for (const UniqueId& originalInterfaceUid : originalInterfaceUids)
                {
                    interfaceYAML["linkToInterface"] = get(originalInterfaceUid)->label();
                    Hyperedges ownerUids(interfacesOf(Hyperedges{originalInterfaceUid}, "", TraversalDirection::INVERSE));
                    for (const UniqueId& ownerUid : ownerUids)
                    {
                        interfaceYAML["linkToNode"] = get(ownerUid)->label();
                        interfacesYAML.push_back(interfaceYAML);
                    }
                }
            }
        }

        // Store default configuration
        YAML::Node defaultConfigYAML(versionYAML["defaultConfiguration"]);
        Hyperedges configUids(configsOf(Hyperedges{versionUid}));
        for (const UniqueId& configUid : configUids)
        {
            YAML::Node configYAML;
            configYAML["name"] = get(versionUid)->label();
            configYAML["data"] = get(configUid)->label();
            defaultConfigYAML.push_back(configYAML);
        }

        versionsYAML.push_back(versionYAML);
    }

    ss << spec;
    return ss.str();
}


}
