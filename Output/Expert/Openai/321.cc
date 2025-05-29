#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(4),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Mesh configuration
    MeshHelper mesh;
    mesh = MeshHelper::Default ();
    mesh.SetStackInstaller ("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType ("RandomStart", TimeValue (Seconds(0.1)));
    mesh.SetNumberOfInterfaces (1);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NetDeviceContainer meshDevices = mesh.Install(phy, meshNodes);

    // Internet stack
    InternetStackHelper internetStack;
    internetStack.Install(meshNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(meshDevices);

    // UDP server on Node 3 (last node)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(meshNodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP clients on Node 0, 1, 2 sending to Node 3
    for (uint32_t i = 0; i < 3; ++i)
    {
        UdpClientHelper client(interfaces.GetAddress(3), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = client.Install(meshNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}