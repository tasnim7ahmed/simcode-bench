#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/propagation-module.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(10);

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", StringValue("100x100"));
    mobility.Install(nodes);

    // Set up LR-WPAN
    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetPhyAttribute("Pcap", BooleanValue(false));
    lrWpanHelper.SetMacAttribute("SlottedCsmaCa", BooleanValue(false));

    // Set up network
    NetDeviceContainer devices = lrWpanHelper.Install(nodes);

    // Install the stack
    SixLowPanHelper sixLowPanHelper;
    InternetStackHelper stack;
    stack.Install(nodes);
    NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install(devices);

    // Assign addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixLowPanDevices);

    // Set up coordinator
    lrWpanHelper.Create<ns3::SimpleChannel>(nodes.Get(0), 0);
    interfaces.GetAddress(0, 1);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInArea(0, true);

    // Set up UDP server on coordinator
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP clients on end devices
    UdpClientHelper client(interfaces.GetAddress(0, 1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    client.SetAttribute("PacketSize", UintegerValue(128));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    lrWpanHelper.EnablePcapAll("zigbee");

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}