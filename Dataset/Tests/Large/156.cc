#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyDatasetTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    // Combine both containers
    NodeContainer allNodes;
    allNodes.Add(centralNode);
    allNodes.Add(peripheralNodes);

    // Ensure 5 nodes are created
    NS_ASSERT_MSG(allNodes.GetN() == 5, "Total 5 nodes (1 central + 4 peripherals) should be created.");
}

// Test 2: Verify Point-to-Point Links Setup
void TestPointToPointLinks()
{
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i)
    {
        devices.Add(p2p.Install(centralNode.Get(0), peripheralNodes.Get(i)));
    }

    // Ensure point-to-point links are established (i.e., 4 links)
    NS_ASSERT_MSG(devices.GetN() == 4, "Four point-to-point links should be established.");
}

// Test 3: Verify Internet Stack Installation
void TestInternetStackInstallation()
{
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    NodeContainer allNodes;
    allNodes.Add(centralNode);
    allNodes.Add(peripheralNodes);

    InternetStackHelper internet;
    internet.Install(allNodes);

    // Verify Internet stack installation on all nodes
    for (uint32_t i = 0; i < allNodes.GetN(); ++i)
    {
        Ptr<Node> node = allNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        NS_ASSERT_MSG(ipv4 != nullptr, "Internet stack should be installed on node " + std::to_string(i));
    }
}

// Test 4: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i)
    {
        devices.Add(p2p.Install(centralNode.Get(0), peripheralNodes.Get(i)));
    }

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Verify IP addresses are assigned correctly
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        NS_ASSERT_MSG(interfaces.GetAddress(i) != Ipv4Address("0.0.0.0"), "IP address not assigned for node " + std::to_string(i));
    }
}

// Test 5: Verify TCP Server Installation on Central Node
void TestTcpServerInstallation()
{
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress("10.1.1.1", port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(centralNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Ensure the TCP server is installed on the central node
    NS_ASSERT_MSG(serverApp.GetN() == 1, "TCP server should be installed on central node.");
}

// Test 6: Verify TCP Client Installation on Peripheral Nodes
void TestTcpClientInstallation()
{
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress("10.1.1.1", port));
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", serverAddress);
    onOffHelper.SetAttribute("DataRate", StringValue("500kbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i)
    {
        clientApps.Add(onOffHelper.Install(peripheralNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // Ensure TCP clients are installed on all peripheral nodes
    for (uint32_t i = 0; i < peripheralNodes.GetN(); ++i)
    {
        Ptr<Application> app = peripheralNodes.Get(i)->GetApplication(0);
        NS_ASSERT_MSG(app != nullptr, "TCP client should be installed on peripheral node " + std::to_string(i));
    }
}

// Test 7: Verify Flow Monitor Installation
void TestFlowMonitorInstallation()
{
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    NodeContainer allNodes;
    allNodes.Add(centralNode);
    allNodes.Add(peripheralNodes);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Ensure flow monitor is installed
    NS_ASSERT_MSG(flowMonitor != nullptr, "Flow monitor should be installed.");
}

// Test 8: Verify TCP Data Recording to CSV File
void TestTcpDataRecording()
{
    // Create dummy flow monitor
    NodeContainer centralNode;
    NodeContainer peripheralNodes;
    centralNode.Create(1);
    peripheralNodes.Create(4);

    NodeContainer allNodes;
    allNodes.Add(centralNode);
    allNodes.Add(peripheralNodes);

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Record TCP data into a CSV file
    std::string filename = "tcp_dataset.csv";
    RecordTcpData(flowMonitor, filename);

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
    TestTcpServerInstallation();
    TestTcpClientInstallation();
    TestFlowMonitorInstallation();
    TestTcpDataRecording();

    return 0;
}
