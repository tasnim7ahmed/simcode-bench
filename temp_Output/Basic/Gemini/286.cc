#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetLogLevel(
            "UdpClient", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("100.0"),
                                  "Y", StringValue("100.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=5]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    mobility.Install(nodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "RtsCtsThreshold", UintegerValue(2200));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper server(4000);
    ApplicationContainer serverApp = server.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpClientHelper client(interfaces.GetAddress(4), 4000);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(20.0));

    /*FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();*/

    //Enable PCAP Tracing
    wifiPhy.EnablePcapAll("manet");

    Simulator::Run();

    /*monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }*/

    Simulator::Destroy();

    return 0;
}