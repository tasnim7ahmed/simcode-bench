#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/csma-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    uint32_t numDevices = 5;
    double simTime = 20.0;

    CommandLine cmd;
    cmd.AddValue("numDevices", "Number of IoT devices", numDevices);
    cmd.AddValue("simTime", "Simulation duration (s)", simTime);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: IoT devices + 1 server
    NodeContainer nodes;
    nodes.Create(numDevices + 1); // Last node is the server

    // Install LrWpan devices
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevs = lrWpanHelper.Install(nodes);

    // Set all devices to the same PAN ID
    lrWpanHelper.AssociateToPan(lrwpanDevs, 0x20);

    // Install 6LoWPAN over the LrWpan devices
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixDevs = sixlowpan.Install(lrwpanDevs);

    // Install Internet stack (IPv6)
    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaces = ipv6.Assign(sixDevs);

    // Bring all interfaces up
    for (uint32_t i = 0; i < ifaces.GetN(); ++i) {
        ifaces.SetForwarding(i, true);
        ifaces.SetDefaultRouteInAllNodes(i);
    }

    // UDP Echo Server on the server node
    uint16_t port = 9876;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numDevices));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo Clients on IoT devices
    for (uint32_t i = 0; i < numDevices; ++i) {
        UdpEchoClientHelper echoClient(ifaces.GetAddress(numDevices, 1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0 + i * 0.1));
        clientApps.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}