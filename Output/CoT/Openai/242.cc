#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SixLowpanIotSimulation");

int main(int argc, char *argv[])
{
    uint32_t nIoTDevices = 5;
    double simTime = 20.0;
    CommandLine cmd;
    cmd.AddValue("nIoTDevices", "Number of IoT devices", nIoTDevices);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    // Create nodes: IoT devices and server
    NodeContainer iotNodes;
    iotNodes.Create(nIoTDevices);

    NodeContainer serverNode;
    serverNode.Create(1);

    NodeContainer allNodes;
    allNodes.Add(iotNodes);
    allNodes.Add(serverNode);

    // Install mobility (static, in a line)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nIoTDevices; ++i)
    {
        positionAlloc->Add(Vector(10*i, 0.0, 0.0));
    }
    positionAlloc->Add(Vector(10*nIoTDevices, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Install IEEE 802.15.4 devices
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrWpanDevs = lrWpanHelper.Install(allNodes);

    // Assign same PAN ID
    lrWpanHelper.AssociateToPan(lrWpanDevs, 0xABCD);

    // Install 6LoWPAN adaptation layer
    SixLowPanHelper sixlowpanHelper;
    NetDeviceContainer sixlowpanDevs = sixlowpanHelper.Install(lrWpanDevs);

    // Install Internet stack
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iotInterfaces = ipv6.Assign(sixlowpanDevs);
    for (uint32_t i = 0; i < iotInterfaces.GetN(); ++i)
    {
        iotInterfaces.SetForwarding(i, true);
        iotInterfaces.SetDefaultRouteInAllNodes(i);
    }

    // UDP Server on last node (central server)
    uint16_t serverPort = 5683;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(serverNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UdpClient on each IoT node, targeting the server
    uint32_t packetSize = 50;
    Time interPacketInterval = Seconds(2.0);

    for (uint32_t i = 0; i < nIoTDevices; ++i)
    {
        UdpClientHelper udpClient(iotInterfaces.GetAddress(nIoTDevices, 1), serverPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = udpClient.Install(iotNodes.Get(i));
        clientApps.Start(Seconds(1.0 + i * 0.2));
        clientApps.Stop(Seconds(simTime));
    }

    // Enable pcap tracing
    lrWpanHelper.EnablePcapAll(std::string("sixlowpan-iot"));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}