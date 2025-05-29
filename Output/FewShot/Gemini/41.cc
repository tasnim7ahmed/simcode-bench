#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool tracing = false;
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    double interval = 1.0;
    std::string phyMode("DsssRate11Mbps");
    int sinkNode = 0;
    int sourceNode = 24;
    double txp = 7.5;
    uint32_t gridSize = 5;
    double gridSpacing = 10.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets generated", numPackets);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("sinkNode", "Specify the sink node", sinkNode);
    cmd.AddValue("sourceNode", "Specify the source node", sourceNode);
    cmd.AddValue("txp", "Transmit power in dBm", txp);
    cmd.AddValue("gridSize", "Side of the grid", gridSize);
    cmd.AddValue("gridSpacing", "Spacing of the grid nodes", gridSpacing);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(gridSize * gridSize);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("RxGain", DoubleValue(10));
    wifiPhy.Set("TxGain", DoubleValue(10));
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    wifiChannel.SetPropagationDelay("Model", StringValue("ns3::ConstantSpeedPropagationDelayModel"));
    wifiChannel.AddPropagationLoss("Model", StringValue("ns3::LogDistancePropagationLossModel"),
                                   "Exponent", DoubleValue(3.0),
                                   "ReferenceLoss", DoubleValue(46.6777));

    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(gridSpacing),
                                  "DeltaY", DoubleValue(gridSpacing),
                                  "GridWidth", UintegerValue(gridSize),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(sinkNode), 9));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(sinkNode));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.00001));
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        devices.Get(i)->SetAttribute("ReceiveErrorModel", PointerValue(em));
    }

    UdpClientHelper client(interfaces.GetAddress(sinkNode), 9);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(nodes.Get(sourceNode));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));


    if (tracing) {
        wifiPhy.EnablePcapAll("wifi-adhoc");
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("wifi-adhoc.tr"));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}