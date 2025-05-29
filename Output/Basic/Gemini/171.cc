#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char* argv[]) {
    bool enablePcap = true;
    std::string phyMode("DsssRate1Mbps");
    double simulationTime = 30.0;
    uint32_t numNodes = 10;

    CommandLine cmd;
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("simulationTime", "Simulation time (s)", simulationTime);
    cmd.AddValue("numNodes", "Number of nodes", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"));
    mobility.Install(nodes);

    uint16_t port = 9;
    UdpClientServerHelper csh(port);
    csh.SetAttribute("MaxBytes", UintegerValue(1024));

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t destNode = (i + 1) % numNodes;
        Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable>();
        startTimeSeconds->SetAttribute("Min", DoubleValue(0.0));
        startTimeSeconds->SetAttribute("Max", DoubleValue(5.0));

        clientApps.Add(csh.CreateClient(interfaces.GetAddress(destNode), port));
        clientApps.Get(i)->SetStartTime(Seconds(startTimeSeconds->GetValue()));
        clientApps.Get(i)->SetStopTime(Seconds(simulationTime));

        serverApps.Add(csh.CreateServer(interfaces.GetAddress(i)));
        serverApps.Get(i)->SetStartTime(Seconds(0.0));
        serverApps.Get(i)->SetStopTime(Seconds(simulationTime));
    }

    if (enablePcap) {
        wifiPhy.EnablePcapAll("adhoc");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalPacketsSent = 0;
    double totalPacketsReceived = 0;
    double totalDelay = 0;
    int numFlows = 0;

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        totalPacketsSent += i->second.txPackets;
        totalPacketsReceived += i->second.rxPackets;
        totalDelay += i->second.delaySum.GetSeconds();
        numFlows++;
    }

    double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
    double avgEndToEndDelay = (totalPacketsReceived > 0) ? (totalDelay / totalPacketsReceived) : 0;

    std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
    std::cout << "Average End-to-End Delay: " << avgEndToEndDelay << " seconds" << std::endl;

    Simulator::Destroy();
    return 0;
}