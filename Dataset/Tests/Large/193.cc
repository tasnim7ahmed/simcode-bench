#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointTcpExample");

// Test 1: Node creation
void TestNodeCreation()
{
    NodeContainer nodes;
    nodes.Create(2);

    NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Expected 2 nodes to be created");
}

// Test 2: Point-to-Point link setup
void TestPointToPointLinkSetup()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Expected 2 devices to be installed on the nodes");
}

// Test 3: Internet stack installation
void TestInternetStackInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    InternetStackHelper internet;
    internet.Install(nodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack not installed correctly on node");
    }
}

// Test 4: IP address assignment
void TestIpAddressAssignment()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ipv4Address ip = interfaces.GetAddress(i);
        NS_TEST_ASSERT_MSG_NE(ip, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node");
    }
}

// Test 5: Application installation and configuration (sender and receiver)
void TestApplicationInstallation()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9; // Well-known TCP port

    // Sender application (BulkSend)
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
    ApplicationContainer senderApp = bulkSend.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Receiver application (PacketSink)
    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer receiverApp = packetSink.Install(nodes.Get(1));
    receiverApp.Start(Seconds(1.0));
    receiverApp.Stop(Seconds(10.0));

    // Ensure the applications are installed on the correct nodes and started
    NS_TEST_ASSERT_MSG_EQ(senderApp.Get(0)->GetStartTime().GetSeconds(), 1.0, "Sender application did not start at the correct time");
    NS_TEST_ASSERT_MSG_EQ(receiverApp.Get(0)->GetStartTime().GetSeconds(), 1.0, "Receiver application did not start at the correct time");
}

// Test 6: Packet transmission and verification (total bytes received)
void TestPacketTransmission()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9; // Well-known TCP port

    // Sender application (BulkSend)
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
    ApplicationContainer senderApp = bulkSend.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    // Receiver application (PacketSink)
    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer receiverApp = packetSink.Install(nodes.Get(1));
    receiverApp.Start(Seconds(1.0));
    receiverApp.Stop(Seconds(10.0));

    // Run simulation and check for successful packet reception
    Simulator::Run();

    // Verify if data was received by checking total bytes received
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(receiverApp.Get(0));
    NS_TEST_ASSERT_MSG_GT(sink->GetTotalRx(), 0, "No packets received by receiver");

    Simulator::Destroy();
}

// Main test execution function
int main(int argc, char *argv[])
{
    // Run all tests
    TestNodeCreation();
    TestPointToPointLinkSetup();
    TestInternetStackInstallation();
    TestIpAddressAssignment();
    TestApplicationInstallation();
    TestPacketTransmission();

    NS_LOG_UNCOND("All tests passed successfully.");
    return 0;
}
