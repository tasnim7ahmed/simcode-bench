#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time resolution
    Time::SetResolution(Time::NS);

    // Create nodes: one eNodeB and three UEs
    NodeContainer eNodeBNode, ueNodes;
    eNodeBNode.Create(1);
    ueNodes.Create(3);

    // Set up LTE helper
    LteHelper lteHelper;
    Ptr<LteEnbNetDevice> enbDevice = lteHelper.InstallEnbDevice(eNodeBNode.Get(0));
    Ptr<LteUeNetDevice> ueDevice1 = lteHelper.InstallUeDevice(ueNodes.Get(0));
    Ptr<LteUeNetDevice> ueDevice2 = lteHelper.InstallUeDevice(ueNodes.Get(1));
    Ptr<LteUeNetDevice> ueDevice3 = lteHelper.InstallUeDevice(ueNodes.Get(2));

    // Install IP stack for routing
    InternetStackHelper internet;
    internet.Install(eNodeBNode);
    internet.Install(ueNodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface, ueIpIface;
    enbIpIface = ipv4.Assign(enbDevice);
    ueIpIface = ipv4.Assign(ueDevice1);
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ipv4.Assign(ueDevice2);
    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    ipv4.Assign(ueDevice3);

    // Set up the UDP server on eNodeB
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(eNodeBNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP clients on the UEs
    UdpClientHelper udpClient(enbIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    clientApps.Add(udpClient.Install(ueNodes.Get(0)));
    clientApps.Add(udpClient.Install(ueNodes.Get(1)));
    clientApps.Add(udpClient.Install(ueNodes.Get(2)));

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set mobility model for eNodeB and UEs
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(eNodeBNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
