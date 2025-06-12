#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/matrix-propagation-loss-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/config.h"
#include <iostream>

using namespace ns3;

class Ns3UnitTest : public TestCase {
public:
    Ns3UnitTest() : TestCase("NS-3 Unit Tests") {}

    void DoRun() override {
        TestNodeCreation();
        TestMobilityAssignment();
        TestPropagationLossSetup();
        TestWifiInstallation();
        TestApplicationSetup();
    }

private:
    void TestNodeCreation() {
        NodeContainer nodes;
        nodes.Create(3);
        NS_TEST_ASSERT_MSG_EQ(nodes.GetN(), 3, "Node creation failed");
    }

    void TestMobilityAssignment() {
        NodeContainer nodes;
        nodes.Create(3);
        for (uint8_t i = 0; i < 3; ++i) {
            nodes.Get(i)->AggregateObject(CreateObject<ConstantPositionMobilityModel>());
        }
        NS_TEST_ASSERT_MSG_NE(nodes.Get(0)->GetObject<MobilityModel>(), nullptr, "Mobility model assignment failed");
    }

    void TestPropagationLossSetup() {
        Ptr<MatrixPropagationLossModel> lossModel = CreateObject<MatrixPropagationLossModel>();
        NS_TEST_ASSERT_MSG_NE(lossModel, nullptr, "Propagation loss model creation failed");
    }

    void TestWifiInstallation() {
        NodeContainer nodes;
        nodes.Create(3);
        YansWifiPhyHelper wifiPhy;
        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());
        WifiHelper wifi;
        WifiMacHelper wifiMac;
        wifiMac.SetType("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);
        NS_TEST_ASSERT_MSG_GT(devices.GetN(), 0, "WiFi installation failed");
    }

    void TestApplicationSetup() {
        NodeContainer nodes;
        nodes.Create(3);
        OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("10.0.0.2"), 12345));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(1400));
        ApplicationContainer cbrApps = onOffHelper.Install(nodes.Get(0));
        NS_TEST_ASSERT_MSG_GT(cbrApps.GetN(), 0, "Application setup failed");
    }
};

static Ns3UnitTest testSuite;
