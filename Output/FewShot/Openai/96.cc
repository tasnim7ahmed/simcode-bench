#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/packet.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

static void QueueEnqueueTrace(std::string context, Ptr<const Packet> packet)
{
    static std::ofstream outfile("udp-echo.tr", std::ios_base::app);
    outfile << Simulator::Now().GetSeconds() << " ENQUEUE " << packet->GetUid() << std::endl;
}

static void QueueDequeueTrace(std::string context, Ptr<const Packet> packet)
{
    static std::ofstream outfile("udp-echo.tr", std::ios_base::app);
    outfile << Simulator::Now().GetSeconds() << " DEQUEUE " << packet->GetUid() << std::endl;
}

static void QueueDropTrace(std::string context, Ptr<const Packet> packet)
{
    static std::ofstream outfile("udp-echo.tr", std::ios_base::app);
    outfile << Simulator::Now().GetSeconds() << " DROP " << packet->GetUid() << std::endl;
}

static void RxTrace(std::string context, Ptr<const Packet> packet, const Address& address)
{
    static std::ofstream outfile("udp-echo.tr", std::ios_base::app);
    outfile << Simulator::Now().GetSeconds() << " RX " << packet->GetUid() << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable logging for UdpEcho apps
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Set up CSMA LAN with DropTail queue
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPs
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on node 1
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0 sending to node 1
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up tracing of queue events and packet reception
    Ptr<CsmaNetDevice> csmaDevice;
    Ptr<Queue<Packet>> queue;

    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        csmaDevice = DynamicCast<CsmaNetDevice> (devices.Get(i));
        queue = csmaDevice->GetQueue();

        if (queue)
        {
            queue->TraceConnect("Enqueue", "device[" + std::to_string(i) + "]", MakeBoundCallback(&QueueEnqueueTrace));
            queue->TraceConnect("Dequeue", "device[" + std::to_string(i) + "]", MakeBoundCallback(&QueueDequeueTrace));
            queue->TraceConnect("Drop",    "device[" + std::to_string(i) + "]", MakeBoundCallback(&QueueDropTrace));
        }
        csmaDevice->TraceConnect("MacRx", "device[" + std::to_string(i) + "]", MakeBoundCallback(&RxTrace));
    }

    // Enable pcap tracing
    csma.EnablePcapAll("udp-echo");

    // Run simulation in real time mode
    RealTimeSimulatorImpl realtime;
    Simulator::SetImplementation(&realtime);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}