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
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack"); // Use IEEE 802.11s mesh stack
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);

    NetDeviceContainer meshDevices = mesh.Install(phy, nodes);

    InternetStackHelper internetStack;
    internetStack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // UDP Server on Node 2 (port 8000)
    uint16_t port = 8000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(2)); // Node 3 acts as the final receiver
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on Node 1, destined to Node 3 (via Node 2)
    UdpClientHelper udpClient(interfaces.GetAddress(2), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    phy.EnablePcapAll("mesh-example");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}