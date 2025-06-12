#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: server and IoT devices
    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer iotNodes;
    iotNodes.Create(3);

    // Create the 6LoWPAN network
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer serverDevices = lrWpanHelper.Install(serverNode);
    NetDeviceContainer iotDevices = lrWpanHelper.Install(iotNodes);

    SixLowPanHelper sixLowPanHelper;
    NetDeviceContainer server6LowPanDevices = sixLowPanHelper.Install(serverDevices);
    NetDeviceContainer iot6LowPanDevices = sixLowPanHelper.Install(iotDevices);

    // Install the Internet stack
    InternetStackHelper internetv6;
    internetv6.Install(serverNode);
    internetv6.Install(iotNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6AddressHelper;
    ipv6AddressHelper.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer serverInterfaces = ipv6AddressHelper.Assign(server6LowPanDevices);
    Ipv6InterfaceContainer iotInterfaces = ipv6AddressHelper.Assign(iot6LowPanDevices);

    // Set up routing (server is the gateway)
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> serverRouting = ipv6RoutingHelper.GetOrCreateRoutingProtocol(serverNode.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    serverRouting->SetDefaultRoute(serverInterfaces.GetAddress(0,1), 0);

    for (uint32_t i = 0; i < iotNodes.GetN(); ++i) {
        Ptr<Ipv6StaticRouting> iotRouting = ipv6RoutingHelper.GetOrCreateRoutingProtocol(iotNodes.Get(i)->GetObject<Ipv6>()->GetRoutingProtocol());
        iotRouting->SetDefaultRoute(iotInterfaces.GetAddress(i,1), 0);

        //Add route from the iot device to the server
        ipv6RoutingHelper.AddRoute(iotNodes.Get(i)->GetObject<Ipv6>()->GetRoutingProtocol(),
                                 serverInterfaces.GetAddress(0,1),
                                 Ipv6Prefix(64),
                                 serverInterfaces.GetAddress(0,1),
                                 0);
    }

    // Application: UDP Server on the server node
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Application: UDP Clients on the IoT devices
    UdpClientHelper client(serverInterfaces.GetAddress(0,1), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < iotNodes.GetN(); ++i) {
        ApplicationContainer clientApp = client.Install(iotNodes.Get(i));
        clientApps.Add(clientApp);
        clientApp.Start(Seconds(2.0 + i * 0.5)); // stagger start times
        clientApp.Stop(Seconds(9.0));
    }

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}