#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wave-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer vehicles;
    vehicles.Create(2);

    YansWaveChannelHelper channel;
    YansWavePhyHelper phy = YansWavePhyHelper::Default();
    phy.SetChannel(channel.Create());

    WaveMacHelper mac;
    Wifi80211pHelper wifi80211p;
    NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(vehicles);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    // UDP Application
    uint16_t port = 9;
    UdpClientHelper client(i.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(12.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(13.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Bytes: " << i->second.lostBytes << "\n";
        std::cout << "  Delay Sum:  " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";
    }

    Simulator::Destroy();
    return 0;
}