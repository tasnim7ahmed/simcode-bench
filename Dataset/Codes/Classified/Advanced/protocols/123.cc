#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OSPFExample");

int main(int argc, char *argv[])
{
    // Enable logging for debugging
    LogComponentEnable("OSPFExample", LOG_LEVEL_INFO);

    // Step 1: Create four nodes in a square topology
    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(4);

    // Step 2: Set up point-to-point links between the nodes
    NS_LOG_INFO("Creating point-to-point links.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesAB, devicesBC, devicesCD, devicesDA;
    devicesAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // Link between node 0 and node 1
    devicesBC = pointToPoint.Install(nodes.Get(1), nodes.Get(2)); // Link between node 1 and node 2
    devicesCD = pointToPoint.Install(nodes.Get(2), nodes.Get(3)); // Link between node 2 and node 3
    devicesDA = pointToPoint.Install(nodes.Get(3), nodes.Get(0)); // Link between node 3 and node 0

    // Step 3: Install the internet stack with OSPF routing
    NS_LOG_INFO("Installing internet stack.");
    OspfHelper ospf; // Helper to set up OSPF
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospf, 10); // Add OSPF with priority 10

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting); // Set the routing protocol to OSPF
    internet.Install(nodes);

    // Step 4: Assign IP addresses to the nodes
    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = ipv4.Assign(devicesAB);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = ipv4.Assign(devicesBC);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCD = ipv4.Assign(devicesCD);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesDA = ipv4.Assign(devicesDA);

    // Step 5: Set up UDP communication
    NS_LOG_INFO("Setting up UDP communication.");
    uint16_t port = 9; // Port number for the UDP application

    // UDP Server on node 3
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on node 0
    UdpClientHelper udpClient(interfacesCD.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(5));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Step 6: Enable packet tracing
    NS_LOG_INFO("Enabling packet tracing.");
    pointToPoint.EnablePcapAll("ospf-routing-example");

    // Step 7: Display the routing tables
    NS_LOG_INFO("Printing routing tables.");
    for (uint32_t i = 0; i < nodes.GetN(); i++)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        ipv4->GetRoutingProtocol()->PrintRoutingTable(Create<OutputStreamWrapper>(&std::cout));
    }

    // Step 8: Run the simulation
    NS_LOG_INFO("Running the simulation.");
    Simulator::Run();

    // Step 9: Clean up and destroy the simulation
    NS_LOG_INFO("Destroying the simulation.");
    Simulator::Destroy();
    return 0;
}

