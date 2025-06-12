#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiGridSimulation");

int main(int argc, char *argv[])
{
    uint32_t gridRows = 5;
    uint32_t gridCols = 5;
    double gridDistance = 50.0;
    std::string phyMode("DsssRate1Mbps");
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    bool verbose = false;
    bool pcapTracing = false;
    bool asciiTracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("gridRows", "Number of rows in the grid", gridRows);
    cmd.AddValue("gridCols", "Number of columns in the grid", gridCols);
    cmd.AddValue("gridDistance", "Distance between nodes in meters", gridDistance);
    cmd.AddValue("phyMode", "WiFi Phy mode (e.g., DsssRate1Mbps)", phyMode);
    cmd.AddValue("packetSize", "Size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("sourceNode", "Index of source node", sourceNode);
    cmd.AddValue("sinkNode", "Index of sink node", sinkNode);
    cmd.AddValue("verbose", "Turn on all log components", verbose);
    cmd.AddValue("pcap", "Enable pcap tracing", pcapTracing);
    cmd.AddValue("ascii", "Enable ASCII tracing", asciiTracing);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        LogComponentEnable("AdhocWifiGridSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    uint32_t totalNodes = gridRows * gridCols;
    NS_ABORT_IF(sourceNode >= totalNodes || sinkNode >= totalNodes);

    NodeContainer nodes;
    nodes.Create(totalNodes);

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-80));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-90));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    Ptr<GridPositionAllocator> gridAlloc = CreateObject<GridPositionAllocator>();
    gridAlloc->SetMinX(0.0);
    gridAlloc->SetMinY(0.0);
    gridAlloc->SetDeltaX(gridDistance);
    gridAlloc->SetDeltaY(gridDistance);
    gridAlloc->SetGridWidth(gridCols);
    gridAlloc->SetLayoutType(GridPositionAllocator::ROW_FIRST);
    mobility.SetPositionAllocator(gridAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    OlsrHelper olsr;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(sinkNode));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(sinkNode, 0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(sourceNode));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    if (pcapTracing)
    {
        wifiPhy.EnablePcapAll("adhoc_wifi_grid");
    }

    if (asciiTracing)
    {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("adhoc_wifi_grid.tr"));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}