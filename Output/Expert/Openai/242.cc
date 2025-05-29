#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SixLowpanIotUdpExample");

int main(int argc, char *argv[])
{
    uint32_t numDevices = 5;
    double simTime = 20.0;
    double interval = 2.0;

    CommandLine cmd;
    cmd.AddValue("numDevices", "Number of IoT devices", numDevices);
    cmd.AddValue("simTime", "Total simulation time [s]", simTime);
    cmd.AddValue("interval", "UDP packet interval [s]", interval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numDevices + 1); // numDevices: IoT devices, +1: server

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numDevices; ++i) {
        posAlloc->Add(Vector(5.0 * i, 0, 0));
    }
    posAlloc->Add(Vector(2.5 * numDevices, 10.0, 0)); // Server position
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrwpanDevs = lrWpanHelper.Install(nodes);
    lrWpanHelper.AssociateToPan(lrwpanDevs, 0);

    SixLowPanHelper sixlowpanHelper;
    NetDeviceContainer sixlowpanDevs = sixlowpanHelper.Install(lrwpanDevs);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixlowpanDevs);
    interfaces.SetForwarding(numDevices, true);
    interfaces.SetDefaultRouteInAllNodes(numDevices);

    uint16_t serverPort = 5683;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(numDevices));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    for (uint32_t i = 0; i < numDevices; ++i) {
        UdpClientHelper udpClient(interfaces.GetAddress(numDevices, 1), serverPort);
        udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
        udpClient.SetAttribute("MaxPackets", UintegerValue(uint32_t(simTime / interval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(40));
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0 + 0.1 * i));
        clientApp.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}