#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include <iostream>
#include <string>
#include <cmath> // For M_PI, cos, sin

// Define M_PI if not already defined (e.g., on Windows with MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Optional: Enable logging for relevant modules to see more details during execution
    LogComponentEnable("PointToPointHelper", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4InterfaceContainer", LOG_LEVEL_INFO);

    // Create the central node (hub)
    Ptr<Node> centralNode = CreateObject<Node>();

    // Create peripheral nodes
    NodeContainer peripheralNodes;
    int numPeripheralNodes = 3; // Example: 3 peripheral nodes
    peripheralNodes.Create(numPeripheralNodes);

    // Install the Internet stack on all nodes (central + peripheral)
    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    // Configure Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Helper for assigning IP addresses
    Ipv4AddressHelper ipv4;
    
    // Base network address for the first link, incrementing the third octet for subsequent links
    uint32_t networkOctet = 1; 

    std::cout << "--- Establishing Star Topology Connections ---" << std::endl;

    // Connect each peripheral node to the central node
    for (uint32_t i = 0; i < numPeripheralNodes; ++i)
    {
        // Get the current peripheral node
        Ptr<Node> peripheralNode = peripheralNodes.Get(i);

        // Create a NodeContainer for the current link (centralNode and current peripheralNode)
        NodeContainer linkNodes;
        linkNodes.Add(centralNode);
        linkNodes.Add(peripheralNode);

        // Install devices on the link nodes
        NetDeviceContainer linkDevices = p2p.Install(linkNodes);

        // Set the IP address base for the current link's subnet
        std::string networkAddress = "10.1." + std::to_string(networkOctet) + ".0";
        ipv4.SetBase(Ipv4Address(networkAddress.c_str()), Ipv4Mask("255.255.255.0"));
        
        // Assign IP addresses to the devices on this link
        Ipv4InterfaceContainer linkInterfaces = ipv4.Assign(linkDevices);

        // Print confirmation of the established connection and assigned IPs
        std::cout << "Link " << (i + 1) << ":" << std::endl;
        std::cout << "  Central Node (ID: " << centralNode->GetId() << ") <--> Peripheral Node (ID: " << peripheralNode->GetId() << ")" << std::endl;
        std::cout << "  Central Node IP: " << linkInterfaces.GetAddress(0) << std::endl;
        std::cout << "  Peripheral Node IP: " << linkInterfaces.GetAddress(1) << std::endl;
        
        networkOctet++; // Increment the network octet for the next link's subnet
    }

    std::cout << "--- Topology Setup Complete ---" << std::endl;

    // Optional: Enable NetAnim for visualization of the topology
    AnimationInterface anim("star-topology.xml");
    
    // Set fixed positions for NetAnim visualization
    anim.SetConstantPosition(centralNode, 50.0, 50.0); // Place central node at (50,50)
    
    // Arrange peripheral nodes in a circle around the central node
    double radius = 30.0; // Radius of the circle
    double angleIncrement = 2.0 * M_PI / numPeripheralNodes; // Angle between nodes

    for (uint32_t i = 0; i < numPeripheralNodes; ++i) {
        double angle = i * angleIncrement;
        double x = 50.0 + radius * cos(angle);
        double y = 50.0 + radius * sin(angle);
        anim.SetConstantPosition(peripheralNodes.Get(i), x, y);
    }

    // Run the simulation (even if nothing is simulated, it initializes NetAnim)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}