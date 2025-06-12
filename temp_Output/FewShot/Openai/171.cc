#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvPdrE2EDelay");

// Statistics
uint32_t g_rxPackets = 0;
uint32_t g_txPackets = 0;
double g_totalDelay = 0.0;

std::map<uint64_t, Time> g_sentPacketTimes; // id: send time

void TxTrace (Ptr<const Packet> pkt)
{
    ++g_txPackets;
    g_sentPacketTimes[pkt->GetUid()] = Simulator::Now();
}

void RxTrace (Ptr<const Packet> pkt, const Address & addr)
{
    ++g_rxPackets;
    auto it = g_sentPacketTimes.find(pkt->GetUid());
    if (it != g_sentPacketTimes.end())
    {
        g_totalDelay += (Simulator::Now() - it->second).GetSeconds();
    }
}

int main(int argc, char *argv[])
{
    uint32_t nNodes = 10;
    double simulationTime = 30.0;
    double areaSize = 500.0;
    double nodeSpeed = 5.0; // m/s

    // Enable pcap
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NodeContainer nodes;
    nodes.Create(nNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", 
                                 PointerValue(CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"))));
    mobility.Install(nodes);

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Internet stack and AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP traffic: random source/dest, random start times
    uint16_t port = 8080;
    uint32_t payloadSize = 512;
    uint32_t packetRate = 5; // packets per second per flow

    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    // Install a UDP server on every node
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        UdpServerHelper server(port);
        ApplicationContainer apps = server.Install(nodes.Get(i));
        apps.Start(Seconds(0.1));
        apps.Stop(Seconds(simulationTime));
        serverApps.Add(apps);
    }

    // For 5 random flows, pick random src/dst, random start time, install UDP client
    for (uint32_t flow = 0; flow < 5; ++flow)
    {
        uint32_t src = uv->GetInteger(0, nNodes - 1);
        uint32_t dst = uv->GetInteger(0, nNodes - 1);
        while (dst == src)
            dst = uv->GetInteger(0, nNodes - 1);

        double startTime = uv->GetValue(1.0, 10.0);
        double stopTime = simulationTime - 1.0;

        UdpClientHelper client(interfaces.GetAddress(dst), port);
        client.SetAttribute("MaxPackets", UintegerValue(packetRate * (stopTime - startTime)));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0 / packetRate)));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer apps = client.Install(nodes.Get(src));
        apps.Start(Seconds(startTime));
        apps.Stop(Seconds(stopTime));
        clientApps.Add(apps);
    }

    // Connect packet send/receive tracing
    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&TxTrace));
    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

    // Enable pcap on every device
    wifiPhy.EnablePcap("adhoc-aodv", devices, true);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    double pdr = 0.0;
    double avgDelay = 0.0;
    if (g_txPackets > 0)
    {
        pdr = 100.0 * g_rxPackets / static_cast<double>(g_txPackets);
        avgDelay = g_rxPackets ? (g_totalDelay / g_rxPackets) : 0.0;
    }

    std::cout << "Simulation Results:\n";
    std::cout << "  Transmitted packets: " << g_txPackets << std::endl;
    std::cout << "  Received packets:    " << g_rxPackets << std::endl;
    std::cout << "  Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "  Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}