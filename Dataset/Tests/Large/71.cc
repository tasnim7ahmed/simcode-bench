#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/csma-net-device.h"

using namespace ns3;

class GlobalRouterTest : public TestCase
{
public:
    GlobalRouterTest() : TestCase("Global Routing Slash32 Functional Test") {}

private:
    virtual void DoRun()
    {
        // Create Nodes
        Ptr<Node> nA = CreateObject<Node>();
        Ptr<Node> nB = CreateObject<Node>();
        Ptr<Node> nC = CreateObject<Node>();
        NodeContainer c = NodeContainer(nA, nB, nC);
        NS_TEST_ASSERT_MSG_EQ(c.GetN(), 3, "Incorrect number of nodes created");

        // Install Internet Stack
        InternetStackHelper internet;
        internet.Install(c);

        // Point-to-point links
        NodeContainer nAnB = NodeContainer(nA, nB);
        NodeContainer nBnC = NodeContainer(nB, nC);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        NetDeviceContainer dAdB = p2p.Install(nAnB);
        NetDeviceContainer dBdC = p2p.Install(nBnC);

        NS_TEST_ASSERT_MSG_EQ(dAdB.GetN(), 2, "Incorrect number of NetDevices installed on nA-nB");
        NS_TEST_ASSERT_MSG_EQ(dBdC.GetN(), 2, "Incorrect number of NetDevices installed on nB-nC");

        // Add CSMA Devices
        Ptr<CsmaNetDevice> deviceA = CreateObject<CsmaNetDevice>();
        deviceA->SetAddress(Mac48Address::Allocate());
        nA->AddDevice(deviceA);

        Ptr<CsmaNetDevice> deviceC = CreateObject<CsmaNetDevice>();
        deviceC->SetAddress(Mac48Address::Allocate());
        nC->AddDevice(deviceC);

        NS_TEST_ASSERT_MSG_NE(deviceA->GetAddress(), Mac48Address("00:00:00:00:00:00"), "Invalid MAC address for device A");
        NS_TEST_ASSERT_MSG_NE(deviceC->GetAddress(), Mac48Address("00:00:00:00:00:00"), "Invalid MAC address for device C");

        // Assign IP addresses
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.252");
        Ipv4InterfaceContainer iAiB = ipv4.Assign(dAdB);
        ipv4.SetBase("10.1.1.4", "255.255.255.252");
        Ipv4InterfaceContainer iBiC = ipv4.Assign(dBdC);

        NS_TEST_ASSERT_MSG_NE(iAiB.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to nA");
        NS_TEST_ASSERT_MSG_NE(iBiC.GetAddress(0), Ipv4Address("0.0.0.0"), "Invalid IP address assigned to nB");

        // Assign Slash-32 IP Addresses
        Ptr<Ipv4> ipv4A = nA->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4C = nC->GetObject<Ipv4>();

        int32_t ifIndexA = ipv4A->AddInterface(deviceA);
        int32_t ifIndexC = ipv4C->AddInterface(deviceC);

        Ipv4InterfaceAddress ifInAddrA =
            Ipv4InterfaceAddress(Ipv4Address("172.16.1.1"), Ipv4Mask("255.255.255.255"));
        ipv4A->AddAddress(ifIndexA, ifInAddrA);
        ipv4A->SetUp(ifIndexA);

        Ipv4InterfaceAddress ifInAddrC =
            Ipv4InterfaceAddress(Ipv4Address("192.168.1.1"), Ipv4Mask("255.255.255.255"));
        ipv4C->AddAddress(ifIndexC, ifInAddrC);
        ipv4C->SetUp(ifIndexC);

        NS_TEST_ASSERT_MSG_NE(ipv4A->GetAddress(ifIndexA, 0).GetLocal(), Ipv4Address("0.0.0.0"), "Invalid Slash-32 IP address assigned to nA");
        NS_TEST_ASSERT_MSG_NE(ipv4C->GetAddress(ifIndexC, 0).GetLocal(), Ipv4Address("0.0.0.0"), "Invalid Slash-32 IP address assigned to nC");

        // Populate Routing Tables
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        // Install OnOff Application
        uint16_t port = 9;
        OnOffHelper onoff("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(ifInAddrC.GetLocal(), port)));
        onoff.SetConstantRate(DataRate(6000));

        ApplicationContainer sender = onoff.Install(nA);
        sender.Start(Seconds(1.0));
        sender.Stop(Seconds(10.0));

        // Install Packet Sink
        PacketSinkHelper sink("ns3::UdpSocketFactory",
                              Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
        ApplicationContainer receiver = sink.Install(nC);
        receiver.Start(Seconds(1.0));
        receiver.Stop(Seconds(10.0));

        // Run Simulation
        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        // Verify Packet Reception
        Ptr<PacketSink> sinkApp = DynamicCast<PacketSink>(receiver.Get(0));
        NS_TEST_ASSERT_MSG_GT(sinkApp->GetTotalRx(), 0, "No packets received at node C");

        Simulator::Destroy();
    }
};

// Register the test
static GlobalRouterTest globalRouterTest;
