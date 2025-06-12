#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaNetworkExample");

int main(int argc, char *argv[])
{
    // Set simulation parameters
    double simulationTime = 10.0; // seconds

    // Create nodes
    NodeContainer csmaNodes;
    csmaNodes.Create(4); // 4 CSMA nodes

    // Create the CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install the CSMA devices
    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(csmaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    // Install TCP application on each node to communicate with node 0 (central server)
    uint16_t port = 9; // TCP port

    // Create a packet sink on the first node (server)
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(csmaNodes.Get(0));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(simulationTime));

    // Create TCP clients on the other nodes
    for (uint32_t i = 1; i < csmaNodes.GetN(); ++i)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(csmaInterfaces.GetAddress(0), port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = onoff.Install(csmaNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    // Enable NetAnim
    AnimationInterface anim("csma_network.xml");
    anim.SetConstantPosition(csmaNodes.Get(0), 10.0, 20.0); // Server
    anim.SetConstantPosition(csmaNodes.Get(1), 20.0, 40.0); // Client 1
    anim.SetConstantPosition(csmaNodes.Get(2), 30.0, 40.0); // Client 2
    anim.SetConstantPosition(csmaNodes.Get(3), 40.0, 40.0); // Client 3

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

