#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyDatasetTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(4);

    // Ensure 4 nodes are created for the ring topology
    NS_ASSERT_MSG(nodes.GetN() == 4, "Total 4 nodes should be created in the ring topology.");
}

// Test 2: Verify Point-to-Point Links Setup
void TestPointToPointLinks()
{
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices.Add(p2p.Install(nodes.Get(0), nodes.Get(1)));
    devices.Add(p2p.Install(nodes.Get(1), nodes.Get(2)));
    devices.Add(p2p.Install(nodes.Get(2), nodes.Get(3)));
    devices.Add(p2p.Install(nodes.Get(3), nodes.Get(0))); // Create the ring

    // Ensure point-to-point links are established (4 links)
    NS_ASSERT_MSG(devices.GetN() == 4, "Four point-to-point links should be established.");
}

// Test 3: Verify Internet Stack Installation
void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    InternetStackHelper internet;
    internet.Install(nodes);

    // Verify Internet stack installation on all nodes
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
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices.Add(p2p.Install(nodes.Get(0), nodes.Get(1)));
    devices.Add(p2p.Install(nodes.Get(1), nodes.Get(2)));
    devices.Add(p2p.Install(nodes.Get(2), nodes.Get(3)));
    devices.Add(p2p.Install(nodes.Get(3), nodes.Get(0)));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Verify IP addresses are assigned correctly
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        NS_ASSERT_MSG(interfaces.GetAddress(i) != Ipv4Address("0.0.0.0"), "IP address not assigned for node " + std::to_string(i));
    }
}

// Test 5: Verify UDP Server Installation on Node 0
void TestUdpServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Ensure the UDP server is installed on node 0
    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP server should be installed on node 0.");
}

// Test 6: Verify UDP Client Installation on Nodes 1-3
void TestUdpClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    uint16_t port = 9;
    UdpClientHelper udpClient("10.1.1.1", port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        clientApps.Add(udpClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // Ensure UDP clients are installed on nodes 1-3
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        Ptr<Application> app = nodes.Get(i)->GetApplication(0);
        NS_ASSERT_MSG(app != nullptr, "UDP client should be installed on node " + std::to_string(i));
    }
}

// Test 7: Verify Flow Monitor Installation
void TestFlowMonitorInstallation()
{
    NodeContainer nodes;
    nodes.Create(4);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Ensure flow monitor is installed
    NS_ASSERT_MSG(flowMonitor != nullptr, "Flow monitor should be installed.");
}

// Test 8: Verify UDP Data Recording to CSV File
void TestUdpDataRecording()
{
    NodeContainer nodes;
    nodes.Create(4);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Record UDP data into a CSV file
    std::string filename = "udp_dataset_ring.csv";
    RecordUdpData(flowMonitor, filename);

    // Check if the CSV file exists and contains data (basic check)
    std::ifstream inFile(filename);
    NS_ASSERT_MSG(inFile.is_open(), "CSV file should be created and openable.");
    inFile.close();
}

int main(int argc, char *argv[])
{
    // Run individual tests
    TestNodeCreation();
    TestPointToPointLinks();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestUdpServerInstallation();
    TestUdpClientInstallation();
    TestFlowMonitorInstallation();
    TestUdpDataRecording();

    return 0;
}
