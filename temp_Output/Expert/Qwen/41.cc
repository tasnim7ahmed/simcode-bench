#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GridAdhocSimulation");

int main(int argc, char *argv[]) {
    uint32_t gridSizeX = 5;
    uint32_t gridSizeY = 5;
    std::string phyMode("DsssRate1Mbps");
    double gridDistance = 100.0; // in meters
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    uint32_t sourceNode = 24;
    uint32_t sinkNode = 0;
    bool enablePcap = false;
    bool enableAscii = false;
    bool verboseLogging = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("gridSizeX", "Grid size in X dimension", gridSizeX);
    cmd.AddValue("gridSizeY", "Grid size in Y dimension", gridSizeY);
    cmd.AddValue("phyMode", "Wifi Phy mode (e.g., DsssRate1Mbps)", phyMode);
    cmd.AddValue("gridDistance", "Distance between nodes in grid (meters)", gridDistance);
    cmd.AddValue("packetSize", "Size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("sourceNode", "Index of source node", sourceNode);
    cmd.AddValue("sinkNode", "Index of sink node", sinkNode);
    cmd.AddValue("enablePcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("enableAscii", "Enable ASCII tracing", enableAscii);
    cmd.AddValue("verbose", "Enable verbose logging", verboseLogging);
    cmd.Parse(argc, argv);

    if (verboseLogging) {
        LogComponentEnable("GridAdhocSimulation", LOG_LEVEL_INFO);
    }

    uint32_t totalNodes = gridSizeX * gridSizeY;
    NS_ABORT_MSG_IF(sourceNode >= totalNodes || sinkNode >= totalNodes,
                    "Source or Sink node index out of bounds.");

    NodeContainer nodes;
    nodes.Create(totalNodes);

    YansWifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    wifiPhy.Set("ChannelWidth", UintegerValue(22));
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0206));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0206));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-96.0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-99.0));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    Ptr<GridPositionAllocator> gridAlloc = CreateObject<GridPositionAllocator>();
    gridAlloc->SetMinX(0.0);
    gridAlloc->SetMinY(0.0);
    gridAlloc->SetDeltaX(gridDistance);
    gridAlloc->SetDeltaY(gridDistance);
    gridAlloc->SetGridWidth(gridSizeX);
    gridAlloc->SetLayoutType(GridPositionAllocator::ROW_MAJOR);
    mobility.SetPositionAllocator(gridAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(olsr, 10);
    internet.SetRoutingHelper(listRouting);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    InetSocketAddress destAddr(interfaces.GetAddress(sinkNode), 9);
    Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(sourceNode), tid);
    sourceSocket->Connect(destAddr);

    auto sinkSocket = Socket::CreateSocket(nodes.Get(sinkNode), tid);
    sinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9));
    sinkSocket->SetRecvCallback(MakeCallback([](Ptr<Socket> socket) {
        Ptr<Packet> packet;
        while ((packet = socket->Recv())) {
            NS_LOG_INFO("Received packet of size " << packet->GetSize());
        }
    }));

    Simulator::Schedule(Seconds(1.0), [sourceSocket, packetSize, numPackets]() {
        for (uint32_t i = 0; i < numPackets; ++i) {
            Ptr<Packet> packet = Create<Packet>(packetSize);
            sourceSocket->Send(packet);
        }
    });

    if (enablePcap) {
        wifiPhy.EnablePcapAll("grid_adhoc_simulation");
    }

    if (enableAscii) {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("grid_adhoc_simulation.tr"));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}