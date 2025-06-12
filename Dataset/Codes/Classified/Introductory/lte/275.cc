#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create the eNB and UE nodes
    NodeContainer eNBNode, ueNode;
    eNBNode.Create(1);
    ueNode.Create(1);

    // Set up the mobility model (eNB is static, UE is moving)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(eNBNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(ueNode);

    // Install LTE stack (eNB and UE)
    LteHelper lteHelper;
    InternetStackHelper internet;
    lteHelper.Install(eNBNode);
    lteHelper.Install(ueNode);

    // Install IP stack for routing
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer ueIpIface, enbIpIface;
    ueIpIface = lteHelper.AssignUeIpv4Address(ueNode.Get(0));
    enbIpIface = lteHelper.AssignUeIpv4Address(eNBNode.Get(0));

    // Create a Simplex UDP server on eNB (receiver)
    UdpServerHelper udpServer(9);
    ApplicationContainer serverApps = udpServer.Install(eNBNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create a Simplex UDP client on UE (sender)
    UdpClientHelper udpClient(enbIpIface.GetAddress(0), 9);
    udpClient.SetAttribute("MaxPackets", UintegerValue(5));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNode.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
