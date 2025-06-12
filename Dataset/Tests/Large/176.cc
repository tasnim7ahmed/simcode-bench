#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"
#include "ns3/test.h" // Include the test header for unit tests

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketLossTest");

class PacketLossTest : public ns3::TestCase
{
public:
    PacketLossTest() : TestCase("Packet Loss Example Test") {}
    virtual ~PacketLossTest() {}

    virtual void DoRun() {
        // Setup code from the original program
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        NetDeviceContainer devices;
        devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::RedQueueDisc"); // RED queue for packet loss
        tch.Install(devices);

        uint16_t port = 9;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(1));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(5.0));

        UdpClientHelper client(interfaces.GetAddress(1), port);
        client.SetAttribute("MaxPackets", UintegerValue(100));
        client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(nodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(5.0));

        AnimationInterface anim("packet_loss_netanim.xml");
        anim.SetConstantPosition(nodes.Get(0), 0, 0);
        anim.SetConstantPosition(nodes.Get(1), 5, 0);
        anim.EnablePacketMetadata(true);

        Simulator::Stop(Seconds(5.0));
        Simulator::Run();
        Simulator::Destroy();
    }
};

// Test point-to-point link configuration
void TestPointToPointLink() {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Test the link configuration
    Ptr<PointToPointNetDevice> device1 = DynamicCast<PointToPointNetDevice>(devices.Get(0));
    Ptr<PointToPointNetDevice> device2 = DynamicCast<PointToPointNetDevice>(devices.Get(1));

    NS_TEST_ASSERT_MSG_EQ(device1->GetDataRate().GetBitRate(), 2e6, "Incorrect data rate on Node 0");
    NS_TEST_ASSERT_MSG_EQ(device2->GetDataRate().GetBitRate(), 2e6, "Incorrect data rate on Node 1");
    NS_TEST_ASSERT_MSG_EQ(device1->GetChannel()->GetDelay().GetMilliSeconds(), 5, "Incorrect delay on Node 0");
    NS_TEST_ASSERT_MSG_EQ(device2->GetChannel()->GetDelay().GetMilliSeconds(), 5, "Incorrect delay on Node 1");
}

// Test IP address assignment
void TestIpAddressAssignment() {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Verify the assigned IP addresses
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(0), Ipv4Address("10.1.1.1"), "Incorrect IP address for Node 0");
    NS_TEST_ASSERT_MSG_EQ(interfaces.GetAddress(1), Ipv4Address("10.1.1.2"), "Incorrect IP address for Node 1");
}

// Test UDP server and client functionality
void TestUdpServerAndClient() {
    NodeContainer nodes;
    nodes.Create(2);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(5.0));

    UdpClientHelper client(Ipv4Address("10.1.1.2"), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(5.0));

    // Check if server and client applications are installed
    NS_TEST_ASSERT_MSG_EQ(serverApp.GetN(), 1, "UDP server not installed correctly!");
    NS_TEST_ASSERT_MSG_EQ(clientApp.GetN(), 1, "UDP client not installed correctly!");
}

// Test Traffic Control (RED Queue Disc)
void TestTrafficControl() {
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("2Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc"); // RED queue for packet loss
    tch.Install(devices);

    // Verify if the queue discipline is correctly installed
    Ptr<PointToPointNetDevice> device1 = DynamicCast<PointToPointNetDevice>(devices.Get(0));
    Ptr<PointToPointNetDevice> device2 = DynamicCast<PointToPointNetDevice>(devices.Get(1));
    
    Ptr<RedQueueDisc> redQueue1 = DynamicCast<RedQueueDisc>(device1->GetQueue());
    Ptr<RedQueueDisc> redQueue2 = DynamicCast<RedQueueDisc>(device2->GetQueue());

    NS_TEST_ASSERT_MSG_NE(redQueue1, nullptr, "RED queue not installed on Node 0");
    NS_TEST_ASSERT_MSG_NE(redQueue2, nullptr, "RED queue not installed on Node 1");
}

// Main function to run all tests
int main(int argc, char *argv[]) {
    TestPointToPointLink();
    TestIpAddressAssignment();
    TestUdpServerAndClient();
    TestTrafficControl();

    return 0;
}
