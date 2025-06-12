#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleCsmaExample");

int main(int argc, char *argv[])
{
    uint32_t nCsmaNodes = 4; // 3 clients + 1 server

    // Create CSMA nodes
    NodeContainer csmaNodes;
    csmaNodes.Create(nCsmaNodes);

    // Set up CSMA network
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices
    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(csmaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    // Set up UDP server on the last node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(csmaNodes.Get(nCsmaNodes - 1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP clients on the first three nodes
    for (uint32_t i = 0; i < nCsmaNodes - 1; ++i)
    {
        UdpEchoClientHelper echoClient(csmaInterfaces.GetAddress(nCsmaNodes - 1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(csmaNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(9.0));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
