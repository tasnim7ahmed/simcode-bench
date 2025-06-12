#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numBackboneRouters = 3;
    uint32_t numLanNodes = 2;
    uint32_t numWifiNodes = 2;
    double simulationTime = 10.0;
    bool enablePcapTracing = false;
    bool enableCourseChangeCallback = false;

    CommandLine cmd;
    cmd.AddValue("numBackboneRouters", "Number of backbone routers", numBackboneRouters);
    cmd.AddValue("numLanNodes", "Number of LAN nodes per router", numLanNodes);
    cmd.AddValue("numWifiNodes", "Number of WiFi nodes per router", numWifiNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("enablePcapTracing", "Enable PCAP tracing", enablePcapTracing);
    cmd.AddValue("enableCourseChangeCallback", "Enable course change callback", enableCourseChangeCallback);
    cmd.Parse(argc, argv);

    NodeContainer backboneRouters;
    backboneRouters.Create(numBackboneRouters);

    NodeContainer lanNodes[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        lanNodes[i].Create(numLanNodes);
    }

    NodeContainer wifiApNodes[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        wifiApNodes[i].Create(1);
    }

    NodeContainer wifiStaNodes[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        wifiStaNodes[i].Create(numWifiNodes);
    }

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    // Infrastructure network
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        staDevices[i] = wifi.Install(phy, mac, wifiStaNodes[i]);
    }

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        apDevices[i] = wifi.Install(phy, mac, wifiApNodes[i]);
    }

    // Ad hoc network
    WifiHelper adhocWifi;
    adhocWifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    YansWifiChannelHelper adhocChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper adhocPhy = YansWifiPhyHelper::Default();
    adhocPhy.SetChannel(adhocChannel.Create());
    WifiMacHelper adhocMac;
    adhocMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer adhocDevices = adhocWifi.Install(adhocPhy, adhocMac, backboneRouters);

    // LAN network
    NodeContainer lanHubs[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i)
    {
        lanHubs[i].Create(1);
    }

    PointToPointHelper p2pLan;
    p2pLan.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pLan.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer lanDevices[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        NetDeviceContainer hubRouterDevices = p2pLan.Install(lanHubs[i].Get(0), backboneRouters.Get(i));
        NetDeviceContainer hubLanDevices = p2pLan.Install(lanHubs[i].Get(0), lanNodes[i]);
        lanDevices[i].Add(hubRouterDevices.Get(0));
        lanDevices[i].Add(hubRouterDevices.Get(1));
        for(uint32_t j = 0; j < numLanNodes; ++j)
        {
            lanDevices[i].Add(hubLanDevices.Get(j));
        }
    }

    // Install Internet stack
    InternetStackHelper stack;
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        stack.Install(lanNodes[i]);
        stack.Install(wifiStaNodes[i]);
        stack.Install(wifiApNodes[i]);
    }
    stack.Install(backboneRouters);

    OlsrHelper olsr;
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Assign IP addresses
    Ipv4AddressHelper address;

    Ipv4InterfaceContainer lanInterfaces[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        address.SetBase("10.1." + std::to_string(i+1) + ".0", "255.255.255.0");
        lanInterfaces[i] = address.Assign(lanDevices[i]);
    }

    Ipv4InterfaceContainer wifiInterfaces[numBackboneRouters];
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        address.SetBase("10.2." + std::to_string(i+1) + ".0", "255.255.255.0");
        wifiInterfaces[i] = address.Assign(apDevices[i]);
        Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices[i]);
        wifiInterfaces[i].Add(staInterfaces);

    }

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer adhocInterfaces = address.Assign(adhocDevices);

    // Install OLSR on backbone routers
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        OlsrHelper olsrRouting;
        Ipv4StaticRoutingHelper staticRouting;
        olsrRouting.SetProtocol(staticRouting);
        stack.SetRoutingHelper(olsrRouting);
    }

    // UDP flow from LAN node 0 to wifi STA node in the last infrastructure network
    uint16_t port = 9;
    UdpClientHelper client(wifiInterfaces[numBackboneRouters-1].GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(lanNodes[0].Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 1.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiStaNodes[numBackboneRouters-1].Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime - 1.0));

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(numBackboneRouters),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(backboneRouters);

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        MobilityHelper wifiMobility;
        wifiMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(100.0 + i * 50),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(10.0),
                                      "GridWidth", UintegerValue(numWifiNodes),
                                      "LayoutType", StringValue("RowFirst"));
        wifiMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
        wifiMobility.Install(wifiStaNodes[i]);
    }

    if(enableCourseChangeCallback)
    {
        for (uint32_t i = 0; i < numBackboneRouters; ++i)
        {
            for (uint32_t j = 0; j < backboneRouters.GetN(); ++j)
            {
                Ptr<MobilityModel> mob = backboneRouters.Get(j)->GetObject<MobilityModel>();
                mob->TraceConnectWithoutContext("CourseChange", MakeCallback (&[](std::string path, Ptr<const MobilityModel> model){
                    Vector pos = model->GetPosition ();
                    std::cout << path << " x=" << pos.x << ", y=" << pos.y << std::endl;
                }));
            }
        }
    }

    // Tracing
    if (enablePcapTracing) {
        phy.EnablePcapAll("mixed_network");
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable NetAnim
    AnimationInterface anim("mixed_network.xml");
    anim.SetConstantPosition(backboneRouters.Get(0), 10, 10);
    anim.SetConstantPosition(backboneRouters.Get(1), 30, 10);
    anim.SetConstantPosition(backboneRouters.Get(2), 50, 10);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

    Simulator::Destroy();
    return 0;
}