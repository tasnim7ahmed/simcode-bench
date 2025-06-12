#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

void
QueueEnqueueTrace(Ptr<const Packet> packet)
{
    std::ofstream trFile;
    trFile.open("wsn-ping6.tr", std::ios::app);
    trFile << Simulator::Now().GetSeconds() << "\tEnqueue\t" << packet->GetSize() << " bytes\n";
    trFile.close();
}

void
QueueDequeueTrace(Ptr<const Packet> packet)
{
    std::ofstream trFile;
    trFile.open("wsn-ping6.tr", std::ios::app);
    trFile << Simulator::Now().GetSeconds() << "\tDequeue\t" << packet->GetSize() << " bytes\n";
    trFile.close();
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
    std::ofstream trFile;
    trFile.open("wsn-ping6.tr", std::ios::app);
    trFile << Simulator::Now().GetSeconds() << "\tRx\t" << packet->GetSize() << " bytes\n";
    trFile.close();
}

int main(int argc, char *argv[])
{
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO);
    LogComponentEnable("LrWpanPhy", LOG_LEVEL_INFO);

    // Clean trace file at start
    std::ofstream trFileClean("wsn-ping6.tr", std::ios::out);
    trFileClean.close();

    // Create 2 nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Mobility: Set both nodes as stationary
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install IEEE 802.15.4 devices
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devs = lrWpanHelper.Install(nodes);

    // Set short addresses for simulation/identification
    Ptr<LrWpanNetDevice> dev0 = DynamicCast<LrWpanNetDevice>(devs.Get(0));
    Ptr<LrWpanNetDevice> dev1 = DynamicCast<LrWpanNetDevice>(devs.Get(1));
    dev0->SetAddress(Mac16Address("00:01"));
    dev1->SetAddress(Mac16Address("00:02"));

    // Enable LR-WPAN (associate nodes to a PAN)
    lrWpanHelper.AssociateToPan(devs, 0);

    // Set DropTail queue for each device
    for (uint32_t i = 0; i < devs.GetN(); ++i)
    {
        Ptr<Queue<Packet>> queue = CreateObject<DropTailQueue<Packet>>();
        Ptr<NetDeviceQueueInterface> ndi = devs.Get(i)->GetObject<NetDeviceQueueInterface>();
        if (ndi)
        {
            for (uint32_t j = 0; j < ndi->GetNQueue(); ++j)
            {
                ndi->SetQueue(j, queue);
                queue->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueEnqueueTrace));
                queue->TraceConnectWithoutContext("Dequeue", MakeCallback(&QueueDequeueTrace));
            }
        }
    }

    // Install the internet stack and assign IPv6 addresses
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(nodes);

    SixLowPanHelper sixlowpanHelper;
    NetDeviceContainer slpDevs = sixlowpanHelper.Install(devs);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaces = ipv6.Assign(slpDevs);
    // Set interfaces as up
    ifaces.SetForwarding(0, true);
    ifaces.SetForwarding(1, true);
    ifaces.SetDefaultRouteInAllNodes(0);

    // Attach a PacketSink for Rx trace on node 1
    uint16_t sinkPort = 9999;
    Address sinkAddress(Inet6SocketAddress(ifaces.GetAddress(1,1), sinkPort));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    sinkApps.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));

    // Set up Ping6App from node 0 to node 1
    Ping6Helper ping6;
    ping6.SetLocal(ifaces.GetAddress(0,1));
    ping6.SetRemote(ifaces.GetAddress(1,1));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("PacketSize", UintegerValue(32));
    ping6.SetAttribute("MaxPackets", UintegerValue(5));
    ApplicationContainer pingApps = ping6.Install(nodes.Get(0));
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(15.0));

    // Enable flow monitor for more packet-level logging
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    monitor->SerializeToXmlFile("wsn-ping6-flow.xml", true, true);
    Simulator::Destroy();

    return 0;
}