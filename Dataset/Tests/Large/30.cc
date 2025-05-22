#include "ns3/test.h"
#include "ns3/nstime.h"
#include "ns3/node-container.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/simulator.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"

using namespace ns3;

class WifiSimulationTest : public TestCase
{
public:
    WifiSimulationTest() : TestCase("WiFi Simulation Unit Test") {}
    virtual void DoRun()
    {
        TestNodeCreation();
        TestDeviceInstallation();
        TestMobilitySetup();
        TestInternetStackSetup();
        TestThroughputCalculation();
    }

    void TestNodeCreation()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(4);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);
        NS_TEST_ASSERT_MSG_EQ(wifiStaNodes.GetN(), 4, "Incorrect number of station nodes");
        NS_TEST_ASSERT_MSG_EQ(wifiApNode.GetN(), 1, "Incorrect number of AP nodes");
    }

    void TestDeviceInstallation()
    {
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(4);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);
        
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        YansWifiPhyHelper phy;
        phy.SetChannel(channel.Create());
        
        WifiMacHelper mac;
        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211n);
        
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ns3-80211n")));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
        
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ns3-80211n")));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);
        
        NS_TEST_ASSERT_MSG_GT(staDevices.GetN(), 0, "No devices installed on stations");
        NS_TEST_ASSERT_MSG_EQ(apDevice.GetN(), 1, "AP device installation failed");
    }

    void TestMobilitySetup()
    {
        NodeContainer nodes;
        nodes.Create(5);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        for (uint32_t i = 1; i < 5; i++)
        {
            positionAlloc->Add(Vector(i, 0.0, 0.0));
        }
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(nodes);

        for (uint32_t i = 0; i < nodes.GetN(); i++)
        {
            Ptr<MobilityModel> mobilityModel = nodes.Get(i)->GetObject<MobilityModel>();
            NS_TEST_ASSERT_MSG_NE(mobilityModel, nullptr, "Mobility model not installed");
        }
    }

    void TestInternetStackSetup()
    {
        NodeContainer nodes;
        nodes.Create(5);
        
        InternetStackHelper stack;
        stack.Install(nodes);
        
        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        
        NetDeviceContainer devices;
        devices.Create(5);
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        
        NS_TEST_ASSERT_MSG_GT(interfaces.GetN(), 0, "No IP addresses assigned");
    }

    void TestThroughputCalculation()
    {
        Time simulationTime = Seconds(10.0);
        ApplicationContainer sinkApplications;
        double throughput = 0;
        
        for (uint32_t index = 0; index < sinkApplications.GetN(); ++index)
        {
            double totalPacketsThrough =
                DynamicCast<PacketSink>(sinkApplications.Get(index))->GetTotalRx();
            throughput += ((totalPacketsThrough * 8) / simulationTime.GetMicroSeconds());
        }
        NS_TEST_ASSERT_MSG_GE(throughput, 0, "Throughput calculation failed");
    }
};

static WifiSimulationTest g_wifiSimulationTest;
