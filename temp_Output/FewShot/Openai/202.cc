#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"
#include <cmath>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnCircleUdpSimulation");

class WsnStats
{
public:
    WsnStats(): m_sent(0), m_received(0), m_firstSent(Seconds(0)), m_lastReceived(Seconds(0)), m_totalDelay(Seconds(0)) {}

    void PacketSent(Ptr<const Packet> pkt)
    {
        if (m_sent == 0)
            m_firstSent = Simulator::Now();
        m_sent++;
    }

    void PacketReceived(Ptr<const Packet> pkt, const Address &from)
    {
        m_received++;
        m_lastReceived = Simulator::Now();
        m_totalDelay += (Simulator::Now() - m_sendTimes[pkt->GetUid()]);
    }

    void LogSendTime(uint64_t uid, Time when)
    {
        m_sendTimes[uid] = when;
    }

    void LogStats(double simulationTime, uint32_t pktSize, std::string csvFile)
    {
        double pdr = (m_sent == 0) ? 0 : double(m_received) / double(m_sent);
        double throughput = (m_lastReceived.GetSeconds() > 0)
            ? ((m_received * pktSize * 8.0) / (simulationTime * 1e6)) // Mbps
            : 0;
        double avgDelay = (m_received == 0) ? 0 : m_totalDelay.GetSeconds() / m_received;

        std::ofstream out(csvFile, std::ios::app);
        out << "Packet Delivery Ratio:," << pdr << std::endl;
        out << "Average End-to-End Delay (s):," << avgDelay << std::endl;
        out << "Throughput (Mbps):," << throughput << std::endl;
        out.close();
    }

    uint32_t m_sent;
    uint32_t m_received;
    Time m_firstSent;
    Time m_lastReceived;
    Time m_totalDelay;
    std::map<uint64_t, Time> m_sendTimes;
};

// Global stats object
WsnStats g_stats;

void TxTrace(Ptr<const Packet> pkt)
{
    g_stats.PacketSent(pkt);
    g_stats.LogSendTime(pkt->GetUid(), Simulator::Now());
}

void RxTrace(Ptr<const Packet> pkt, const Address &from)
{
    g_stats.PacketReceived(pkt, from);
}

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    double radius = 20.0;
    double simulationTime = 30.0;    // seconds
    uint32_t packetSize = 64;        // bytes
    double interval = 2.0;           // seconds
    uint16_t port = 4000;

    // Logging
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Mobility: Place nodes in a circle
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i=0; i<numNodes; ++i)
    {
        double angle = 2.0 * M_PI * double(i) / numNodes;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        posAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install LrWpan (IEEE 802.15.4)
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);
    lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

    // Set PHY frequency to 2.4GHz (IEEE 802.15.4 Channel 26)
    for (uint32_t i=0; i<lrwpanDevices.GetN(); ++i)
    {
        Ptr<LrWpanNetDevice> dev = DynamicCast<LrWpanNetDevice>(lrwpanDevices.Get(i));
        dev->GetPhy()->SetChannelNumber(26);
    }

    // Install Internet stack over 6LoWPAN
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrwpanDevices);

    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ipv6if = ipv6.Assign(sixlowpanDevices);

    // Set up UDP applications: Node 0 is sink, others send
    // The sink will listen on port 'port' on node 0 (index 0)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    ApplicationContainer clientApps;

    for (uint32_t i=1; i<numNodes; ++i)
    {
        UdpClientHelper udpClient(ipv6if.GetAddress(0, 1), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(uint32_t(simulationTime / interval)));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer app = udpClient.Install(nodes.Get(i));
        app.Start(Seconds(2.0 + ((i-1)*0.2))); // stagger starts a bit
        app.Stop(Seconds(simulationTime));
        clientApps.Add(app);
    }

    // Connect trace sources to gather statistics (Tx at clients, Rx at server)
    Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&TxTrace));
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&RxTrace));

    // NetAnim setup
    AnimationInterface anim("wsn-circular.xml");
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        anim.UpdateNodeDescription(i, (i==0) ? "Sink" : "Node"+std::to_string(i));
        anim.UpdateNodeColor(i, (i==0)? 0 : 80, (i==0)? 200 : 80, 255);
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // Record statistics
    g_stats.LogStats(simulationTime, packetSize, "wsn_stats.csv");

    Simulator::Destroy();
    return 0;
}