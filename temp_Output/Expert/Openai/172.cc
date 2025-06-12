#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSim");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 10;
    double simTime = 30.0;
    double areaSize = 500.0;
    uint16_t port = 4000;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 2. Mobility model: RandomWaypoint
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable>();
    speed->SetAttribute("Min", DoubleValue(1.0));
    speed->SetAttribute("Max", DoubleValue(20.0));
    mobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed", PointerValue(speed),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
        "PositionAllocator",
            PointerValue(
                CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]")
                )
            )
    );
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (20.0),
                                   "DeltaY", DoubleValue (20.0),
                                   "GridWidth", UintegerValue (5),
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.Install(nodes);

    // 3. Wifi: 802.11b ad-hoc
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 4. Internet stack + AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 5. UDP Traffic: each node sends to another (randomly chosen), randomized start times
    uint32_t numApps = numNodes;
    ApplicationContainer clientApps, serverApps;
    Ptr<UniformRandomVariable> startTimeRv = CreateObject<UniformRandomVariable>();
    startTimeRv->SetAttribute("Min", DoubleValue(1.0));
    startTimeRv->SetAttribute("Max", DoubleValue(5.0));
    Ptr<UniformRandomVariable> destRv = CreateObject<UniformRandomVariable>();
    destRv->SetAttribute ("Min", DoubleValue(0.0));
    destRv->SetAttribute ("Max", DoubleValue(static_cast<double>(numNodes-1)));
    uint32_t nPackets = 500;
    uint32_t packetSize = 512;
    double interval = 0.05; // 20 packets/s per client

    std::vector<uint32_t> dests(numNodes);
    for (uint32_t i = 0; i < numNodes; ++i) dests[i] = i;

    // randomize destination assignment (no loopbacks)
    do
    {
        std::random_shuffle(dests.begin(), dests.end());
    }
    while (std::any_of(dests.begin(), dests.end(), [=](uint32_t x, uint32_t i){return x==i;}) );

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        // Each node runs a server
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(nodes.Get(dests[i]));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simTime));
        serverApps.Add(serverApp);

        // Each node runs a client to another node (not self)
        UdpClientHelper client(interfaces.GetAddress(dests[i]), port);
        client.SetAttribute("MaxPackets", UintegerValue(nPackets));
        client.SetAttribute("Interval", TimeValue(Seconds(interval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        double startTime = startTimeRv->GetValue();
        clientApp.Start(Seconds(startTime));
        clientApp.Stop(Seconds(simTime));
        clientApps.Add(clientApp);
    }

    // 6. Pcap tracing
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
    wifiPhy.EnablePcapAll("adhoc-aodv");

    // 7. Flow Monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Compute PDR and avg E2E delay
    double deliveredPackets = 0;
    double sentPackets = 0;
    double sumDelay = 0;

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (const auto &kv : stats)
    {
        sentPackets += kv.second.txPackets;
        deliveredPackets += kv.second.rxPackets;
        sumDelay += kv.second.delaySum.GetSeconds();
    }

    double pdr = (sentPackets > 0) ? (deliveredPackets / sentPackets) * 100.0 : 0.0;
    double avgDelay = (deliveredPackets > 0) ? (sumDelay / deliveredPackets) : 0.0;

    std::cout << "========== Simulation Results ==========" << std::endl;
    std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}