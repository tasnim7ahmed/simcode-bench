#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string phyMode = "DsssRate1Mbps";
    double rss = -80.0;
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    double interval = 1.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set to true", verbose);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("rss", "Received signal strength", rss);
    cmd.AddValue("packetSize", "Size of packet sent", packetSize);
    cmd.AddValue("numPackets", "Number of packets generated", numPackets);
    cmd.AddValue("interval", "Interval between packets", interval);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("CcaEdThreshold", DoubleValue(-90));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90));
    wifiPhy.Set("RxSensitivity", DoubleValue(rss - 10));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelay");
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));

    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    uint16_t port = 9;

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    wifiPhy.EnablePcap("fixed-rss", devices);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}