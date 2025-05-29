#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t nNodes = 4;
    double simulationTime = 10.0;

    NodeContainer meshNodes;
    meshNodes.Create(nNodes);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);

    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(30.0, 0.0, 0.0));
    positionAlloc->Add(Vector(60.0, 0.0, 0.0));
    positionAlloc->Add(Vector(90.0, 0.0, 0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(meshNodes);

    InternetStackHelper internetStack;
    internetStack.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    uint16_t port = 9;

    // UDP Server on last node
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(meshNodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // UDP Clients on first three nodes
    for (uint32_t i = 0; i < 3; ++i)
    {
        UdpClientHelper udpClient(interfaces.GetAddress(3), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = udpClient.Install(meshNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}