#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaExample");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Create a CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(2)));

    // Install CSMA on the nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Print confirmation
    NS_LOG_INFO("CSMA devices installed on nodes.");

    // Add internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("Assigned IP addresses to interfaces.");

    // Enable global static routing, so any node can reach any other node
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}