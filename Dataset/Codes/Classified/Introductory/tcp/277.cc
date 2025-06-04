#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set the simulation time resolution
    Time::SetResolution(Time::NS);

    // Create nodes: one gNB (evolved NodeB) and one UE (User Equipment)
    NodeContainer gnbNode, ueNode;
    gnbNode.Create(1);
    ueNode.Create(1);

    // Set up the mobility model (static gNB and random UE)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(ueNode);

    // Set up the 5G NR module (gNB and UE)
    NrHelper nrHelper;
    Ptr<NrNetDevice> gnbDevice = nrHelper.Install(gnbNode.Get(0));
    Ptr<NrNetDevice> ueDevice = nrHelper.Install(ueNode.Get(0));

    // Install IP stack for routing
    InternetStackHelper internet;
    internet.Install(gnbNode);
    internet.Install(ueNode);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface, gnbIpIface;
    ueIpIface = ipv4.Assign(ueDevice->GetObject<NetDevice>());
    gnbIpIface = ipv4.Assign(gnbDevice->GetObject<NetDevice>());

    // Set up TCP server on the gNB
    uint16_t port = 9;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(gnbNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on the UE
    TcpClientHelper tcpClient(gnbIpIface.GetAddress(0), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(5));
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = tcpClient.Install(ueNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
