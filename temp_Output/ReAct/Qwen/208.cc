#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessHandoverSimulation");

class HandoverStats {
public:
    HandoverStats() : m_handovers(0), m_packetLoss(0) {}

    void CountHandover() { m_handovers++; }

    void CountPacketLoss(uint32_t num) { m_packetLoss += num; }

    uint32_t GetHandovers() const { return m_handovers; }

    uint32_t GetPacketLoss() const { return m_packetLoss; }

private:
    uint32_t m_handovers;
    uint32_t m_packetLoss;
};

static void PhyDropTrace(Ptr<HandoverStats> stats, Ptr<const Packet> p) {
    stats->CountPacketLoss(1);
}

static void AssocChangeCallback(std::string context, Mac48Address address, Ssid ssid) {
    NS_LOG_INFO(context << " Node " << address << " associated with SSID " << ssid);
}

int main(int argc, char *argv[]) {
    double simDuration = 60.0; // seconds
    uint32_t numMobileNodes = 6;
    bool enablePcap = false;
    bool verbose = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simDuration", "Total simulation time (seconds)", simDuration);
    cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
    }

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(999999));

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer mobileNodes;
    mobileNodes.Create(numMobileNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHz);

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("AP-Network-1");
    Ssid ssid2 = Ssid("AP-Network-2");

    NetDeviceContainer apDevices1, apDevices2;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2)));
    apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2)));
    apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    NetDeviceContainer mobileDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    mobileDevices = wifi.Install(phy, mac, mobileNodes);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    mobileDevices.Add(wifi.Install(phy, mac, mobileNodes));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(mobileNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer mobileInterfaces = address.Assign(mobileDevices);

    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(mobileNodes);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simDuration));

    for (uint32_t i = 0; i < mobileNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(mobileInterfaces.GetAddress(i), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(apNodes);
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simDuration));
    }

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("handover-trace.tr");
    phy.EnableAsciiAll(stream);

    if (enablePcap) {
        phy.EnablePcapAll("wifi-handover");
    }

    Config::Connect("/NodeList/*/DeviceList/*/Mac/MacAddr",
                    MakeCallback(&AssocChangeCallback));

    Ptr<HandoverStats> handoverStats = CreateObject<HandoverStats>();
    Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/Phy/PhyRxDrop",
                                  MakeBoundCallback(&PhyDropTrace, handoverStats));

    AnimationInterface anim("wireless-handover.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    std::cout << "Total handovers: " << handoverStats->GetHandovers() << std::endl;
    std::cout << "Estimated packet loss: " << handoverStats->GetPacketLoss() << std::endl;

    Simulator::Destroy();
    return 0;
}