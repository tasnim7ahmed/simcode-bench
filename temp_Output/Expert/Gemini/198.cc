#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/applications-module.h>
#include <ns3/wave-module.h>
#include <ns3/mobility-module.h>
#include <ns3/wifi-module.h>
#include <ns3/flow-monitor-module.h>
#include <iostream>

using namespace ns3;

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    WaveMacHelper mac;
    WavePhyHelper phy = WavePhyHelper::Default();
    WaveHelper waveHelper = WaveHelper::CreateRemote();
    NetDeviceContainer devices = waveHelper.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(5.0),
                                   "DeltaY", DoubleValue(0.0),
                                   "GridWidth", UintegerValue(2),
                                   "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    ApplicationContainer senderApp;
    ApplicationContainer receiverApp;

    uint16_t port = 9;

    UdpServerHelper server(port);
    receiverApp = server.Install(nodes.Get(1));
    receiverApp.Start(Seconds(1.0));
    receiverApp.Stop(Seconds(10.0));

    UdpClientHelper client(nodes.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    senderApp = client.Install(nodes.Get(0));
    senderApp.Start(Seconds(2.0));
    senderApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 << " kbps\n";
    }

    Simulator::Destroy();

    return 0;
}