#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class PointToPointTestSuite : public TestCase
{
public:
    PointToPointTestSuite() : TestCase("Test Point-to-Point Network with TCP BulkSend and PacketSink Applications") {}
    virtual ~PointToPointTestSuite() {}

    void DoRun() override
    {
        TestNodeCreation();
        TestPointToPointConnection();
        TestDeviceInstallation();
        TestInternetStackInstallation();
        TestIpAddressAssignment();
        TestTcpBulkSendSetup();
        TestPacketSinkSetup();
        TestDataTransmission();
    }

private:
    // Test node creation (verify that 2 nodes are created correctly)
    void TestNodeCreation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        // Verify that 2 nodes are created
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 2, "Failed to create the expected number of nodes.");
    }

    // Test Point-to-Point connection (verify the setup of data rate and delay)
    void TestPointToPointConnection()
    {
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        // Check if the configured data rate and delay are set correctly
        NS_TEST_ASSERT_MSG_EQ(pointToPoint.GetDeviceAttribute("DataRate").GetValue(), StringValue("10Mbps").GetValue(), "Failed to set the correct data rate.");
        NS_TEST_ASSERT_MSG_EQ(pointToPoint.GetChannelAttribute("Delay").GetValue(), StringValue("2ms").GetValue(), "Failed to set the correct delay.");
    }

    // Test device installation (verify that devices are installed on both nodes)
    void TestDeviceInstallation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        // Verify that network devices are installed on both nodes
        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Failed to install network devices on both nodes.");
    }

    // Test Internet stack installation (verify the Internet stack is installed)
    void TestInternetStackInstallation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        InternetStackHelper stack;
        stack.Install(nodes);

        // Verify that the Internet stack is installed on both nodes
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ptr<Node> node = nodes.Get(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            NS_TEST_ASSERT_MSG_NE(ipv4, nullptr, "Internet stack was not installed on node " + std::to_string(i));
        }
    }

    // Test IP address assignment (verify that IP addresses are assigned correctly)
    void TestIpAddressAssignment()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        // Verify that IP addresses are assigned to devices
        for (uint32_t i = 0; i < nodes.GetN(); ++i)
        {
            Ipv4Address ipAddr = interfaces.GetAddress(i);
            NS_TEST_ASSERT_MSG_NE(ipAddr, Ipv4Address("0.0.0.0"), "Failed to assign IP address to node " + std::to_string(i));
        }
    }

    // Test TCP BulkSend application setup (verify that the source application is set up)
    void TestTcpBulkSendSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
        ApplicationContainer sourceApp = source.Install(nodes.Get(0));

        // Verify that the TCP BulkSend application is installed on Node 0
        sourceApp.Start(Seconds(1.0));
        sourceApp.Stop(Seconds(10.0));
        NS_TEST_ASSERT_MSG_EQ(sourceApp.GetN(), 1, "Failed to install BulkSend application on Node 0.");
    }

    // Test PacketSink application setup (verify that the sink application is set up)
    void TestPacketSinkSetup()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 9;
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(1));

        // Verify that the PacketSink application is installed on Node 1
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));
        NS_TEST_ASSERT_MSG_EQ(sinkApp.GetN(), 1, "Failed to install PacketSink application on Node 1.");
    }

    // Test data transmission (verify that data is transferred from source to sink)
    void TestDataTransmission()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);

        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data transfer
        ApplicationContainer sourceApp = source.Install(nodes.Get(0));

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(1));

        sourceApp.Start(Seconds(1.0));
        sourceApp.Stop(Seconds(10.0));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));

        Simulator::Run();

        // Verify that data is received at the PacketSink
        uint32_t totalRx = DynamicCast<PacketSink>(sinkApp.Get(0))->GetTotalRx();
        NS_TEST_ASSERT_MSG_GT(totalRx, 0, "No data received at the PacketSink.");
        Simulator::Destroy();
    }
};

// Test Suite registration
int main(int argc, char *argv[])
{
    PointToPointTestSuite pointToPointTestSuite;
    pointToPointTestSuite.Run();
    return 0;
}
