/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Wireless Sensor Network (WSN) circular topology simulation with ns-3
 * - 6 nodes in a circle
 * - IEEE 802.15.4 (2.4 GHz)
 * - UDP app: 5 nodes send to central sink
 * - Metrics: PDR, e2e delay, throughput
 * - NetAnim visualization
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/sixlowpan-helper.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CircularWsnExample");

// Global statistics
uint32_t g_rxPackets = 0;
uint32_t g_txPackets = 0;
uint64_t g_totalDelayMicroseconds = 0;
std::map<uint32_t, Time> g_sentPacketTimes;

void TxTrace(Ptr<const Packet> packet)
{
    g_txPackets++;
    // Mark send time for delay computation
    uint64_t uid = packet->GetUid();
    g_sentPacketTimes[uid] = Simulator::Now();
}

void RxTrace(Ptr<const Packet> packet, const Address&, const Address&)
{
    g_rxPackets++;
    uint64_t uid = packet->GetUid();
    auto it = g_sentPacketTimes.find(uid);
    if (it != g_sentPacketTimes.end())
    {
        Time delay = Simulator::Now() - it->second;
        g_totalDelayMicroseconds += delay.GetMicroSeconds();
        g_sentPacketTimes.erase(it);
    }
}

int main(int argc, char *argv[])
{
    /* Configurable parameters */
    uint32_t numNodes = 6;
    double radius = 30.0; // meters
    double simTime = 20.0; // seconds
    double appStart = 1.0; // seconds
    double appStop = simTime;
    double interPacketInterval = 1.0; // seconds
    uint32_t packetSize = 50; // bytes
    std::string dataRate = "20kbps";

    // Log
    LogComponentEnable("CircularWsnExample", LOG_LEVEL_INFO);

    // 1. Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 2. Mobility: Circular topology
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        double angle = 2 * M_PI * i / numNodes;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 3. IEEE 802.15.4 device install (2.4GHz)
    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetChannel(CreateObject<LrWpanChannel>());
    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // 2.4GHz PHY
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<LrWpanNetDevice> dev = DynamicCast<LrWpanNetDevice>(lrwpanDevices.Get(i));
        dev->GetPhy()->SetChannelNumber(26); // Channel 26 is in 2.4 GHz band
    }

    // Assign short addresses
    lrWpanHelper.AssociateToPan(lrwpanDevices, 0);

    // 4. 6LoWPAN adaptation
    SixLowPanHelper sixlowpanHelper;
    NetDeviceContainer sixlowpanDevices = sixlowpanHelper.Install(lrwpanDevices);

    // 5. Internet stack (UDP app needs IP)
    InternetStackHelper internetStack;
    internetStack.Install(nodes);

    // 6. IPv4 addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(sixlowpanDevices);

    // 7. UDP app: five sensor nodes send to sink (node 0)
    uint16_t sinkPort = 4000;
    Ipv4Address sinkAddr = interfaces.GetAddress(0);

    // --- Sink Application on node 0 ---
    UdpServerHelper udpServer(sinkPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(appStart));
    serverApp.Stop(Seconds(appStop));

    // --- Sensor/Client Applications ---
    ApplicationContainer clientApps;
    for (uint32_t n = 1; n < numNodes; ++n)
    {
        UdpClientHelper udpClient(sinkAddr, sinkPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(((uint32_t)((appStop-appStart)/interPacketInterval))));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer app = udpClient.Install(nodes.Get(n));
        app.Start(Seconds(appStart));
        app.Stop(Seconds(appStop));
        clientApps.Add(app);
    }

    // 8. Packet tracing for metrics
    // At the sender (UDP Client), can trace at MacTx for better UID match
    for (uint32_t n = 1; n < numNodes; ++n)
    {
        Ptr<NetDevice> dev = lrwpanDevices.Get(n);
        dev->TraceConnectWithoutContext("MacTx", MakeCallback(&TxTrace));
    }
    // At sink receiver, trace at MacRx or use application receive
    Ptr<NetDevice> sinkDev = lrwpanDevices.Get(0);
    sinkDev->TraceConnectWithoutContext("MacRx", MakeCallback(&RxTrace));

    // 9. NetAnim
    AnimationInterface anim("wsn-circular.xml");
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        std::ostringstream oss;
        if (i == 0)
            oss << "Sink";
        else
            oss << "Sensor " << i;
        anim.UpdateNodeDescription(i, oss.str());
        anim.UpdateNodeColor(i, (i == 0 ? 255 : 0), (i == 0 ? 0 : 180), (i == 0 ? 0 : 255));
    }
    anim.EnablePacketMetadata(true);

    // 10. Schedule a statistics summary at simulation end
    Simulator::Schedule(Seconds(simTime), []()
    {
        double pdr = (g_txPackets > 0) ? (100.0 * g_rxPackets / g_txPackets) : 0.0;
        double avgDelay = (g_rxPackets > 0) ? (g_totalDelayMicroseconds / 1e6 / g_rxPackets) : 0.0; // seconds
        double throughput = (g_rxPackets * 50 * 8) / 1e6 / 20.0; // Mbit/s, assuming 50B packet, 20s duration

        std::cout << "\n=== Simulation Result ===\n";
        std::cout << "Packets sent:     " << g_txPackets << "\n";
        std::cout << "Packets received: " << g_rxPackets << "\n";
        std::cout << "Packet Delivery Ratio (PDR): " << pdr << "%\n";
        std::cout << "Average end-to-end delay:    " << avgDelay << " s" << std::endl;
        std::cout << "Throughput:                  " << throughput << " Mbit/s\n";
    });

    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}