#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpExampleTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create (2);

    // Ensure 2 nodes are created
    NS_ASSERT_MSG(nodes.GetN() == 2, "Total 2 nodes should be created.");
}

// Test 2: Verify Point-to-Point Link Setup
void TestPointToPointLink()
{
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices = pointToPoint.Install (nodes);

    // Ensure point-to-point link is created between nodes
    NS_ASSERT_MSG(devices.GetN() == 2, "There should be 2 devices (one for each node) in the link.");
}

// Test 3: Verify Internet Stack Installation
void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create (2);

    InternetStackHelper stack;
    stack.Install (nodes);

    // Ensure Internet stack is installed on both nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        NS_ASSERT_MSG(ipv4 != nullptr, "Internet stack should be installed on node " + std::to_string(i));
    }
}

// Test 4: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer devices = pointToPoint.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Ensure IP addresses are assigned correctly to both nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        NS_ASSERT_MSG(interfaces.GetAddress(i) != Ipv4Address("0.0.0.0"), "IP address not assigned to node " + std::to_string(i));
    }
}

// Test 5: Verify TCP Server Installation on Node 1
void TestTcpServerInstallation()
{
    NodeContainer nodes;
    nodes.Create (2);

    uint16_t port = 8080;
    Address serverAddress (InetSocketAddress ("10.1.1.2", port)); // Assuming node 1 gets address 10.1.1.2
    PacketSinkHelper tcpServerHelper ("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = tcpServerHelper.Install (nodes.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // Ensure the server application is installed on node 1
    NS_ASSERT_MSG(serverApp.GetN() == 1, "TCP server should be installed on node 1.");
}

// Test 6: Verify TCP Client Installation on Node 0
void TestTcpClientInstallation()
{
    NodeContainer nodes;
    nodes.Create (2);

    uint16_t port = 8080;
    Address serverAddress (InetSocketAddress ("10.1.1.2", port)); // Assuming node 1 gets address 10.1.1.2

    OnOffHelper tcpClientHelper ("ns3::TcpSocketFactory", serverAddress);
    tcpClientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    tcpClientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    tcpClientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
    tcpClientHelper.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApp = tcpClientHelper.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0)); // Start client after 2 seconds
    clientApp.Stop (Seconds (10.0)); // Stop after 10 seconds

    // Ensure the client application is installed on node 0
    NS_ASSERT_MSG(clientApp.GetN() == 1, "TCP client should be installed on node 0.");
}

// Test 7: Verify Flow Monitor Installation
void TestFlowMonitorInstallation()
{
    NodeContainer nodes;
    nodes.Create (2);

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

    // Ensure flow monitor is installed
    NS_ASSERT_MSG(monitor != nullptr, "Flow monitor should be installed.");
}

// Test 8: Verify Flow Statistics Output
void TestFlowStatistics()
{
    NodeContainer nodes;
    nodes.Create (2);

    // Run the simulation
    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();
    
    // Ensure flow statistics are available
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    NS_ASSERT_MSG(stats.size() > 0, "Flow statistics should not be empty.");

    // Example check: Ensure at least one flow exists
    NS_ASSERT_MSG(stats.begin() != stats.end(), "No flow statistics collected.");
    
    Simulator::Destroy ();
}

// Test 9: Verify Flow Monitor Serialization
void TestFlowMonitorSerialization()
{
    NodeContainer nodes;
    nodes.Create (2);

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();
    
    // Serialize flow monitor results to XML file
    std::string filename = "tcp-flow-monitor.xml";
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
    TestPointToPointLink();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestTcpServerInstallation();
    TestTcpClientInstallation();
    TestFlowMonitorInstallation();
    TestFlowStatistics();
    TestFlowMonitorSerialization();

    return 0;
}
