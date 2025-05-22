#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class TcpPcapNanosecExampleTest : public TestCase
{
public:
    TcpPcapNanosecExampleTest() : TestCase("Test TcpPcapNanosecExample") {}

    void DoRun() override
    {
        TestBasicSimulation();
        TestPacketSent();
        TestReceivedBytes();
        TestTracing();
    }

private:
    void TestBasicSimulation()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(devices);

        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
        ApplicationContainer sourceApps = source.Install(nodes.Get(0));
        sourceApps.Start(Seconds(0.0));
        sourceApps.Stop(Seconds(10.0));

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        Simulator::Run();

        // Ensure the simulation ran
        NS_ASSERT_TRUE(Simulator::GetContext() == 0);
    }

    void TestPacketSent()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(devices);

        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(1000));
        ApplicationContainer sourceApps = source.Install(nodes.Get(0));
        sourceApps.Start(Seconds(0.0));
        sourceApps.Stop(Seconds(10.0));

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        Simulator::Run();

        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
        NS_ASSERT_EQUAL(sink1->GetTotalRx(), 1000);
    }

    void TestReceivedBytes()
    {
        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(devices);

        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(1000));
        ApplicationContainer sourceApps = source.Install(nodes.Get(0));
        sourceApps.Start(Seconds(0.0));
        sourceApps.Stop(Seconds(10.0));

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        Simulator::Run();

        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
        uint32_t receivedBytes = sink1->GetTotalRx();

        NS_ASSERT_EQUAL(receivedBytes, 1000);
    }

    void TestTracing()
    {
        bool tracingEnabled = true;

        NodeContainer nodes;
        nodes.Create(2);

        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
        pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

        NetDeviceContainer devices = pointToPoint.Install(nodes);

        InternetStackHelper internet;
        internet.Install(nodes);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign(devices);

        uint16_t port = 9;
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
        ApplicationContainer sourceApps = source.Install(nodes.Get(0));
        sourceApps.Start(Seconds(0.0));
        sourceApps.Stop(Seconds(10.0));

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(10.0));

        if (tracingEnabled)
        {
            AsciiTraceHelper ascii;
            pointToPoint.EnablePcapAll("tcp-pcap-nanosec-example", false);
        }

        Simulator::Run();

        // Check if tracing file is created
        std::ifstream pcapFile("tcp-pcap-nanosec-example-0-0.pcap");
        NS_ASSERT_TRUE(pcapFile.is_open());
    }
};

// Register the test
TestSuite* CreateTcpPcapNanosecExampleTestSuite()
{
    TestSuite* suite = new TestSuite("TcpPcapNanosecExample", SYSTEM);
    suite->AddTestCase(new TcpPcapNanosecExampleTest());
    return suite;
}

