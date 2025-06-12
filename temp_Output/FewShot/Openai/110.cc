#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

std::ofstream traceFile;

void
QueueTrace(std::string context, Ptr<const Packet> packet)
{
    traceFile << Simulator::Now().GetSeconds() << " " << context << " enqueued packet of size " << packet->GetSize() << std::endl;
}

void
PacketRxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
    traceFile << Simulator::Now().GetSeconds() << " " << context << " received packet of size " << packet->GetSize() << std::endl;
}

int main(int argc, char *argv[])
{
    uint32_t numPackets = 5;
    double interval = 1.0;

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create CSMA channel with DropTail queues
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma.SetQueue("ns3::DropTailQueue<Packet>");

    NetDeviceContainer devices = csma.Install(nodes);

    // Install IPv6 stack
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
    // Ensure interfaces are up
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    interfaces.SetForwarding(1, true);
    interfaces.SetDefaultRouteInAllNodes(1);

    // Open trace file
    traceFile.open("ping6.tr", std::ios::out);

    // Queue tracing
    Ptr<CsmaNetDevice> csmaDev0 = DynamicCast<CsmaNetDevice>(devices.Get(0));
    Ptr<CsmaNetDevice> csmaDev1 = DynamicCast<CsmaNetDevice>(devices.Get(1));

    if (csmaDev0 && csmaDev0->GetQueue())
    {
        csmaDev0->GetQueue()->TraceConnect("Enqueue", "n0", MakeCallback(&QueueTrace));
    }
    if (csmaDev1 && csmaDev1->GetQueue())
    {
        csmaDev1->GetQueue()->TraceConnect("Enqueue", "n1", MakeCallback(&QueueTrace));
    }

    // Packet receive tracing at protocol level
    devices.Get(1)->TraceConnect("PhyRxEnd", "n1", MakeCallback(&PacketRxTrace));

    // Install Ping6 application on n0 (to n1)
    uint32_t packetSize = 56;
    Ping6Helper ping6;
    ping6.SetLocal(interfaces.GetAddress(0, 1));
    ping6.SetRemote(interfaces.GetAddress(1, 1));
    ping6.SetAttribute("MaxPackets", UintegerValue(numPackets));
    ping6.SetAttribute("Interval", TimeValue(Seconds(interval)));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
    ping6.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer app = ping6.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(numPackets * interval + 2.0));

    // Additional receiving node packet tracing (if application on n1 responds)
    devices.Get(0)->TraceConnect("PhyRxEnd", "n0", MakeCallback(&PacketRxTrace));

    Simulator::Stop(Seconds(numPackets * interval + 5.0));
    Simulator::Run();

    traceFile.close();

    Simulator::Destroy();
    return 0;
}