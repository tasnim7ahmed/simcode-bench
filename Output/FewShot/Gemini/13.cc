#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWireless");

void CourseChange(std::string context, Ptr<const MobilityModel> model) {
    Vector position = model->GetPosition();
    NS_LOG_UNCOND(context << " x = " << position.x << ", y = " << position.y);
}

int main(int argc, char *argv[]) {
    bool tracing = false;
    bool animation = false;
    uint32_t nBackboneRouters = 3;
    uint32_t nLanNodes = 2;
    uint32_t nWifiNodes = 2;

    CommandLine cmd;
    cmd.AddValue("nBackboneRouters", "Number of backbone routers", nBackboneRouters);
    cmd.AddValue("nLanNodes", "Number of LAN nodes per router", nLanNodes);
    cmd.AddValue("nWifiNodes", "Number of WiFi nodes per router", nWifiNodes);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("animation", "Enable NetAnim animation", animation);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer backboneRouters;
    backboneRouters.Create(nBackboneRouters);

    OlsrHelper olsrRouting;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsrRouting);
    internet.Install(backboneRouters);

    WifiHelper wifiAdhoc;
    wifiAdhoc.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifiAdhoc.SetRemoteStationManager("ns3::AarfWifiManager");
    NqosWifiMacHelper macAdhoc = NqosWifiMacHelper::Default();
    macAdhoc.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper phyAdhoc = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channelAdhoc = YansWifiChannelHelper::Create();
    phyAdhoc.SetChannel(channelAdhoc.Create());
    NetDeviceContainer adhocDevices = wifiAdhoc.Install(phyAdhoc, macAdhoc, backboneRouters);

    MobilityHelper mobilityAdhoc;
    mobilityAdhoc.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                        "X", StringValue("50.0"),
                                        "Y", StringValue("50.0"),
                                        "Z", StringValue("0.0"),
                                        "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
    mobilityAdhoc.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                    "Bounds", RectangleValue(Rectangle(0, 0, 100, 100)));
    mobilityAdhoc.Install(backboneRouters);

    for (uint32_t i = 0; i < backboneRouters.GetN(); ++i) {
        std::stringstream ss;
        ss << "/NodeList/" << backboneRouters.Get(i)->GetId() << "/$ns3::MobilityModel/CourseChange";
        Config::Connect(ss.str(), MakeCallback(&CourseChange));
    }

    Ipv4AddressHelper ipv4Adhoc;
    ipv4Adhoc.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer adhocInterfaces = ipv4Adhoc.Assign(adhocDevices);

    std::vector<NodeContainer> lanNodes(nBackboneRouters);
    std::vector<NodeContainer> wifiNodes(nBackboneRouters);
    std::vector<NetDeviceContainer> lanDevices(nBackboneRouters);
    std::vector<NetDeviceContainer> wifiInfraDevices(nBackboneRouters);
    std::vector<NetDeviceContainer> staDevices(nBackboneRouters);
    std::vector<Ipv4InterfaceContainer> lanInterfaces(nBackboneRouters);
    std::vector<Ipv4InterfaceContainer> wifiInfraInterfaces(nBackboneRouters);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    WifiHelper wifiInfra;
    wifiInfra.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifiInfra.SetRemoteStationManager("ns3::AarfWifiManager");

    NqosWifiMacHelper macInfraAp = NqosWifiMacHelper::Default();
    macInfraAp.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("ns3-ssid")));

    NqosWifiMacHelper macInfraSta = NqosWifiMacHelper::Default();
    macInfraSta.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("ns3-ssid")));

    YansWifiPhyHelper phyInfra = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channelInfra = YansWifiChannelHelper::Create();
    phyInfra.SetChannel(channelInfra.Create());

    Ipv4AddressHelper ipv4Infra;

    for (uint32_t i = 0; i < nBackboneRouters; ++i) {
        lanNodes[i].Create(nLanNodes);
        internet.Install(lanNodes[i]);

        wifiNodes[i].Create(nWifiNodes);
        internet.Install(wifiNodes[i]);

        NetDeviceContainer routerDevice = pointToPoint.Install(backboneRouters.Get(i), lanNodes[i].Get(0));
        lanDevices[i].Add(routerDevice.Get(0));
        lanDevices[i].Add(routerDevice.Get(1));

        NetDeviceContainer apDevice = wifiInfra.Install(phyInfra, macInfraAp, backboneRouters.Get(i));
        wifiInfraDevices[i].Add(apDevice);

        staDevices[i] = wifiInfra.Install(phyInfra, macInfraSta, wifiNodes[i]);
        wifiInfraDevices[i].Add(staDevices[i]);

        ipv4Infra.SetBase("10.1." + std::to_string(i+1) + ".0", "255.255.255.0");

        lanInterfaces[i] = ipv4Infra.Assign(lanDevices[i]);
        wifiInfraInterfaces[i] = ipv4Infra.Assign(wifiInfraDevices[i]);

        MobilityHelper mobilityInfra;
        mobilityInfra.SetPositionAllocator("ns3::GridPositionAllocator",
                                             "MinX", DoubleValue(100.0 + i * 50.0),
                                             "MinY", DoubleValue(100.0),
                                             "DeltaX", DoubleValue(5.0),
                                             "DeltaY", DoubleValue(5.0),
                                             "GridWidth", UintegerValue(nWifiNodes),
                                             "LayoutType", StringValue("RowFirst"));
        mobilityInfra.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobilityInfra.Install(wifiNodes[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpClientHelper client(wifiInfraInterfaces[nBackboneRouters-1].GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(1024);

    ApplicationContainer clientApps = client.Install(lanNodes[0].Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiNodes[nBackboneRouters-1].Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    if (tracing) {
        phyAdhoc.EnablePcapAll("adhoc");
        phyInfra.EnablePcapAll("infra");
        pointToPoint.EnablePcapAll("lan");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    if (animation) {
        AnimationInterface anim("mixed-wired-wireless.xml");
    }

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

    Simulator::Destroy();

    return 0;
}