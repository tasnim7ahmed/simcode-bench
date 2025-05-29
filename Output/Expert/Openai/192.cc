#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    double simTime = 10.0;
    std::string phyMode = "HtMcs7";
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10000;
    double interval = 0.01; // 100 packets/sec

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    // Wifi configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility: Line topology (0 ---- 1 ---- 2)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Applications
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(2), sinkPort));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime + 1));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("8Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer srcApp = onoff.Install(nodes.Get(0));
    srcApp.Start(Seconds(1.0));
    srcApp.Stop(Seconds(simTime + 1));

    // Enable tracing
    phy.EnablePcap("wireless-simple", devices);

    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wireless-simple-trace.tr"));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::ofstream outFile("throughput-results.txt");
    for (auto it = monitor->GetFlowStats().begin(); it != monitor->GetFlowStats().end(); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(2))
        {
            double throughput = it->second.rxBytes * 8.0 / simTime / 1e6; // Mbps
            outFile << "FlowId: " << it->first << " Src: " << t.sourceAddress
                    << " Dst: " << t.destinationAddress
                    << " Throughput: " << throughput << " Mbps" << std::endl;
        }
    }
    outFile.close();

    Simulator::Destroy();
    return 0;
}