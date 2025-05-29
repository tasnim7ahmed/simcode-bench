#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocFixedRssExample");

int main(int argc, char *argv[])
{
    double rss = -80.0; // dBm
    uint32_t packetSize = 1000;
    double packetInterval = 1.0; // seconds
    uint32_t numPackets = 1;
    bool verbose = false;
    bool pcap = true;
    double rxThresholdDbm = -90.0;

    CommandLine cmd;
    cmd.AddValue("rss", "Received Signal Strength [dBm]", rss);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("interval", "Packet interval in seconds", packetInterval);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("pcap", "Enable PCAP tracing", pcap);
    cmd.AddValue("rxThresholdDbm", "RX sensitivity threshold (dBm)", rxThresholdDbm);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("AdhocFixedRssExample", LOG_LEVEL_INFO);
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
        LogComponentEnable("WifiPhy", LOG_LEVEL_INFO);
        LogComponentEnable("YansWifiPhy", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiPhy.Set("RxSensitivity", DoubleValue(std::pow(10.0, rxThresholdDbm/10.0)/1000.0)); // convert dBm to watts

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 7000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer senderApp = onoff.Install(nodes.Get(0));
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(10.0));

    if (pcap)
    {
        wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        wifiPhy.EnablePcap("adhoc-fixedrss-node0", devices.Get(0), true, true);
        wifiPhy.EnablePcap("adhoc-fixedrss-node1", devices.Get(1), true, true);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}