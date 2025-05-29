#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t gridWidth = 5;
    uint32_t gridHeight = 5;
    double gridDistance = 30.0;
    std::string phyMode = "DsssRate11Mbps";
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    bool verbose = false;
    bool enablePcap = false;
    bool enableAscii = false;

    CommandLine cmd;
    cmd.AddValue("phyMode", "802.11b mode", phyMode);
    cmd.AddValue("gridWidth", "Grid width", gridWidth);
    cmd.AddValue("gridHeight", "Grid height", gridHeight);
    cmd.AddValue("gridDistance", "Grid distance (meters)", gridDistance);
    cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
    cmd.AddValue("numPackets", "Number of packets", numPackets);
    cmd.AddValue("sourceNode", "Source node index", sourceNode);
    cmd.AddValue("sinkNode", "Sink node index", sinkNode);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("enableAscii", "Enable ascii tracing", enableAscii);
    cmd.Parse(argc, argv);

    uint32_t numNodes = gridWidth * gridHeight;

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);

    // Setup distance-based propagation loss and RSSI cutoff
    double rssiCutoffDbm = -85.0;
    Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
    lossModel->SetReference(1.0, 7.9); // refDist = 1m, refLoss ~7.9dB @ 2.4GHz (default)
    lossModel->SetPathLossExponent(3.0);
    Ptr<RandomPropagationLossModel> randLoss = CreateObject<RandomPropagationLossModel>();
    lossModel->SetNext(randLoss);
    Ptr<ThresholdPropagationLossModel> thresholdLoss = CreateObject<ThresholdPropagationLossModel>();
    thresholdLoss->SetThresholdDbm(rssiCutoffDbm);
    lossModel->SetNext(thresholdLoss);
    channel->SetPropagationLossModel(lossModel);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(gridDistance),
                                 "DeltaY", DoubleValue(gridDistance),
                                 "GridWidth", UintegerValue(gridWidth),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    OlsrHelper olsr;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    if (verbose)
    {
        LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(sinkNode), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(sinkNode));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(15.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(15.0)));
    ApplicationContainer app = onoff.Install(nodes.Get(sourceNode));

    if (enablePcap)
    {
        wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        wifiPhy.EnablePcap("adhoc-wifi-grid", devices, true);
    }

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream;
    if (enableAscii)
    {
        stream = ascii.CreateFileStream("adhoc-wifi-grid.tr");
        wifiPhy.EnableAsciiAll(stream);
    }

    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}