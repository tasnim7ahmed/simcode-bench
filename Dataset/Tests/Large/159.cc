#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologyExampleTest");

// Test 1: Verify Node Creation for Star and Bus Topology
void TestNodeCreation()
{
    NodeContainer starNodes;
    starNodes.Create(4);  // 1 central node and 3 peripheral nodes
    NodeContainer busNodes;
    busNodes.Create(3);   // 3 nodes in bus topology

    // Ensure the correct number of nodes are created
    NS_ASSERT_MSG(starNodes.GetN() == 4, "Star topology should have 4 nodes.");
    NS_ASSERT_MSG(busNodes.GetN() == 3, "Bus topology should have 3 nodes.");
}

// Test 2: Verify Point-to-Point Link Setup for Star Topology
void TestStarTopologyLinks()
{
    NodeContainer starNodes;
    starNodes.Create(4); // 1 central node and 3 peripheral nodes

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices;
    for (uint32_t i = 1; i < starNodes.GetN(); i++)
    {
        starDevices.Add(p2p.Install(starNodes.Get(0), starNodes.Get(i)));
    }

    // Ensure correct number of point-to-point devices are installed for the star topology
    NS_ASSERT_MSG(starDevices.GetN() == 3, "There should be 3 links in the star topology.");
}

// Test 3: Verify Point-to-Point Link Setup for Bus Topology
void TestBusTopologyLinks()
{
    NodeContainer busNodes;
    busNodes.Create(3);  // 3 nodes in bus topology

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer busDevices;
    busDevices.Add(p2p.Install(busNodes.Get(0), busNodes.Get(1)));
    busDevices.Add(p2p.Install(busNodes.Get(1), busNodes.Get(2)));

    // Ensure correct number of point-to-point devices are installed for the bus topology
    NS_ASSERT_MSG(busDevices.GetN() == 2, "There should be 2 links in the bus topology.");
}

// Test 4: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer starNodes;
    starNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices = p2p.Install(starNodes);
    NetDeviceContainer busDevices = p2p.Install(busNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(starDevices);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(busDevices);

    // Ensure that IP addresses are assigned correctly
    for (uint32_t i = 0; i < starNodes.GetN(); ++i)
    {
        NS_ASSERT_MSG(ipv4.GetAddress(i).IsGlobal(), "Node " + std::to_string(i) + " should have a global IP address.");
    }

    for (uint32_t i = 0; i < busNodes.GetN(); ++i)
    {
        NS_ASSERT_MSG(ipv4.GetAddress(i + 3).IsGlobal(), "Node " + std::to_string(i + 3) + " should have a global IP address.");
    }
}

// Test 5: Verify UDP Echo Server Installation on Star Central Node
void TestUdpEchoServer()
{
    NodeContainer starNodes;
    starNodes.Create(4);

    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApp = echoServer.Install(starNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Ensure the server application is installed on the central node (node 0)
    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed on the central node.");
}

// Test 6: Verify UDP Echo Client Installation on Bus Nodes
void TestUdpEchoClient()
{
    NodeContainer busNodes;
    busNodes.Create(3);

    UdpEchoClientHelper echoClient("10.1.1.1", 9); // Central node in star topology
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient.Install(busNodes.Get(0))); // Client on bus node 4
    clientApps.Add(echoClient.Install(busNodes.Get(1))); // Client on bus node 5
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Ensure the client applications are installed on the bus nodes
    NS_ASSERT_MSG(clientApps.GetN() == 2, "UDP Echo Clients should be installed on 2 bus nodes.");
}

// Test 7: Verify Flow Monitor Installation
void TestFlowMonitorInstallation()
{
    NodeContainer starNodes;
    starNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(3);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Ensure the flow monitor is installed
    NS_ASSERT_MSG(monitor != nullptr, "Flow monitor should be installed.");
}

// Test 8: Verify Flow Statistics Output
void TestFlowStatistics()
{
    NodeContainer starNodes;
    starNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(3);

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Ensure flow statistics are available
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    NS_ASSERT_MSG(stats.size() > 0, "Flow statistics should not be empty.");

    // Example check: Ensure at least one flow exists
    NS_ASSERT_MSG(stats.begin() != stats.end(), "No flow statistics collected.");
    
    Simulator::Destroy();
}

// Test 9: Verify Flow Monitor Serialization to XML
void TestFlowMonitorSerialization()
{
    NodeContainer starNodes;
    starNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(3);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Serialize flow monitor results to XML file
    std::string filename = "hybrid-topology.flowmon";
    monitor->SerializeToXmlFile(filename, true, true);

    // Check if the XML file exists and contains data
    std::ifstream inFile(filename);
    NS_ASSERT_MSG(inFile.is_open(), "Flow monitor XML file should be created and openable.");
    inFile.close();
}

int main (int argc, char *argv[])
{
    // Run individual tests
    TestNodeCreation();
    TestStarTopologyLinks();
    TestBusTopologyLinks();
    TestIpAddressAssignment();
    TestUdpEchoServer();
    TestUdpEchoClient();
    TestFlowMonitorInstallation();
    TestFlowStatistics();
    TestFlowMonitorSerialization();

    return 0;
}
