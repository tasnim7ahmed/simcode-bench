#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class TcpBulkSendTest : public TestCase
{
public:
    TcpBulkSendTest() : TestCase("TCP Bulk Send Functional Test") {}

private:
    virtual void DoRun()
    {
        // Create Nodes
        NodeContainer nodes;
        nodes.Create(2);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Incorrect number of nodes created");

        // Create Point-to-Point Channel
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Incorrect number of NetDevices installed");

        // Install Internet Stack
        InternetStackHelper internet;
        internet.Install(nodes);

        // Assign IP Addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(devices);
        NS_TEST_ASSERT_MSG_NE(i.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP assigned");

        // Set up Bulk Send Application
        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0));

        ApplicationContainer sourceApps = source.Install(nodes.Get(0));
        sourceApps.Start(Seconds(0.0));
        sourceApps.Stop(Seconds(10.0));

        // Set up Packet Sink Application
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        // Run Simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        // Verify Packet Reception
        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
        NS_TEST_ASSERT_MSG_GT(sink1->GetTotalRx(), 0, "No packets received");

        Simulator::Destroy();
    }
};

// Register the test
static TcpBulkSendTest tcpBulkSendTest;
