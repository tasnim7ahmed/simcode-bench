#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteMobileNetworkExample");

int main(int argc, char *argv[])
{
    // Create nodes
    NodeContainer eNodeB, ue;
    eNodeB.Create(1);
    ue.Create(1);

    // Install LTE model
    LteHelper lte;
    InternetStackHelper internet;

    // Install LTE devices on the nodes
    NetDeviceContainer enbLteDevs, ueLteDevs;
    enbLteDevs = lte.InstallEnbDevice(eNodeB);
    ueLteDevs = lte.InstallUeDevice(ue);

    // Install Internet stack
    internet.Install(eNodeB);
    internet.Install(ue);

    // Set up mobility model for nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(ue);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(eNodeB);

    // Assign IP addresses to devices
    Ipv4AddressHelper ipv4Address;
    ipv4Address.SetBase("7.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpIface = ipv4Address.Assign(ueLteDevs);
    Ipv4InterfaceContainer enbIpIface = ipv4Address.Assign(enbLteDevs);

    // Set up UDP client application on Node 2 (UE)
    uint16_t port = 1234;
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(eNodeB.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Set up UDP server application on Node 1 (eNB)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ue.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
