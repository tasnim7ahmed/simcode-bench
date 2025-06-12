#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessTopology");

class NetworkScenario {
public:
    NetworkScenario(uint32_t backboneRouters, uint32_t lanNodesPerRouter, uint32_t wirelessStasPerInfra)
        : m_backboneRouters(backboneRouters),
          m_lanNodesPerRouter(lanNodesPerRouter),
          m_wirelessStasPerInfra(wirelessStasPerInfra) {}

    void Setup();
    void SetupMobility();
    void SetupApplications();
    void EnableTracing();

private:
    uint32_t m_backboneRouters;
    uint32_t m_lanNodesPerRouter;
    uint32_t m_wirelessStasPerInfra;

    NodeContainer m_backboneNodes;
    std::vector<NodeContainer> m_lanNodes;
    std::vector<NetDeviceContainer> m_lanDevices;
    std::vector<Ipv4InterfaceContainer> m_lanInterfaces;

    NodeContainer m_apNodes;
    NodeContainer m_staNodes;
    NetDeviceContainer m_adhocDevices;
    NetDeviceContainer m_apDevices;
    NetDeviceContainer m_staDevices;
    Ipv4InterfaceContainer m_adhocInterfaces;
    std::vector<Ipv4InterfaceContainer> m_apInterfaces;
    std::vector<Ipv4InterfaceContainer> m_staInterfaces;

    WifiHelper m_wifi;
    YansWifiPhyHelper m_phy;
    WifiMacHelper m_mac;
    CsmaHelper m_csma;
    PointToPointHelper m_p2p;
};

void CourseChangeCallback(std::string context, const MobilityModel *model) {
    Vector position = model->GetPosition();
    NS_LOG_DEBUG(context << " course change to position (" << position.x << "," << position.y << ")");
}

void NetworkScenario::Setup() {
    // Create backbone routers (Ad Hoc WiFi)
    m_backboneNodes.Create(m_backboneRouters);

    // Create LAN nodes per router
    m_lanNodes.resize(m_backboneRouters);
    for (uint32_t i = 0; i < m_backboneRouters; ++i) {
        m_lanNodes[i].Create(m_lanNodesPerRouter);
    }

    // Create AP and STA nodes for infrastructure networks
    m_apNodes.Create(m_backboneRouters);
    m_staNodes.Create(m_wirelessStasPerInfra);

    // Setup Ad Hoc WiFi for backbone
    YansWifiChannelHelper adhocChannel = YansWifiChannelHelper::Default();
    m_phy.SetChannel(adhocChannel.Create());
    m_mac.SetType("ns3::AdhocWifiMac");
    m_wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));
    m_adhocDevices = m_wifi.Install(m_phy, m_mac, m_backboneNodes);

    // Setup Infrastructure WiFi
    for (uint32_t i = 0; i < m_backboneRouters; ++i) {
        // Setup AP
        m_mac.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(Ssid("wifi-infra-" + std::to_string(i))),
                      "BeaconInterval", TimeValue(Seconds(2.5)));
        NetDeviceContainer apDev = m_wifi.Install(m_phy, m_mac, m_apNodes.Get(i));
        m_apDevices.Add(apDev);

        // Setup STAs
        m_mac.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(Ssid("wifi-infra-" + std::to_string(i))));
        NetDeviceContainer staDev = m_wifi.Install(m_phy, m_mac, m_staNodes);
        m_staDevices.Add(staDev);
    }

    // Connect LANs via CSMA
    m_csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    m_csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    m_lanDevices.resize(m_backboneRouters);
    m_lanInterfaces.resize(m_backboneRouters);
    InternetStackHelper stack;
    stack.InstallAll();

    for (uint32_t i = 0; i < m_backboneRouters; ++i) {
        NodeContainer lanNodesWithRouter = NodeContainer(m_backboneNodes.Get(i), m_lanNodes[i]);
        NetDeviceContainer lanDevices = m_csma.Install(lanNodesWithRouter);
        m_lanDevices[i] = lanDevices;

        Ipv4AddressHelper lanAddress;
        lanAddress.SetBase("192.168." + std::to_string(i) + ".0", "255.255.255.0");
        m_lanInterfaces[i] = lanAddress.Assign(lanDevices);
    }

    // Assign IP addresses to backbone (Ad Hoc WiFi)
    Ipv4AddressHelper adhocAddress;
    adhocAddress.SetBase("10.0.0.0", "255.255.255.0");
    m_adhocInterfaces = adhocAddress.Assign(m_adhocDevices);

    // Assign IP addresses to AP and STA
    m_apInterfaces.resize(m_backboneRouters);
    m_staInterfaces.resize(m_backboneRouters);
    for (uint32_t i = 0; i < m_backboneRouters; ++i) {
        Ipv4AddressHelper apAddress;
        apAddress.SetBase("172.16." + std::to_string(i) + ".0", "255.255.255.0");
        m_apInterfaces[i] = apAddress.Assign(m_apDevices.Get(i));

        Ipv4AddressHelper staAddress;
        staAddress.SetBase("172.17." + std::to_string(i) + ".0", "255.255.255.0");
        m_staInterfaces[i] = staAddress.Assign(m_staDevices.Get(i));
    }

    // Setup routing with OLSR on backbone
    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 0);
    list.Add(olsr, 10);

    for (uint32_t i = 0; i < m_backboneRouters; ++i) {
        list.Install(m_backboneNodes.Get(i));
    }
}

void NetworkScenario::SetupMobility() {
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.InstallAll();

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mobility/Model/CourseChange",
                    MakeCallback(&CourseChangeCallback));
}

void NetworkScenario::SetupApplications() {
    // UDP flow from first LAN node to last wireless STA
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(m_staInterfaces[m_backboneRouters - 1].GetAddress(0), port));
    PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSink.Install(m_staNodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onoff.Install(m_lanNodes[0].Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
}

void NetworkScenario::EnableTracing() {
    m_wifi.EnablePcapAll("mixed_topology");
    m_csma.EnablePcapAll("mixed_topology");
}

int main(int argc, char *argv[]) {
    uint32_t backboneRouters = 3;
    uint32_t lanNodesPerRouter = 2;
    uint32_t wirelessStasPerInfra = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("backboneRouters", "Number of backbone routers", backboneRouters);
    cmd.AddValue("lanNodesPerRouter", "Number of LAN nodes per backbone router", lanNodesPerRouter);
    cmd.AddValue("wirelessStasPerInfra", "Number of wireless STAs per infrastructure network", wirelessStasPerInfra);
    cmd.Parse(argc, argv);

    NetworkScenario scenario(backboneRouters, lanNodesPerRouter, wirelessStasPerInfra);
    scenario.Setup();
    scenario.SetupMobility();
    scenario.SetupApplications();
    scenario.EnableTracing();

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}