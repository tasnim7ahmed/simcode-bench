#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/test.h"

using namespace ns3;

class WifiSleepTestSuite : public TestSuite
{
public:
    WifiSleepTestSuite() : TestSuite("wifi-sleep", UNIT)
    {
        AddTestCase(new TestEnergyDepletion, TestCase::QUICK);
        AddTestCase(new TestPacketTransmission, TestCase::QUICK);
        AddTestCase(new TestMobilitySetup, TestCase::QUICK);
    }
};

static WifiSleepTestSuite wifiSleepTestSuite;

class TestEnergyDepletion : public TestCase
{
public:
    TestEnergyDepletion() : TestCase("Test if energy depletes over time") {}

    virtual void DoRun()
    {
        NodeContainer c;
        c.Create(2);

        BasicEnergySourceHelper energySourceHelper;
        energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(5.0));
        EnergySourceContainer sources = energySourceHelper.Install(c);

        Simulator::Stop(Seconds(10.0));
        Simulator::Run();

        Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(sources.Get(0));
        NS_TEST_ASSERT_MSG_LT(source->GetRemainingEnergy(), 5.0, "Energy should decrease over time");

        Simulator::Destroy();
    }
};

class TestPacketTransmission : public TestCase
{
public:
    TestPacketTransmission() : TestCase("Test if packets are transmitted") {}

    virtual void DoRun()
    {
        NodeContainer c;
        c.Create(2);

        WifiHelper wifi;
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");

        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);
        InternetStackHelper internet;
        internet.Install(c);

        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApps = echoServer.Install(c.Get(1));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(5.0));

        UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        
        ApplicationContainer clientApps = echoClient.Install(c.Get(0));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(4.0));

        Simulator::Run();
        Simulator::Destroy();

        NS_TEST_ASSERT_MSG_EQ(echoServer.GetServer()->GetReceived(), 1, "Packet should be received");
    }
};

class TestMobilitySetup : public TestCase
{
public:
    TestMobilitySetup() : TestCase("Test if nodes have mobility set up") {}

    virtual void DoRun()
    {
        NodeContainer c;
        c.Create(2);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(10.0, 0.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(c);

        Simulator::Run();
        Simulator::Destroy();

        Ptr<MobilityModel> model = c.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_EQ(model->GetPosition().x, 0.0, "Position should be correctly assigned");
    }
};
