#include "ns3/test.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

class MixedWirelessTest : public TestCase
{
public:
    MixedWirelessTest() : TestCase("Test Mixed Wireless Simulation") {}
    virtual void DoRun()
    {
        TestBackboneNodesCreation();
        TestLanNodesCreation();
        TestWifiInfraNodesCreation();
        TestMobilityModelAssignment();
        TestApplicationLayerSetup();
    }

private:
    void TestBackboneNodesCreation()
    {
        uint32_t backboneNodes = 10;
        NodeContainer backbone;
        backbone.Create(backboneNodes);
        NS_TEST_ASSERT_MSG_EQ(backbone.GetN(), backboneNodes, "Backbone nodes creation failed.");
    }

    void TestLanNodesCreation()
    {
        uint32_t lanNodes = 2;
        NodeContainer newLanNodes;
        newLanNodes.Create(lanNodes - 1);
        NS_TEST_ASSERT_MSG_EQ(newLanNodes.GetN(), lanNodes - 1, "LAN nodes creation failed.");
    }

    void TestWifiInfraNodesCreation()
    {
        uint32_t infraNodes = 2;
        NodeContainer stas;
        stas.Create(infraNodes - 1);
        NS_TEST_ASSERT_MSG_EQ(stas.GetN(), infraNodes - 1, "Infrastructure nodes creation failed.");
    }

    void TestMobilityModelAssignment()
    {
        NodeContainer backbone;
        backbone.Create(10);
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(backbone);
        Ptr<MobilityModel> mobilityModel = backbone.Get(0)->GetObject<MobilityModel>();
        NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model assignment failed.");
    }

    void TestApplicationLayerSetup()
    {
        NodeContainer sender, receiver;
        sender.Create(1);
        receiver.Create(1);
        InternetStackHelper internet;
        internet.Install(sender);
        internet.Install(receiver);

        OnOffHelper onoff("ns3::UdpSocketFactory", Address());
        onoff.SetAttribute("DataRate", StringValue("100kb/s"));
        ApplicationContainer app = onoff.Install(sender.Get(0));

        NS_TEST_ASSERT_MSG_EQ(app.GetN(), 1, "Application layer setup failed.");
    }
};

// Register the test case
static MixedWirelessTest g_mixedWirelessTest;
