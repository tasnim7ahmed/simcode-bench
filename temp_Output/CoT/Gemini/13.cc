#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWireless");

void CourseChange(std::string context, Ptr<const MobilityModel> model) {
    Vector position = model->GetPosition();
    NS_LOG_UNCOND(context << " x = " << position.x << ", y = " << position.y);
}

int main(int argc, char *argv[]) {
    bool tracing = false;
    bool enableNetAnim = false;
    uint32_t nBackboneRouters = 3;
    uint32_t nLanNodes = 2;
    uint32_t nWifiSta = 2;
    double simulationTime = 10;

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("nBackboneRouters", "Number of backbone routers", nBackboneRouters);
    cmd.AddValue("nLanNodes", "Number of LAN nodes per router", nLanNodes);
    cmd.AddValue("nWifiSta", "Number of wireless stations per router", nWifiSta);
    cmd.AddValue("enableNetAnim", "Enable network animation", enableNetAnim);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer backboneRouters;
    backboneRouters.Create(nBackboneRouters);

    OlsrHelper olsrRouting;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsrRouting);
    internet.Install(backboneRouters);

    WifiHelper wifiAdhoc;
    wifiAdhoc.SetStandard(WIFI_PHY_STANDARD_80211b);
    NqosWifiMacHelper macAdhoc = NqosWifiMacHelper::Default();
    macAdhoc.SetType("ns3::AdhocWifiMac", "QosSupported", BooleanValue(true));
    YansWifiPhyHelper phyAdhoc = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channelAdhoc = YansWifiChannelHelper::Create();
    phyAdhoc.SetChannel(channelAdhoc.Create());
    NetDeviceContainer adhocDevices = wifiAdhoc.Install(phyAdhoc, macAdhoc, backboneRouters);

    MobilityHelper mobilityAdhoc;
    mobilityAdhoc.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                        "X", StringValue("100.0"),
                                        "Y", StringValue("100.0"),
                                        "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=30]"));
    mobilityAdhoc.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                    "Bounds", RectangleValue(Rectangle(-150, 150, -150, 150)));
    mobilityAdhoc.Install(backboneRouters);
    for (uint32_t i = 0; i < backboneRouters.GetN(); ++i) {
        Ptr<MobilityModel> mob = backboneRouters.Get(i)->GetObject<MobilityModel>();
        std::stringstream ss;
        ss << "/NodeList/" << backboneRouters.Get(i)->GetId() << "/$ns3::MobilityModel";
        Config::Connect(ss.str() + "::CourseChange", MakeCallback(&CourseChange));
    }

    NodeContainer lanNodes[nBackboneRouters];
    NodeContainer wifiNodes[nBackboneRouters];
    NetDeviceContainer lanDevices[nBackboneRouters];
    NetDeviceContainer wifiDevices[nBackboneRouters];
    WifiHelper wifiInfra;
    wifiInfra.SetStandard(WIFI_PHY_STANDARD_80211b);
    WifiMacHelper macInfra;
    Ssid ssid = Ssid("ns-3-ssid");

    for (uint32_t i = 0; i < nBackboneRouters; ++i) {
        lanNodes[i].Create(nLanNodes);
        internet.Install(lanNodes[i]);

        wifiNodes[i].Create(nWifiSta);
        internet.Install(wifiNodes[i]);

        macInfra.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
        wifiDevices[i] = wifiInfra.Install(phyAdhoc, macInfra, wifiNodes[i]);

        macInfra.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer apDevice = wifiInfra.Install(phyAdhoc, macInfra, backboneRouters.Get(i));
        wifiDevices[i].Add(apDevice);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));
        lanDevices[i] = p2p.Install(backboneRouters.Get(i), lanNodes[i]);
    }

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer adhocInterfaces = address.Assign(adhocDevices);

    for (uint32_t i = 0; i < nBackboneRouters; ++i) {
        address.SetBase("10.1." + std::to_string(i + 2) + ".0", "255.255.255.0");
        Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices[i]);

        address.SetBase("10.1." + std::to_string(i + 10) + ".0", "255.255.255.0");
        Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiDevices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpServerHelper server(port);
    server.SetAttribute("MaxPackets", UintegerValue(1000000));
    ApplicationContainer apps = server.Install(wifiNodes[nBackboneRouters - 1].Get(nWifiSta - 1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime - 1));

    UdpClientHelper client(Ipv4Address("10.1." + std::to_string(nBackboneRouters + 9) + "." + std::to_string(nWifiSta)), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    apps = client.Install(lanNodes[0].Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(simulationTime - 1));

    if (tracing) {
        phyAdhoc.EnablePcapAll("mixed_wired_wireless");
    }

    if (enableNetAnim) {
        AnimationInterface anim("mixed_wired_wireless.xml");
        anim.SetMaxPktsPerTraceFile(5000000);
        anim.EnablePacketMetadata();
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Flow Duration: " << i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds() << " s\n";
    }

    Simulator::Destroy();
    return 0;
}