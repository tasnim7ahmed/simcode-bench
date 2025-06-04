#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ue-phy-simulator.h"
#include "ns3/gnb-phy-simulator.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time resolution
    Time::SetResolution(Time::NS);

    // Create nodes: one gNodeB and two UEs
    NodeContainer gnbNode, ueNodes;
    gnbNode.Create(1);
    ueNodes.Create(2);

    // Set up the 5G NR Helper
    NrHelper nrHelper;
    Ptr<NrNetDevice> gnbDevice = nrHelper.InstallGnbDevice(gnbNode.Get(0));
    Ptr<NrNetDevice> ueDevice1 = nrHelper.InstallUeDevice(ueNodes.Get(0));
    Ptr<NrNetDevice> ueDevice2 = nrHelper.InstallUeDevice(ueNodes.Get(1));

    // Install IP stack for routing
    InternetStackHelper internet;
    internet.Install(gnbNode);
    internet.Install(ueNodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpIface, ueIpIface;
    gnbIpIface = ipv4.Assign(gnbDevice);
    ueIpIface = ipv4.Assign(ueDevice1);
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    ipv4.Assign(ueDevice2);

    // Set up the TCP server on UE1 (client side)
    uint16_t port = 8080;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the TCP client on UE2 (server side)
    TcpClientHelper tcpClient(gnbIpIface.GetAddress(0), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(10));
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = tcpClient.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set mobility model for gNodeB and UEs
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);

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
