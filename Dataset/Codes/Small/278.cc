#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time resolution
    Time::SetResolution(Time::NS);

    // Create nodes: one eNodeB and one UE
    NodeContainer enbNode, ueNode;
    enbNode.Create(1);
    ueNode.Create(1);

    // Set up the LTE helper
    LteHelper lteHelper;
    Ptr<NrCellularNetDevice> enbDevice = lteHelper.InstallEnbDevice(enbNode.Get(0));
    Ptr<NrCellularNetDevice> ueDevice = lteHelper.InstallUeDevice(ueNode.Get(0));

    // Install IP stack for routing
    InternetStackHelper internet;
    internet.Install(enbNode);
    internet.Install(ueNode);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface, enbIpIface;
    ueIpIface = ipv4.Assign(ueDevice->GetObject<NetDevice>());
    enbIpIface = ipv4.Assign(enbDevice->GetObject<NetDevice>());

    // Set up the HTTP server on the eNodeB
    uint16_t port = 80;
    UdpServerHelper httpServer(port);
    ApplicationContainer serverApp = httpServer.Install(enbNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the HTTP client on the UE
    UdpClientHelper httpClient(enbIpIface.GetAddress(0), port);
    httpClient.SetAttribute("MaxPackets", UintegerValue(5));
    httpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    httpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = httpClient.Install(ueNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set the mobility model for the eNodeB and UE
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNode);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
