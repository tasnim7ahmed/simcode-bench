#include "ns3/test.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/config.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

class WifiSimulationTest : public TestCase {
public:
    WifiSimulationTest() : TestCase("Test Wi-Fi Simulation") {}

    virtual void DoRun() {
        // Define test parameters
        uint8_t minMcs = 0, maxMcs = 11;
        uint8_t mcs = minMcs;
        uint8_t nStations = 2;
        uint16_t payloadSize = 1472;
        bool udp = true;
        double frequency = 5.0;
        double simulationTime = 1.0;
        uint16_t expectedThroughput = 100;
        
        NodeContainer wifiStaNodes;
        wifiStaNodes.Create(nStations);
        NodeContainer wifiApNode;
        wifiApNode.Create(1);

        WifiHelper wifi;
        wifi.SetStandard(WIFI_STANDARD_80211be);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue("EhtMcs" + std::to_string(mcs)),
                                    "ControlMode", StringValue("EhtMcs" + std::to_string(mcs)));

        YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phy.SetChannel(channel.Create());
        
        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-80211be");
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

        InternetStackHelper stack;
        stack.Install(wifiStaNodes);
        stack.Install(wifiApNode);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
        Ipv4InterfaceContainer apNodeInterface = address.Assign(apDevice);

        UdpServerHelper server(9);
        ApplicationContainer serverApp = server.Install(wifiApNode);
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime + 1.0));

        UdpClientHelper client(apNodeInterface.GetAddress(0), 9);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
        client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));
        ApplicationContainer clientApp = client.Install(wifiStaNodes);
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime + 1.0));

        Simulator::Stop(Seconds(simulationTime + 1.0));
        Simulator::Run();

        uint64_t rxBytes = server.GetServer()->GetReceived();
        double throughput = (rxBytes * 8) / simulationTime / 1e6;
        NS_TEST_ASSERT_MSG_GE(throughput, expectedThroughput * 0.9, "Throughput lower than expected");

        Simulator::Destroy();
    }
};

static WifiSimulationTest wifiSimulationTestInstance;
