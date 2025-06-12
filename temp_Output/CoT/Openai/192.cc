#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeWifiLine");

int main(int argc, char *argv[])
{
    double simulationTime = 10.0; // seconds
    std::string phyMode("HtMcs7");
    uint32_t payloadSize = 1024;

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Duration of the simulation", simulationTime);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    // Wifi and channel configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-wifi-network");

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));
    NetDeviceContainer devices;
    wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: place nodes in a line, 50m apart
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up routing on the middle node
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 5000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(2), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime + 1));

    // Set up CBR traffic (OnOffApplication)
    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("10Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app = onoff.Install(nodes.Get(0));

    // Tracing
    wifiPhy.EnablePcap("wifi-line", devices);
    AsciiTraceHelper ascii;
    wifiPhy.EnableAscii(ascii.CreateFileStream("wifi-line.tr"), devices);

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // Throughput calculation
    double throughput = 0;
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    if (sink)
    {
        uint64_t totalBytes = sink->GetTotalRx();
        throughput = totalBytes * 8.0 / (simulationTime * 1000000.0); // Mbps
        std::ofstream outfile("throughput.txt");
        outfile << "End-to-end throughput: " << throughput << " Mbps" << std::endl;
        outfile.close();
    }

    // Flow Monitor analysis
    monitor->SerializeToXmlFile("wifi-line-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}