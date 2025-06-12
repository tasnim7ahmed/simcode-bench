#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaTcpExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("CsmaTcpExample", LOG_LEVEL_INFO);

    // Create CSMA nodes
    NodeContainer csmaNodes;
    csmaNodes.Create(4);

    // Set up CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices on nodes
    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(csmaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    // Set up a TCP server on node 0
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(csmaInterfaces.GetAddress(0), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = sinkHelper.Install(csmaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP clients on nodes 1, 2, and 3
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < 4; ++i)
    {
        clientApps.Add(clientHelper.Install(csmaNodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable tracing
    csma.EnablePcap("csma-tcp", csmaDevices.Get(1), true);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
