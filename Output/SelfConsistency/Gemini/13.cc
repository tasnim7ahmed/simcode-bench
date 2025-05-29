#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessNetwork");

void CourseChange(std::string context, Ptr<const MobilityModel> model) {
    Vector position = model->GetPosition();
    NS_LOG_UNCOND(context << " x = " << position.x << ", y = " << position.y);
}

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("MixedWiredWirelessNetwork", LOG_LEVEL_INFO);

    // Command line arguments
    uint32_t numBackboneRouters = 3;
    uint32_t numLanNodes = 2;
    uint32_t numWifiNodes = 2;
    double simulationTime = 10.0;
    bool enablePcapTracing = false;
    bool enableNetAnim = false;

    CommandLine cmd;
    cmd.AddValue("numBackboneRouters", "Number of backbone routers", numBackboneRouters);
    cmd.AddValue("numLanNodes", "Number of LAN nodes per router", numLanNodes);
    cmd.AddValue("numWifiNodes", "Number of WiFi nodes per router", numWifiNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("enablePcapTracing", "Enable PCAP tracing", enablePcapTracing);
    cmd.AddValue("enableNetAnim", "Enable NetAnim animation", enableNetAnim);
    cmd.Parse(argc, argv);

    // Create backbone routers
    NodeContainer backboneRouters;
    backboneRouters.Create(numBackboneRouters);

    // Install OLSR routing protocol
    OlsrHelper olsrRouting;
    InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(olsrRouting);
    internetStack.Install(backboneRouters);

    // Ad hoc WiFi network for backbone routers
    WifiHelper wifiAdhoc;
    wifiAdhoc.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifiAdhoc.SetRemoteStationManager("ns3::AarfWifiManager");
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer adhocDevices = wifiAdhoc.Install(wifiPhy, wifiMac, backboneRouters);

    // Mobility model for backbone routers
    MobilityHelper mobilityAdhoc;
    mobilityAdhoc.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                        "X", StringValue("50.0"),
                                        "Y", StringValue("50.0"),
                                        "Rho", StringValue("10.0"));
    mobilityAdhoc.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                    "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobilityAdhoc.Install(backboneRouters);

    for (uint32_t i = 0; i < backboneRouters.GetN(); ++i) {
        std::stringstream ss;
        ss << "/NodeList/" << backboneRouters.Get(i)->GetId() << "/$ns3::MobilityModel/CourseChange";
        Config::Connect(ss.str(), MakeCallback(&CourseChange));
    }


    // Create LAN nodes and connect to backbone routers
    NodeContainer lanNodes[numBackboneRouters];
    NetDeviceContainer lanDevices[numBackboneRouters];
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        lanNodes[i].Create(numLanNodes);
        internetStack.Install(lanNodes[i]);
        NetDeviceContainer link = p2p.Install(backboneRouters.Get(i), lanNodes[i]);
        lanDevices[i].Add(link.Get(0)); // Router interface
        for(uint32_t j = 1; j < link.GetN(); ++j){
             lanDevices[i].Add(link.Get(j));
        }

    }

    // Create WiFi infrastructure networks for each backbone router
    NodeContainer wifiNodes[numBackboneRouters];
    NetDeviceContainer staDevices[numBackboneRouters];
    NetDeviceContainer apDevices[numBackboneRouters];
    WifiHelper wifiInfra;
    wifiInfra.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifiInfra.SetRemoteStationManager("ns3::AarfWifiManager");
    WifiMacHelper wifiMacInfra;
    Ssid ssid = Ssid("ns3-ssid");
    wifiMacInfra.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        wifiNodes[i].Create(numWifiNodes);
        internetStack.Install(wifiNodes[i]);

        staDevices[i] = wifiInfra.Install(wifiPhy, wifiMacInfra, wifiNodes[i]);

        wifiMacInfra.SetType("ns3::ApWifiMac",
                              "Ssid", SsidValue(ssid));

        apDevices[i] = wifiInfra.Install(wifiPhy, wifiMacInfra, backboneRouters.Get(i));
    }

    // Mobility model for WiFi nodes
    MobilityHelper mobilityWifi;
    mobilityWifi.SetPositionAllocator("ns3::GridPositionAllocator",
                                      "MinX", DoubleValue(0.0),
                                      "MinY", DoubleValue(0.0),
                                      "DeltaX", DoubleValue(5.0),
                                      "DeltaY", DoubleValue(5.0),
                                      "GridWidth", UintegerValue(3),
                                      "LayoutType", StringValue("RowFirst"));
    mobilityWifi.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        mobilityWifi.Install(wifiNodes[i]);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer adhocInterfaces = address.Assign(adhocDevices);

    uint32_t interfaceIndex = 1;
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
      address.SetBase( ("10.1." + std::to_string(interfaceIndex) + ".0").c_str(), "255.255.255.0");
      Ipv4InterfaceContainer lanInterfaces = address.Assign(lanDevices[i]);
      interfaceIndex++;
      address.SetBase( ("10.1." + std::to_string(interfaceIndex) + ".0").c_str(), "255.255.255.0");
      Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices[i]);
      address.Assign(apDevices[i]);
      interfaceIndex++;
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP flow from a wired LAN node to a wireless STA
    uint16_t port = 9; // Discard port (RFC 863)
    UdpClientServerHelper udpsink(port);
    udpsink.SetAttribute ("MaxPackets", UintegerValue (1000000));
    udpsink.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = udpsink.Install (lanNodes[0].Get(0));
    ApplicationContainer serverApps = udpsink.Install (wifiNodes[numBackboneRouters - 1].Get(numWifiNodes -1));

    Ipv4Address sinkAddress = wifiNodes[numBackboneRouters - 1].Get(numWifiNodes -1)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();

    Ptr<UdpClient> client = DynamicCast<UdpClient> (clientApps.Get (0));
    client->SetRemote (sinkAddress, port);

    clientApps.Start (Seconds (1.0));
    serverApps.Start (Seconds (0.0));
    clientApps.Stop (Seconds (simulationTime - 1));
    serverApps.Stop (Seconds (simulationTime));


    // Enable PCAP tracing
    if (enablePcapTracing) {
        p2p.EnablePcapAll("mixed-wired-wireless");
        wifiPhy.EnablePcapAll("mixed-wired-wireless");
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable NetAnim
    if (enableNetAnim) {
        AnimationInterface anim("mixed-wired-wireless.xml");
        anim.SetMaxPktsPerTraceFile(500000);
        // Optionally configure the animation.
    }


    NS_LOG_INFO("Running simulation...");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets ();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	  NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
	  NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
	  NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
	  NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps");
      }


    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}