#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpBulkTransferExample");

// Test 1: Node creation (Client and Server)
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Expected 2 nodes (client and server)");
}

// Test 2: Point-to-Point Link Setup
void TestPointToPointLink()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Verify if the devices were correctly installed
    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Expected 2 devices for the point-to-point link");
}

// Test 3: IP Address Assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify that IP addresses are correctly assigned
    NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(0), Ipv4Address("0.0.0.0"), "Failed to assign IP to client node");
    NS_TEST_ASSERT_MSG_NE(interfaces.GetAddress(1), Ipv4Address("0.0.0.0"), "Failed to assign IP to server node");
}

// Test 4: Application Installation (Client and Server)
void TestApplicationInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP bulk send application (client)
    uint16_t port = 9; 
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Set up packet sink application (server)
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Verify that the applications were correctly installed
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "Failed to install client application");
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "Failed to install server application");
}

// Test 5: Packet Reception by Server
void TestPacketReception()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP bulk send application (client)
    uint16_t port = 9; 
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Set up packet sink application (server)
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));

    // Verify that the server received some packets
    NS_TEST_ASSERT_MSG_GT(sink->GetTotalRx(), 0, "Server did not receive any packets");

    Simulator::Destroy();
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestPointToPointLink();
    TestIpAddressAssignment();
    TestApplicationInstallation();
    TestPacketReception();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
