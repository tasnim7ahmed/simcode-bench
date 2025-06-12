#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/config.h"
#include "ns3/simulator.h"
#include "ns3/test.h"

using namespace ns3;

class Ns3UnitTest : public TestCase {
public:
    Ns3UnitTest() : TestCase("NS-3 Functional Tests") {}

    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(3);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node count mismatch");
    }

    void TestMobilityAssignment() {
        NodeContainer nodes;
        nodes.Create(1);
        MobilityHelper mobility;
        mobility.Install(nodes);
        
        Ptr<MobilityModel> mobilityModel = nodes.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not assigned");
    }

    void TestWifiInstallation() {
        NodeContainer nodes;
        nodes.Create(2);
        
        WifiHelper wifi;
        YansWifiPhyHelper wifiPhy;
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        WifiMacHelper wifiMac;
        Ssid ssid = Ssid("test-ssid");

        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

        NS_TEST_ASSERT_MSG_EQ(devices.GetN(), 2, "Wifi devices not properly installed");
    }

    void TestPacketTransmission() {
        NodeContainer nodes;
        nodes.Create(2);
        
        PacketSocketHelper packetSocket;
        packetSocket.Install(nodes);

        PacketSocketAddress socket;
        socket.SetSingleDevice(0);
        socket.SetProtocol(1);
        
        OnOffHelper onoff("ns3::PacketSocketFactory", Address(socket));
        onoff.SetConstantRate(DataRate("500kb/s"));
        ApplicationContainer apps = onoff.Install(nodes.Get(0));

        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(2.0));

        Simulator::Stop(Seconds(3.0));
        Simulator::Run();
        Simulator::Destroy();
    }

    virtual void DoRun() {
        TestNodeCreation();
        TestMobilityAssignment();
        TestWifiInstallation();
        TestPacketTransmission();
    }
};

static Ns3UnitTest ns3UnitTestInstance;
