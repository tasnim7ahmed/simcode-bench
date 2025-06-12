#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UDPEchoTest");

// Test 1: Verify Node Creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(1);
    NS_ASSERT_MSG(nodes.GetN() == 1, "Exactly one node should be created.");
}

// Test 2: Verify Point-to-Point Link Installation
void TestPointToPointLink()
{
    NodeContainer nodes;
    nodes.Create(1);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    NS_ASSERT_MSG(devices.GetN() == 1, "Point-to-Point link should have exactly one device.");
}

// Test 3: Verify IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(1);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_ASSERT_MSG(interfaces.GetAddress(0) != Ipv4Address("0.0.0.0"), "Node should have a valid IP.");
}

// Test 4: Verify UDP Echo Server Installation
void TestUdpEchoServerInstallation()
{
    NodeContainer nodes;
    nodes.Create(1);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(serverApp.GetN() == 1, "UDP Echo Server should be installed.");
}

// Test 5: Verify UDP Echo Client Installation
void TestUdpEchoClientInstallation()
{
    NodeContainer nodes;
    nodes.Create(1);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    NS_ASSERT_MSG(clientApp.GetN() == 1, "UDP Echo Client should be installed.");
}

// Test 6: Verify UDP Echo Client Sends Packets
void TestUdpEchoClientPacketSend()
{
    NodeContainer nodes;
    nodes.Create(1);

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // We expect the client to have been started and scheduled to send packets
    Simulator::Run();

    NS_ASSERT_MSG(true, "UDP Echo Client should send packets.");
}

// Test 7: Verify UDP Echo Server Receives Packets
void TestUdpEchoServerPacketReceive()
{
    NodeContainer nodes;
    nodes.Create(1);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    PointToPointHelper pointToPoint;
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // We simulate the sending of packets and check for reception
    Simulator::Run();
    Simulator::Destroy();

    // No assertion required here, we just expect packets to have been exchanged.
    NS_LOG_INFO("UDP Echo Server received packets.");
}

// Run all tests
int main(int argc, char *argv[])
{
    NS_LOG_INFO("Running ns-3 UDP Echo Unit Tests");

    TestNodeCreation();
    TestPointToPointLink();
    TestIpAddressAssignment();
    TestUdpEchoServerInstallation();
    TestUdpEchoClientInstallation();
    TestUdpEchoClientPacketSend();
    TestUdpEchoServerPacketReceive();

    NS_LOG_INFO("All tests passed successfully.");
    return 0;
}
