#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointAndLanExample");

// Test 1: Node Creation
void TestNodeCreation()
{
    NodeContainer routers;
    routers.Create(3);

    NodeContainer lanNodes;
    lanNodes.Add(routers.Get(1)); // Connect R2 to the LAN
    lanNodes.Create(2);           // Add 2 LAN nodes

    // Verify that 3 routers and 2 LAN nodes were created
    NS_TEST_ASSERT_MSG_EQ(routers.GetN(), 3, "Expected 3 router nodes");
    NS_TEST_ASSERT_MSG_EQ(lanNodes.GetN(), 3, "Expected 3 LAN nodes including router R2");
}

// Test 2: Point-to-Point Link Installation
void TestPointToPointLinkInstallation()
{
    NodeContainer routers;
    routers.Create(3);

    // Create point-to-point links between routers
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesR1R2, devicesR2R3;
    devicesR1R2 = pointToPoint.Install(routers.Get(0), routers.Get(1)); // R1-R2 link
    devicesR2R3 = pointToPoint.Install(routers.Get(1), routers.Get(2)); // R2-R3 link

    // Verify that the point-to-point devices were installed
    NS_TEST_ASSERT_MSG_EQ(devicesR1R2.GetN(), 2, "Failed to install point-to-point devices between R1 and R2");
    NS_TEST_ASSERT_MSG_EQ(devicesR2R3.GetN(), 2, "Failed to install point-to-point devices between R2 and R3");
}

// Test 3: LAN (CSMA) Link Installation
void TestLanLinkInstallation()
{
    NodeContainer routers;
    routers.Create(3);

    NodeContainer lanNodes;
    lanNodes.Add(routers.Get(1)); // Connect R2 to the LAN
    lanNodes.Create(2);           // Add 2 LAN nodes

    // Create CSMA (LAN) link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer lanDevices;
    lanDevices = csma.Install(lanNodes);

    // Verify that the CSMA devices were installed
    NS_TEST_ASSERT_MSG_EQ(lanDevices.GetN(), 3, "Failed to install CSMA devices on LAN nodes");
}

// Test 4: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer routers;
    routers.Create(3);

    NodeContainer lanNodes;
    lanNodes.Add(routers.Get(1)); // Connect R2 to the LAN
    lanNodes.Create(2);           // Add 2 LAN nodes

    // Create point-to-point links between routers
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesR1R2, devicesR2R3;
    devicesR1R2 = pointToPoint.Install(routers.Get(0), routers.Get(1)); // R1-R2 link
    devicesR2R3 = pointToPoint.Install(routers.Get(1), routers.Get(2)); // R2-R3 link

    // Create CSMA (LAN) link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer lanDevices;
    lanDevices = csma.Install(lanNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(lanNodes.Get(1)); // Install on LAN nodes

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR1R2 = address.Assign(devicesR1R2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR2R3 = address.Assign(devicesR2R3);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);

    // Verify IP addresses have been assigned
    NS_TEST_ASSERT_MSG_NE(interfacesR1R2.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to R1-R2 link");
    NS_TEST_ASSERT_MSG_NE(interfacesR2R3.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to R2-R3 link");
    NS_TEST_ASSERT_MSG_NE(lanInterfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to LAN node");
}

// Test 5: UDP Server Installation
void TestUdpServerInstallation()
{
    NodeContainer routers;
    routers.Create(3);

    // Set up UDP server on R3 (node 2)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(routers.Get(2)); // R3
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Verify that the UDP server was installed correctly
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install UDP server on R3");
}

// Test 6: UDP Client Installation
void TestUdpClientInstallation()
{
    NodeContainer routers;
    routers.Create(3);

    NodeContainer lanNodes;
    lanNodes.Add(routers.Get(1)); // Connect R2 to the LAN
    lanNodes.Create(2);           // Add 2 LAN nodes

    // Assign IP addresses and install UDP server
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesR1R2, devicesR2R3;
    devicesR1R2 = pointToPoint.Install(routers.Get(0), routers.Get(1)); // R1-R2 link
    devicesR2R3 = pointToPoint.Install(routers.Get(1), routers.Get(2)); // R2-R3 link

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer lanDevices;
    lanDevices = csma.Install(lanNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(lanNodes.Get(1)); // Install on LAN nodes

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR1R2 = address.Assign(devicesR1R2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR2R3 = address.Assign(devicesR2R3);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);

    uint16_t port = 4000;
    UdpClientHelper udpClient(interfacesR2R3.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(lanNodes.Get(1)); // LAN Node
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Verify that the UDP client was installed correctly
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install UDP client on LAN node");
}

// Test 7: Flow Monitor and Statistics
void TestFlowMonitorStatistics()
{
    NodeContainer routers;
    routers.Create(3);

    NodeContainer lanNodes;
    lanNodes.Add(routers.Get(1)); // Connect R2 to the LAN
    lanNodes.Create(2);           // Add 2 LAN nodes

    // Create point-to-point links between routers
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesR1R2, devicesR2R3;
    devicesR1R2 = pointToPoint.Install(routers.Get(0), routers.Get(1)); // R1-R2 link
    devicesR2R3 = pointToPoint.Install(routers.Get(1), routers.Get(2)); // R2-R3 link

    // Create CSMA (LAN) link
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer lanDevices;
    lanDevices = csma.Install(lanNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(routers);
    stack.Install(lanNodes.Get(1)); // Install on LAN nodes

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR1R2 = address.Assign(devicesR1R2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesR2R3 = address.Assign(devicesR2R3);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices);

    // Set up FlowMonitor to track traffic
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Check for lost packets
    monitor->CheckForLostPackets();

    // Verify FlowMonitor statistics
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    NS_TEST_ASSERT_MSG_EQ(stats.size(), 1, "FlowMonitor did not track the expected number of flows");

    // Clean up and exit
    Simulator::Destroy();
}

int main(int argc, char *argv[])
{
    // Call each test function
    TestNodeCreation();
    TestPointToPointLinkInstallation();
    TestLanLinkInstallation();
    TestIpAddressAssignment();
    TestUdpServerInstallation();
    TestUdpClientInstallation();
    TestFlowMonitorStatistics();

    return 0;
}
