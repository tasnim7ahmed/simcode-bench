#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessNetwork");

class MixedNetworkSimulation {
public:
    MixedNetworkSimulation();
    void Run(int argc, char *argv[]);
private:
    void SetupMobility(NodeContainer &nodes, bool adhoc);
    void CourseChangeCallback(std::string context, const MobilityModel *model);

    uint32_t m_numBackboneRouters;
    uint32_t m_numLanNodesPerRouter;
    uint32_t m_numWirelessStasPerAp;
    double m_simDuration;
    bool m_traceMobility;
    bool m_enableCourseChangeCb;
};

MixedNetworkSimulation::MixedNetworkSimulation()
    : m_numBackboneRouters(3),
      m_numLanNodesPerRouter(2),
      m_numWirelessStasPerAp(2),
      m_simDuration(10.0),
      m_traceMobility(false),
      m_enableCourseChangeCb(false)
{
}

void
MixedNetworkSimulation::CourseChangeCallback(std::string context, const MobilityModel *model)
{
    Vector pos = model->GetPosition();
    Vector vel = model->GetVelocity();
    std::cout << Simulator::Now().GetSeconds() << " " << context
              << " POS: x=" << pos.x << ", y=" << pos.y << ", z=" << pos.z
              << " VEL: dx=" << vel.x << ", dy=" << vel.y << ", dz=" << vel.z
              << std::endl;
}

void
MixedNetworkSimulation::SetupMobility(NodeContainer &nodes, bool adhoc)
{
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    if (adhoc) {
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)));
    } else {
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }

    mobility.Install(nodes);

    if (m_traceMobility) {
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mobility/Movement",
                        MakeCallback(&MixedNetworkSimulation::CourseChangeCallback, this));
    }
}

void
MixedNetworkSimulation::Run(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("numBackboneRouters", "Number of backbone routers", m_numBackboneRouters);
    cmd.AddValue("numLanNodesPerRouter", "Number of LAN nodes per router", m_numLanNodesPerRouter);
    cmd.AddValue("numWirelessStasPerAp", "Number of wireless stations per AP", m_numWirelessStasPerAp);
    cmd.AddValue("simDuration", "Simulation duration in seconds", m_simDuration);
    cmd.AddValue("traceMobility", "Enable mobility tracing", m_traceMobility);
    cmd.AddValue("enableCourseChangeCb", "Enable course change callback", m_enableCourseChangeCb);
    cmd.Parse(argc, argv);

    NodeContainer backboneRouters;
    backboneRouters.Create(m_numBackboneRouters);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer backboneDevices;
    for (uint32_t i = 0; i < m_numBackboneRouters - 1; ++i) {
        backboneDevices.Add(p2p.Install(backboneRouters.Get(i), backboneRouters.Get(i + 1)));
    }

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer adhocDevices = wifi.Install(wifiPhy, wifiMac, backboneRouters);

    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(staticRouting, 0);
    listRouting.Add(olsr, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(backboneRouters);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.255.0");
    ipv4.Assign(backboneDevices);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer adhocInterfaces = ipv4.Assign(adhocDevices);

    std::vector<NodeContainer> lanNodes(m_numBackboneRouters);
    std::vector<NetDeviceContainer> lanDevices(m_numBackboneRouters);
    std::vector<Ipv4InterfaceContainer> lanInterfaces(m_numBackboneRouters);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    for (uint32_t r = 0; r < m_numBackboneRouters; ++r) {
        lanNodes[r].Create(m_numLanNodesPerRouter);
        internet.Install(lanNodes[r]);
        lanDevices[r] = csma.Install(backboneRouters.Get(r)->GetDevice(0), lanNodes[r]);
        ipv4.NewNetwork();
        lanInterfaces[r] = ipv4.Assign(lanDevices[r]);
    }

    std::vector<WifiApHelper> apHelpers(m_numBackboneRouters);
    std::vector<NetDeviceContainer> apDevices(m_numBackboneRouters);
    std::vector<NetDeviceContainer> staDevices(m_numBackboneRouters);
    std::vector<Ipv4InterfaceContainer> apInterfaces(m_numBackboneRouters);
    std::vector<Ipv4InterfaceContainer> staInterfaces(m_numBackboneRouters);

    for (uint32_t r = 0; r < m_numBackboneRouters; ++r) {
        wifiMac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(Ssid("network-" + std::to_string(r))),
                        "BeaconInterval", TimeValue(Seconds(2.5)));
        apHelpers[r].SetRemoteStationManager("ns3::ArfWifiManager");
        apHelpers[r].SetPhy(wifiPhy);
        apHelpers[r].SetMac(wifiMac);
        apDevices[r] = apHelpers[r].Install(backboneRouters.Get(r));

        NodeContainer stas;
        stas.Create(m_numWirelessStasPerAp);
        internet.Install(stas);

        wifiMac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(Ssid("network-" + std::to_string(r))),
                        "ActiveProbing", BooleanValue(false));
        staDevices[r] = wifi.Install(wifiPhy, wifiMac, stas);
        csma.Install(backboneRouters.Get(r)->GetDevice(0), apDevices[r]);

        ipv4.NewNetwork();
        apInterfaces[r] = ipv4.Assign(apDevices[r]);
        ipv4.NewNetwork();
        staInterfaces[r] = ipv4.Assign(staDevices[r]);

        SetupMobility(stas, false);
    }

    SetupMobility(backboneRouters, true);

    if (m_enableCourseChangeCb) {
        Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mobility/Movement",
                        MakeCallback(&MixedNetworkSimulation::CourseChangeCallback, this));
    }

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(staInterfaces.back().GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(lanNodes[0].Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(m_simDuration));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer sourceApps = onoff.Install(lanNodes[0].Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(m_simDuration - 0.1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (m_traceMobility) {
        AsciiTraceHelper ascii;
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("mobility-trace.tr");
        wifiPhy.EnableAsciiAll(stream);
    }

    Simulator::Stop(Seconds(m_simDuration));
    Simulator::Run();
    Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
    MixedNetworkSimulation sim;
    sim.Run(argc, argv);
}