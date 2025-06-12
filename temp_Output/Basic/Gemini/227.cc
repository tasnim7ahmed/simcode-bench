#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UAVNetworkSimulation");

int main(int argc, char* argv[]) {
    bool enablePcap = false;
    bool enableNetAnim = false;
    int numUAVs = 5;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
    cmd.AddValue("numUAVs", "Number of UAVs", numUAVs);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetLogLevel(LOG_LEVEL_INFO);

    NodeContainer uavNodes;
    uavNodes.Create(numUAVs);
    NodeContainer gcsNode;
    gcsNode.Create(1);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingProtocol("ns3::AodvHelper", "EnableHello", BooleanValue(true));
    stack.Install(uavNodes);
    stack.Install(gcsNode);

    InternetAddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer uavInterfaces = address.Assign(uavNodes);
    Ipv4InterfaceContainer gcsInterfaces = address.Assign(gcsNode);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(3.5));
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer uavDevices = wifi.Install(wifiPhy, wifiMac, uavNodes);
    NetDeviceContainer gcsDevices = wifi.Install(wifiPhy, wifiMac, gcsNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Z", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=30.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gcsNode);

    mobility.SetMobilityModel("ns3::RandomWalk3dMobilityModel",
        "Bounds", BoxValue(Box(0, 0, 10, 100, 100, 30)),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5]"),
        "Mode", StringValue("Time"));
    mobility.Install(uavNodes);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(gcsNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient(gcsInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (int i = 0; i < numUAVs; ++i) {
        clientApps.Add(echoClient.Install(uavNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(60.0));

    if (enablePcap) {
        wifiPhy.EnablePcapAll("uav_simulation");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (enableNetAnim) {
        AnimationInterface anim("uav_simulation.xml");
        for (uint32_t i = 0; i < uavNodes.GetN(); ++i) {
            anim.UpdateNodeColor(uavNodes.Get(i), 0, 0, 255);
        }
        anim.UpdateNodeColor(gcsNode.Get(0), 255, 0, 0);
    }

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}